#include <stdlib.h>
#include <string.h>
#include "verifier.h"
#include "tseytin.h"

static char *duplicate_string(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    return copy;
}

void verifier_free_cec_result(cec_result_t *res) {
    if (!res) return;
    if (res->input_names) {
        for (size_t i = 0; i < res->num_inputs; ++i) {
            free(res->input_names[i]);
        }
        free(res->input_names);
        res->input_names = NULL;
    }
    if (res->counterexample_inputs) {
        free(res->counterexample_inputs);
        res->counterexample_inputs = NULL;
    }
}

void verifier_free_atpg_result(atpg_result_t *res) {
    if (!res) return;
    if (res->input_names) {
        for (size_t i = 0; i < res->num_inputs; ++i) {
            free(res->input_names[i]);
        }
        free(res->input_names);
        res->input_names = NULL;
    }
    if (res->test_vector) {
        free(res->test_vector);
        res->test_vector = NULL;
    }
}

logisat_status_t verifier_check_equivalence(const netlist_t *nl1,
                                            const netlist_t *nl2,
                                            const solver_config_t *config,
                                            cec_result_t *out_result) {
    if (!nl1 || !nl2 || !out_result) return LOGISAT_ERR_NULL_PTR;
    memset(out_result, 0, sizeof(cec_result_t));

    /* Interface validation: must have equal input and output counts */
    if (nl1->num_primary_inputs != nl2->num_primary_inputs ||
        nl1->num_primary_outputs != nl2->num_primary_outputs) {
        return LOGISAT_ERR_NETLIST_INCOMPATIBLE;
    }

    size_t num_inputs = nl1->num_primary_inputs;
    size_t num_outputs = nl1->num_primary_outputs;

    /* Verify inputs have matching names */
    for (size_t i = 0; i < num_inputs; ++i) {
        const char *name1 = netlist_get_net_name(nl1, nl1->primary_inputs[i]);
        int32_t id2 = 0;
        if (netlist_find_net(nl2, name1, &id2) != LOGISAT_OK) {
            return LOGISAT_ERR_NETLIST_INCOMPATIBLE;
        }
    }

    /* Build combined CNF formula */
    cnf_formula_t *formula = cnf_formula_create(64, 128);
    if (!formula) return LOGISAT_ERR_OUT_OF_MEMORY;

    /* Variable allocation:
       1..num_inputs: Shared Primary Inputs */
    int32_t current_var = (int32_t)num_inputs;

    /* Map netlist 1 nets to CNF vars */
    int32_t *map1 = (int32_t *)calloc(nl1->num_nets + 1, sizeof(int32_t));
    if (!map1) {
        cnf_formula_destroy(formula);
        return LOGISAT_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < num_inputs; ++i) {
        map1[nl1->primary_inputs[i]] = (int32_t)(i + 1);
    }
    for (size_t i = 1; i <= nl1->num_nets; ++i) {
        if (map1[i] == 0) {
            map1[i] = ++current_var;
        }
    }

    /* Map netlist 2 nets to CNF vars */
    int32_t *map2 = (int32_t *)calloc(nl2->num_nets + 1, sizeof(int32_t));
    if (!map2) {
        free(map1);
        cnf_formula_destroy(formula);
        return LOGISAT_ERR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < num_inputs; ++i) {
        const char *name = netlist_get_net_name(nl1, nl1->primary_inputs[i]);
        int32_t id2 = 0;
        netlist_find_net(nl2, name, &id2);
        map2[id2] = (int32_t)(i + 1);
    }
    for (size_t i = 1; i <= nl2->num_nets; ++i) {
        if (map2[i] == 0) {
            map2[i] = ++current_var;
        }
    }

    /* Encode both netlists into formula */
    tseytin_encode_netlist(nl1, formula, map1);
    tseytin_encode_netlist(nl2, formula, map2);

    /* Construct Miter Comparator:
       For each output pair i: diff_var_i = out1_i XOR out2_i */
    int32_t *diff_vars = (int32_t *)malloc(sizeof(int32_t) * num_outputs);
    for (size_t i = 0; i < num_outputs; ++i) {
        int32_t out1 = map1[nl1->primary_outputs[i]];
        int32_t out2 = map2[nl2->primary_outputs[i]];
        int32_t diff = ++current_var;
        diff_vars[i] = diff;

        int32_t in_pair[2] = { out1, out2 };
        tseytin_encode_gate(GATE_XOR, diff, in_pair, 2, formula);
    }

    /* Top-level OR gate: miter_out = diff_0 OR diff_1 OR ... OR diff_{m-1} */
    int32_t miter_out = ++current_var;
    tseytin_encode_gate(GATE_OR, miter_out, diff_vars, num_outputs, formula);
    free(diff_vars);

    /* Assert miter_out must be TRUE (unit clause {miter_out}) */
    int32_t unit_assert[1] = { miter_out };
    cnf_formula_add_clause_lits(formula, unit_assert, 1);
    cnf_formula_update_var_count(formula);

    /* Run Solver */
    var_value_t *assignments = (var_value_t *)calloc(formula->num_vars + 1, sizeof(var_value_t));
    solver_stats_t stats;
    solver_result_t res = solver_solve(formula, config, assignments, &stats);

    out_result->solved = true;
    out_result->stats = stats;

    if (res == SOLVER_UNSAT) {
        out_result->is_equivalent = true;
    } else if (res == SOLVER_SAT) {
        out_result->is_equivalent = false;
        out_result->num_inputs = num_inputs;
        out_result->input_names = (char **)malloc(sizeof(char *) * num_inputs);
        out_result->counterexample_inputs = (var_value_t *)malloc(sizeof(var_value_t) * num_inputs);

        for (size_t i = 0; i < num_inputs; ++i) {
            const char *iname = netlist_get_net_name(nl1, nl1->primary_inputs[i]);
            out_result->input_names[i] = duplicate_string(iname);
            out_result->counterexample_inputs[i] = assignments[i + 1];
        }
    }

    free(map1);
    free(map2);
    free(assignments);
    cnf_formula_destroy(formula);

    return LOGISAT_OK;
}

