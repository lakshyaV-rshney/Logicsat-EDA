/**
 * ==============================================================================
 * LogiSAT Formal Verification Engine: CEC Miter & ATPG (Compact Edition)
 * ==============================================================================
 */

#include <stdlib.h>
#include <string.h>
#include "verifier.h"
#include "tseytin.h"

static char *dup_str(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *c = (char *)malloc(len + 1);
    if (c) memcpy(c, s, len + 1);
    return c;
}

void verifier_free_cec_result(cec_result_t *res) {
    if (!res) return;
    if (res->input_names) {
        for (size_t i = 0; i < res->num_inputs; ++i) free(res->input_names[i]);
        free(res->input_names);
    }
    free(res->counterexample_inputs);
    memset(res, 0, sizeof(*res));
}

void verifier_free_atpg_result(atpg_result_t *res) {
    if (!res) return;
    if (res->input_names) {
        for (size_t i = 0; i < res->num_inputs; ++i) free(res->input_names[i]);
        free(res->input_names);
    }
    free(res->test_vector);
    memset(res, 0, sizeof(*res));
}

/* Helper to attach XOR outputs, top-level OR miter, and run DPLL solver */
static solver_result_t solve_miter(cnf_formula_t *formula, const int32_t *out1_vars, const int32_t *out2_vars,
                                   size_t num_outputs, int32_t *cur_var, const solver_config_t *cfg,
                                   var_value_t **out_assign, solver_stats_t *out_stats) {
    int32_t *diff_vars = (int32_t *)malloc(sizeof(int32_t) * (num_outputs > 0 ? num_outputs : 1));
    for (size_t i = 0; i < num_outputs; ++i) {
        diff_vars[i] = ++(*cur_var);
        int32_t pair[2] = { out1_vars[i], out2_vars[i] };
        tseytin_encode_gate(GATE_XOR, diff_vars[i], pair, 2, formula);
    }

    int32_t miter_out = ++(*cur_var);
    tseytin_encode_gate(GATE_OR, miter_out, diff_vars, num_outputs, formula);
    free(diff_vars);

    int32_t unit[1] = { miter_out };
    cnf_formula_add_clause_lits(formula, unit, 1);
    cnf_formula_update_var_count(formula);

    *out_assign = (var_value_t *)calloc(formula->num_vars + 1, sizeof(var_value_t));
    return solver_solve(formula, cfg, *out_assign, out_stats);
}

logisat_status_t verifier_check_equivalence(const netlist_t *nl1, const netlist_t *nl2,
                                            const solver_config_t *config, cec_result_t *out_res) {
    if (!nl1 || !nl2 || !out_res) return LOGISAT_ERR_NULL_PTR;
    memset(out_res, 0, sizeof(*out_res));

    size_t n_in = nl1->num_primary_inputs, n_out = nl1->num_primary_outputs;
    if (n_in != nl2->num_primary_inputs || n_out != nl2->num_primary_outputs) {
        return LOGISAT_ERR_NETLIST_INCOMPATIBLE;
    }

    for (size_t i = 0; i < n_in; ++i) {
        int32_t id2 = 0;
        if (netlist_find_net(nl2, netlist_get_net_name(nl1, nl1->primary_inputs[i]), &id2) != LOGISAT_OK) {
            return LOGISAT_ERR_NETLIST_INCOMPATIBLE;
        }
    }

    cnf_formula_t *formula = cnf_formula_create(64, 128);
    int32_t cur_var = (int32_t)n_in;

    int32_t *map1 = (int32_t *)calloc(nl1->num_nets + 1, sizeof(int32_t));
    int32_t *map2 = (int32_t *)calloc(nl2->num_nets + 1, sizeof(int32_t));
    for (size_t i = 0; i < n_in; ++i) {
        map1[nl1->primary_inputs[i]] = (int32_t)(i + 1);
        int32_t id2 = 0;
        netlist_find_net(nl2, netlist_get_net_name(nl1, nl1->primary_inputs[i]), &id2);
        map2[id2] = (int32_t)(i + 1);
    }
    for (size_t i = 1; i <= nl1->num_nets; ++i) if (!map1[i]) map1[i] = ++cur_var;
    for (size_t i = 1; i <= nl2->num_nets; ++i) if (!map2[i]) map2[i] = ++cur_var;

    tseytin_encode_netlist(nl1, formula, map1);
    tseytin_encode_netlist(nl2, formula, map2);

    int32_t *out1 = (int32_t *)malloc(sizeof(int32_t) * n_out);
    int32_t *out2 = (int32_t *)malloc(sizeof(int32_t) * n_out);
    for (size_t i = 0; i < n_out; ++i) {
        out1[i] = map1[nl1->primary_outputs[i]];
        out2[i] = map2[nl2->primary_outputs[i]];
    }

    var_value_t *assign = NULL;
    solver_stats_t stats;
    solver_result_t res = solve_miter(formula, out1, out2, n_out, &cur_var, config, &assign, &stats);
    free(out1); free(out2);

    out_res->solved = true;
    out_res->stats = stats;
    out_res->is_equivalent = (res == SOLVER_UNSAT);

    if (res == SOLVER_SAT) {
        out_res->num_inputs = n_in;
        out_res->input_names = (char **)malloc(sizeof(char *) * n_in);
        out_res->counterexample_inputs = (var_value_t *)malloc(sizeof(var_value_t) * n_in);
        for (size_t i = 0; i < n_in; ++i) {
            out_res->input_names[i] = dup_str(netlist_get_net_name(nl1, nl1->primary_inputs[i]));
            out_res->counterexample_inputs[i] = assign[i + 1];
        }
    }

    free(map1); free(map2); free(assign);
    cnf_formula_destroy(formula);
    return LOGISAT_OK;
}

