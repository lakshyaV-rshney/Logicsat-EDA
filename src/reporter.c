#include <stdio.h>
#include <stdlib.h>
#include "reporter.h"

static void print_metrics(FILE *fp, const solver_stats_t *s) {
    fprintf(fp, "--------------------------------------------------------------------------------\n");
    fprintf(fp, "Performance: Time: %.3f ms | Decisions: %llu | Backtracks: %llu | BCP: %llu\n",
            s->solve_time_ms, (unsigned long long)s->num_decisions,
            (unsigned long long)s->num_backtracks, (unsigned long long)s->num_unit_propagations);
    fprintf(fp, "================================================================================\n");
}

logisat_status_t reporter_write_cec_report(const char *path, const netlist_t *nl1, const netlist_t *nl2, const cec_result_t *res) {
    if (!path || !nl1 || !nl2 || !res) return LOGISAT_ERR_NULL_PTR;
    FILE *fp = fopen(path, "w");
    if (!fp) return LOGISAT_ERR_FILE_IO;

    fprintf(fp, "=== LogiSAT Combinational Equivalence Report ===\n");
    fprintf(fp, "Circuit 1: %s (%zu gates) | Circuit 2: %s (%zu gates)\n", nl1->name, nl1->num_gates, nl2->name, nl2->num_gates);
    fprintf(fp, "Verdict  : %s\n", res->is_equivalent ? "EQUIVALENT [PROVEN]" : "NON-EQUIVALENT [DISPROVEN]");

    if (!res->is_equivalent) {
        fprintf(fp, "Counterexample:\n");
        for (size_t i = 0; i < res->num_inputs; ++i) {
            fprintf(fp, "  %s = %d\n", res->input_names[i], res->counterexample_inputs[i] == VAL_TRUE ? 1 : 0);
        }
    }
    print_metrics(fp, &res->stats);
    fclose(fp);
    return LOGISAT_OK;
}

logisat_status_t reporter_write_atpg_report(const char *path, const netlist_t *nl, const char *net, fault_type_t ftype, const atpg_result_t *res) {
    if (!path || !nl || !net || !res) return LOGISAT_ERR_NULL_PTR;
    FILE *fp = fopen(path, "w");
    if (!fp) return LOGISAT_ERR_FILE_IO;

    fprintf(fp, "=== LogiSAT ATPG Test Pattern Report ===\n");
    fprintf(fp, "Circuit: %s | Target Wire: %s (Stuck-At-%d)\n", nl->name, net, ftype == FAULT_STUCK_AT_0 ? 0 : 1);
    fprintf(fp, "Verdict: %s\n", res->is_testable ? "TESTABLE FAULT" : "REDUNDANT FAULT");

    if (res->is_testable) {
        fprintf(fp, "Test Pattern:\n");
        for (size_t i = 0; i < res->num_inputs; ++i) {
            fprintf(fp, "  %s = %d\n", res->input_names[i], res->test_vector[i] == VAL_TRUE ? 1 : 0);
        }
    }
    print_metrics(fp, &res->stats);
    fclose(fp);
    return LOGISAT_OK;
}

logisat_status_t reporter_write_dimacs_summary(const char *path, const char *file, const cnf_formula_t *f,
                                               solver_result_t res, const solver_stats_t *stats, const var_value_t *assign) {
    if (!path || !f || !stats) return LOGISAT_ERR_NULL_PTR;
    FILE *fp = fopen(path, "w");
    if (!fp) return LOGISAT_ERR_FILE_IO;

    fprintf(fp, "=== LogiSAT DIMACS Benchmark Report ===\n");
    fprintf(fp, "File: %s (Vars: %zu, Clauses: %zu) -> %s\n", file ? file : "-", f->num_vars, f->num_clauses, res == SOLVER_SAT ? "SAT" : "UNSAT");

    if (res == SOLVER_SAT && assign) {
        fprintf(fp, "Model: ");
        size_t lim = f->num_vars < 30 ? f->num_vars : 30;
        for (size_t i = 1; i <= lim; ++i) fprintf(fp, "%d ", assign[i] == VAL_TRUE ? (int)i : -(int)i);
        if (f->num_vars > 30) fprintf(fp, "... [truncated]");
        fprintf(fp, "\n");
    }
    print_metrics(fp, stats);
    fclose(fp);
    return LOGISAT_OK;
}