logisat_status_t verifier_generate_test_pattern(const netlist_t *nl,
                                                const char *target_net,
                                                fault_type_t fault_type,
                                                const solver_config_t *config,
                                                atpg_result_t *out_result) {
    if (!nl || !target_net || !out_result) return LOGISAT_ERR_NULL_PTR;
    memset(out_result, 0, sizeof(atpg_result_t));

    if (fault_type == FAULT_NONE) return LOGISAT_OK;

    int32_t target_id = 0;
    if (netlist_find_net(nl, target_net, &target_id) != LOGISAT_OK) {
        return LOGISAT_ERR_INVALID_VARIABLE;
    }

    size_t num_inputs = nl->num_primary_inputs;
    size_t num_outputs = nl->num_primary_outputs;

    cnf_formula_t *formula = cnf_formula_create(64, 128);
    if (!formula) return LOGISAT_ERR_OUT_OF_MEMORY;

    int32_t current_var = (int32_t)num_inputs;

    /* Map good circuit */
    int32_t *map_good = (int32_t *)calloc(nl->num_nets + 1, sizeof(int32_t));
    for (size_t i = 0; i < num_inputs; ++i) {
        map_good[nl->primary_inputs[i]] = (int32_t)(i + 1);
    }
    for (size_t i = 1; i <= nl->num_nets; ++i) {
        if (map_good[i] == 0) map_good[i] = ++current_var;
    }

    /* Map faulty circuit: if target_id is a primary input, sever it from the external pin */
    int32_t *map_faulty = (int32_t *)calloc(nl->num_nets + 1, sizeof(int32_t));
    for (size_t i = 0; i < num_inputs; ++i) {
        int32_t net_id = nl->primary_inputs[i];
        if (net_id == target_id) {
            map_faulty[net_id] = ++current_var;
        } else {
            map_faulty[net_id] = (int32_t)(i + 1);
        }
    }
    for (size_t i = 1; i <= nl->num_nets; ++i) {
        if (map_faulty[i] == 0) map_faulty[i] = ++current_var;
    }

    /* Encode good circuit */
    tseytin_encode_netlist(nl, formula, map_good);

    /* Encode faulty circuit with target net replaced/tied */
    for (size_t i = 0; i < nl->num_gates; ++i) {
        const circuit_gate_t *g = &nl->gates[i];
        if (g->output_net == target_id) {
            /* Skip normal driving gate for the faulty net; it is driven by fault condition! */
            continue;
        }

        int32_t out_var = map_faulty[g->output_net];
        int32_t in_vars[32];
        for (size_t j = 0; j < g->num_inputs; ++j) {
            in_vars[j] = map_faulty[g->input_nets[j]];
        }
        tseytin_encode_gate(g->type, out_var, in_vars, g->num_inputs, formula);
    }

    /* Tie faulty net to constant */
    int32_t faulty_target_var = map_faulty[target_id];
    int32_t tie_clause[1];
    tie_clause[0] = (fault_type == FAULT_STUCK_AT_0) ? -faulty_target_var : faulty_target_var;
    cnf_formula_add_clause_lits(formula, tie_clause, 1);

    /* Output XOR miter */
    int32_t *diff_vars = (int32_t *)malloc(sizeof(int32_t) * num_outputs);
    for (size_t i = 0; i < num_outputs; ++i) {
        int32_t g_out = map_good[nl->primary_outputs[i]];
        int32_t f_out = map_faulty[nl->primary_outputs[i]];
        int32_t diff = ++current_var;
        diff_vars[i] = diff;

        int32_t in_pair[2] = { g_out, f_out };
        tseytin_encode_gate(GATE_XOR, diff, in_pair, 2, formula);
    }

    int32_t miter_out = ++current_var;
    tseytin_encode_gate(GATE_OR, miter_out, diff_vars, num_outputs, formula);
    free(diff_vars);

    int32_t unit_assert[1] = { miter_out };
    cnf_formula_add_clause_lits(formula, unit_assert, 1);
    cnf_formula_update_var_count(formula);

    var_value_t *assignments = (var_value_t *)calloc(formula->num_vars + 1, sizeof(var_value_t));
    solver_stats_t stats;
    solver_result_t res = solver_solve(formula, config, assignments, &stats);

    out_result->solved = true;
    out_result->stats = stats;

    if (res == SOLVER_SAT) {
        out_result->is_testable = true;
        out_result->num_inputs = num_inputs;
        out_result->input_names = (char **)malloc(sizeof(char *) * num_inputs);
        out_result->test_vector = (var_value_t *)malloc(sizeof(var_value_t) * num_inputs);

        for (size_t i = 0; i < num_inputs; ++i) {
            const char *iname = netlist_get_net_name(nl, nl->primary_inputs[i]);
            out_result->input_names[i] = duplicate_string(iname);
            out_result->test_vector[i] = assignments[i + 1];
        }
    } else {
        out_result->is_testable = false;
    }

    free(map_good);
    free(map_faulty);
    free(assignments);
    cnf_formula_destroy(formula);

    return LOGISAT_OK;
}