logisat_status_t verifier_generate_test_pattern(const netlist_t *nl, const char *target_net,
                                                fault_type_t ftype, const solver_config_t *config,
                                                atpg_result_t *out_res) {
    if (!nl || !target_net || !out_res) return LOGISAT_ERR_NULL_PTR;
    memset(out_res, 0, sizeof(*out_res));
    if (ftype == FAULT_NONE) return LOGISAT_OK;

    int32_t target_id = 0;
    if (netlist_find_net(nl, target_net, &target_id) != LOGISAT_OK) return LOGISAT_ERR_INVALID_VARIABLE;

    size_t n_in = nl->num_primary_inputs, n_out = nl->num_primary_outputs;
    cnf_formula_t *formula = cnf_formula_create(64, 128);
    int32_t cur_var = (int32_t)n_in;

    int32_t *map_good = (int32_t *)calloc(nl->num_nets + 1, sizeof(int32_t));
    int32_t *map_fault = (int32_t *)calloc(nl->num_nets + 1, sizeof(int32_t));
    for (size_t i = 0; i < n_in; ++i) {
        int32_t net_id = nl->primary_inputs[i];
        map_good[net_id] = (int32_t)(i + 1);
        map_fault[net_id] = (net_id == target_id) ? ++cur_var : (int32_t)(i + 1);
    }
    for (size_t i = 1; i <= nl->num_nets; ++i) {
        if (!map_good[i]) map_good[i] = ++cur_var;
        if (!map_fault[i]) map_fault[i] = ++cur_var;
    }

    tseytin_encode_netlist(nl, formula, map_good);
    for (size_t i = 0; i < nl->num_gates; ++i) {
        const circuit_gate_t *g = &nl->gates[i];
        if (g->output_net == target_id) continue; /* severed */
        int32_t in_vars[32];
        for (size_t j = 0; j < g->num_inputs; ++j) in_vars[j] = map_fault[g->input_nets[j]];
        tseytin_encode_gate(g->type, map_fault[g->output_net], in_vars, g->num_inputs, formula);
    }

    /* Clamp faulty target net to constant */
    int32_t tie[1] = { (ftype == FAULT_STUCK_AT_0) ? -map_fault[target_id] : map_fault[target_id] };
    cnf_formula_add_clause_lits(formula, tie, 1);

    int32_t *out1 = (int32_t *)malloc(sizeof(int32_t) * n_out);
    int32_t *out2 = (int32_t *)malloc(sizeof(int32_t) * n_out);
    for (size_t i = 0; i < n_out; ++i) {
        out1[i] = map_good[nl->primary_outputs[i]];
        out2[i] = map_fault[nl->primary_outputs[i]];
    }

    var_value_t *assign = NULL;
    solver_stats_t stats;
    solver_result_t res = solve_miter(formula, out1, out2, n_out, &cur_var, config, &assign, &stats);
    free(out1); free(out2);

    out_res->solved = true;
    out_res->stats = stats;
    out_res->is_testable = (res == SOLVER_SAT);

    if (res == SOLVER_SAT) {
        out_res->num_inputs = n_in;
        out_res->input_names = (char **)malloc(sizeof(char *) * n_in);
        out_res->test_vector = (var_value_t *)malloc(sizeof(var_value_t) * n_in);
        for (size_t i = 0; i < n_in; ++i) {
            out_res->input_names[i] = dup_str(netlist_get_net_name(nl, nl->primary_inputs[i]));
            out_res->test_vector[i] = assign[i + 1];
        }
    }

    free(map_good); free(map_fault); free(assign);
    cnf_formula_destroy(formula);
    return LOGISAT_OK;
}
