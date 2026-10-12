#include <stdlib.h>
#include "tseytin.h"

logisat_status_t tseytin_encode_gate(gate_type_t type, int32_t out_var, const int32_t *in_vars, size_t num_inputs, cnf_formula_t *formula) {
    if (!formula || out_var == 0) return LOGISAT_ERR_NULL_PTR;
    if (num_inputs == 0 && type != GATE_BUF) return LOGISAT_ERR_INVALID_CLAUSE;

    switch (type) {
        case GATE_BUF: {
            if (num_inputs < 1) return LOGISAT_ERR_INVALID_CLAUSE;
            int32_t in = in_vars[0];
            /* (out v ~in) ^ (~out v in) */
            int32_t c1[2] = { out_var, -in };
            int32_t c2[2] = { -out_var, in };
            cnf_formula_add_clause_lits(formula, c1, 2);
            cnf_formula_add_clause_lits(formula, c2, 2);
            break;
        }

        case GATE_NOT: {
            if (num_inputs < 1) return LOGISAT_ERR_INVALID_CLAUSE;
            int32_t in = in_vars[0];
            /* (out v in) ^ (~out v ~in) */
            int32_t c1[2] = { out_var, in };
            int32_t c2[2] = { -out_var, -in };
            cnf_formula_add_clause_lits(formula, c1, 2);
            cnf_formula_add_clause_lits(formula, c2, 2);
            break;
        }

        case GATE_AND: {
            /* (~out v in_1) ^ (~out v in_2) ... ^ (out v ~in_1 v ~in_2 ...) */
            int32_t *big_clause = (int32_t *)malloc(sizeof(int32_t) * (num_inputs + 1));
            if (!big_clause) return LOGISAT_ERR_OUT_OF_MEMORY;
            big_clause[0] = out_var;

            for (size_t i = 0; i < num_inputs; ++i) {
                int32_t bin[2] = { -out_var, in_vars[i] };
                cnf_formula_add_clause_lits(formula, bin, 2);
                big_clause[i + 1] = -in_vars[i];
            }
            cnf_formula_add_clause_lits(formula, big_clause, num_inputs + 1);
            free(big_clause);
            break;
        }

        case GATE_OR: {
            /* (out v ~in_1) ^ (out v ~in_2) ... ^ (~out v in_1 v in_2 ...) */
            int32_t *big_clause = (int32_t *)malloc(sizeof(int32_t) * (num_inputs + 1));
            if (!big_clause) return LOGISAT_ERR_OUT_OF_MEMORY;
            big_clause[0] = -out_var;

            for (size_t i = 0; i < num_inputs; ++i) {
                int32_t bin[2] = { out_var, -in_vars[i] };
                cnf_formula_add_clause_lits(formula, bin, 2);
                big_clause[i + 1] = in_vars[i];
            }
            cnf_formula_add_clause_lits(formula, big_clause, num_inputs + 1);
            free(big_clause);
            break;
        }

        case GATE_NAND: {
            /* (out v in_1) ^ (out v in_2) ... ^ (~out v ~in_1 v ~in_2 ...) */
            int32_t *big_clause = (int32_t *)malloc(sizeof(int32_t) * (num_inputs + 1));
            if (!big_clause) return LOGISAT_ERR_OUT_OF_MEMORY;
            big_clause[0] = -out_var;

            for (size_t i = 0; i < num_inputs; ++i) {
                int32_t bin[2] = { out_var, in_vars[i] };
                cnf_formula_add_clause_lits(formula, bin, 2);
                big_clause[i + 1] = -in_vars[i];
            }
            cnf_formula_add_clause_lits(formula, big_clause, num_inputs + 1);
            free(big_clause);
            break;
        }

        case GATE_NOR: {
            /* (~out v ~in_1) ^ (~out v ~in_2) ... ^ (out v in_1 v in_2 ...) */
            int32_t *big_clause = (int32_t *)malloc(sizeof(int32_t) * (num_inputs + 1));
            if (!big_clause) return LOGISAT_ERR_OUT_OF_MEMORY;
            big_clause[0] = out_var;

            for (size_t i = 0; i < num_inputs; ++i) {
                int32_t bin[2] = { -out_var, -in_vars[i] };
                cnf_formula_add_clause_lits(formula, bin, 2);
                big_clause[i + 1] = in_vars[i];
            }
            cnf_formula_add_clause_lits(formula, big_clause, num_inputs + 1);
            free(big_clause);
            break;
        }

        case GATE_XOR: {
            if (num_inputs != 2) return LOGISAT_ERR_UNSUPPORTED_GATE;
            int32_t a = in_vars[0], b = in_vars[1], z = out_var;
            /* (~a v ~b v ~z) ^ (a v b v ~z) ^ (a v ~b v z) ^ (~a v b v z) */
            int32_t c1[3] = { -a, -b, -z };
            int32_t c2[3] = {  a,  b, -z };
            int32_t c3[3] = {  a, -b,  z };
            int32_t c4[3] = { -a,  b,  z };
            cnf_formula_add_clause_lits(formula, c1, 3);
            cnf_formula_add_clause_lits(formula, c2, 3);
            cnf_formula_add_clause_lits(formula, c3, 3);
            cnf_formula_add_clause_lits(formula, c4, 3);
            break;
        }

        case GATE_XNOR: {
            if (num_inputs != 2) return LOGISAT_ERR_UNSUPPORTED_GATE;
            int32_t a = in_vars[0], b = in_vars[1], z = out_var;
            /* (~a v b v ~z) ^ (a v ~b v ~z) ^ (a v b v z) ^ (~a v ~b v z) */
            int32_t c1[3] = { -a,  b, -z };
            int32_t c2[3] = {  a, -b, -z };
            int32_t c3[3] = {  a,  b,  z };
            int32_t c4[3] = { -a, -b,  z };
            cnf_formula_add_clause_lits(formula, c1, 3);
            cnf_formula_add_clause_lits(formula, c2, 3);
            cnf_formula_add_clause_lits(formula, c3, 3);
            cnf_formula_add_clause_lits(formula, c4, 3);
            break;
        }

        default:
            return LOGISAT_ERR_UNSUPPORTED_GATE;
    }

    return LOGISAT_OK;
}

logisat_status_t tseytin_encode_netlist(const netlist_t *nl, cnf_formula_t *formula, const int32_t *net_var_map) {
    if (!nl || !formula) return LOGISAT_ERR_NULL_PTR;

    for (size_t i = 0; i < nl->num_gates; ++i) {
        const circuit_gate_t *g = &nl->gates[i];
        int32_t out_var = net_var_map ? net_var_map[g->output_net] : g->output_net;

        int32_t mapped_inputs[32];
        for (size_t j = 0; j < g->num_inputs; ++j) {
            mapped_inputs[j] = net_var_map ? net_var_map[g->input_nets[j]] : g->input_nets[j];
        }

        logisat_status_t st = tseytin_encode_gate(g->type, out_var, mapped_inputs, g->num_inputs, formula);
        if (st != LOGISAT_OK) return st;
    }

    cnf_formula_update_var_count(formula);
    return LOGISAT_OK;
}
