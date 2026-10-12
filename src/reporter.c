#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "reporter.h"

static void write_header(FILE *fp, const char *title) {
    fprintf(fp, "================================================================================\n");
    fprintf(fp, "  LogiSAT EDA Verification Engine - %s\n", title);
    fprintf(fp, "================================================================================\n\n");
}

logisat_status_t reporter_write_cec_report(const char *filepath,
                                           const netlist_t *nl1,
                                           const netlist_t *nl2,
                                           const cec_result_t *res) {
    if (!filepath || !nl1 || !nl2 || !res) return LOGISAT_ERR_NULL_PTR;

    FILE *fp = fopen(filepath, "w");
    if (!fp) return LOGISAT_ERR_FILE_IO;

    write_header(fp, "Combinational Equivalence Checking (CEC) Report");

    fprintf(fp, "[Circuit 1 Reference] : %s (%zu gates, %zu inputs, %zu outputs)\n",
            nl1->name, nl1->num_gates, nl1->num_primary_inputs, nl1->num_primary_outputs);
    fprintf(fp, "[Circuit 2 Candidate] : %s (%zu gates, %zu inputs, %zu outputs)\n",
            nl2->name, nl2->num_gates, nl2->num_primary_inputs, nl2->num_primary_outputs);
    fprintf(fp, "--------------------------------------------------------------------------------\n");

    if (res->is_equivalent) {
        fprintf(fp, "VERIFICATION VERDICT : EQUIVALENT [PROVEN]\n");
        fprintf(fp, "The two circuits are functionally identical across ALL 2^%zu input vectors.\n\n",
                nl1->num_primary_inputs);
    } else {
        fprintf(fp, "VERIFICATION VERDICT : NON-EQUIVALENT [DISPROVEN]\n");
        fprintf(fp, "Functional mismatch detected! Counterexample input vector:\n\n");
        fprintf(fp, "  Input Pin                 Logic Value\n");
        fprintf(fp, "  ------------------------  -----------\n");
        for (size_t i = 0; i < res->num_inputs; ++i) {
            fprintf(fp, "  %-24s  %s\n", res->input_names[i],
                    res->counterexample_inputs[i] == VAL_TRUE ? "1 (HIGH)" : "0 (LOW)");
        }
        fprintf(fp, "\n");
    }

    fprintf(fp, "--------------------------------------------------------------------------------\n");
    fprintf(fp, "Solver Performance Metrics:\n");
    fprintf(fp, "  Total Solve Time          : %.4f ms\n", res->stats.solve_time_ms);
    fprintf(fp, "  Branching Decisions       : %llu\n", (unsigned long long)res->stats.num_decisions);
    fprintf(fp, "  Backtrack Steps           : %llu\n", (unsigned long long)res->stats.num_backtracks);
    fprintf(fp, "  Unit Propagations (BCP)   : %llu\n", (unsigned long long)res->stats.num_unit_propagations);
    fprintf(fp, "  Pure Literals Eliminated  : %llu\n", (unsigned long long)res->stats.num_pure_literals);
    fprintf(fp, "================================================================================\n");

    fclose(fp);
    return LOGISAT_OK;
}

logisat_status_t reporter_write_atpg_report(const char *filepath,
                                            const netlist_t *nl,
                                            const char *target_net,
                                            fault_type_t fault_type,
                                            const atpg_result_t *res) {
    if (!filepath || !nl || !target_net || !res) return LOGISAT_ERR_NULL_PTR;

    FILE *fp = fopen(filepath, "w");
    if (!fp) return LOGISAT_ERR_FILE_IO;

    write_header(fp, "Automatic Test Pattern Generation (ATPG) Report");

    const char *ftype_str = (fault_type == FAULT_STUCK_AT_0) ? "Stuck-At-0 (s-a-0)" : "Stuck-At-1 (s-a-1)";
    fprintf(fp, "[Circuit Model]       : %s\n", nl->name);
    fprintf(fp, "[Target Fault Net]    : %s\n", target_net);
    fprintf(fp, "[Fault Model]         : %s\n", ftype_str);
    fprintf(fp, "--------------------------------------------------------------------------------\n");

    if (res->is_testable) {
        fprintf(fp, "TESTABILITY VERDICT   : TESTABLE FAULT\n");
        fprintf(fp, "Sensitizing test vector to excite and propagate fault to primary outputs:\n\n");
        fprintf(fp, "  Primary Input             Applied Test Value\n");
        fprintf(fp, "  ------------------------  ------------------\n");
        for (size_t i = 0; i < res->num_inputs; ++i) {
            fprintf(fp, "  %-24s  %s\n", res->input_names[i],
                    res->test_vector[i] == VAL_TRUE ? "1" : "0");
        }
        fprintf(fp, "\n");
    } else {
        fprintf(fp, "TESTABILITY VERDICT   : REDUNDANT / UNDETECTABLE FAULT\n");
        fprintf(fp, "No input combination can propagate this fault to any primary output.\n\n");
    }

    fprintf(fp, "--------------------------------------------------------------------------------\n");
    fprintf(fp, "Solver Performance Metrics:\n");
    fprintf(fp, "  Total Solve Time          : %.4f ms\n", res->stats.solve_time_ms);
    fprintf(fp, "  Branching Decisions       : %llu\n", (unsigned long long)res->stats.num_decisions);
    fprintf(fp, "  Backtrack Steps           : %llu\n", (unsigned long long)res->stats.num_backtracks);
    fprintf(fp, "  Unit Propagations (BCP)   : %llu\n", (unsigned long long)res->stats.num_unit_propagations);
    fprintf(fp, "================================================================================\n");

    fclose(fp);
    return LOGISAT_OK;
}

logisat_status_t reporter_write_dimacs_summary(const char *filepath,
                                               const char *dimacs_file,
                                               const cnf_formula_t *formula,
                                               solver_result_t res,
                                               const solver_stats_t *stats,
                                               const var_value_t *assignments) {
    if (!filepath || !formula || !stats) return LOGISAT_ERR_NULL_PTR;

    FILE *fp = fopen(filepath, "w");
    if (!fp) return LOGISAT_ERR_FILE_IO;

    write_header(fp, "DIMACS CNF Benchmark Solver Summary");

    fprintf(fp, "[Benchmark File]      : %s\n", dimacs_file ? dimacs_file : "standard_input");
    fprintf(fp, "[Variable Count]      : %zu\n", formula->num_vars);
    fprintf(fp, "[Clause Count]        : %zu\n", formula->num_clauses);
    fprintf(fp, "--------------------------------------------------------------------------------\n");

    if (res == SOLVER_SAT) {
        fprintf(fp, "RESULT                : SATISFIABLE (SAT)\n\n");
        if (assignments) {
            fprintf(fp, "Satisfying Model (first 100 vars shown):\n");
            size_t limit = formula->num_vars < 100 ? formula->num_vars : 100;
            for (size_t i = 1; i <= limit; ++i) {
                fprintf(fp, "%d ", assignments[i] == VAL_TRUE ? (int)i : -(int)i);
                if (i % 20 == 0) fprintf(fp, "\n");
            }
            if (formula->num_vars > 100) fprintf(fp, " ... [truncated]");
            fprintf(fp, " 0\n\n");
        }
    } else if (res == SOLVER_UNSAT) {
        fprintf(fp, "RESULT                : UNSATISFIABLE (UNSAT)\n\n");
    } else {
        fprintf(fp, "RESULT                : UNDETERMINED\n\n");
    }

    fprintf(fp, "--------------------------------------------------------------------------------\n");
    fprintf(fp, "Solver Performance Metrics:\n");
    fprintf(fp, "  Total Solve Time          : %.4f ms\n", stats->solve_time_ms);
    fprintf(fp, "  Branching Decisions       : %llu\n", (unsigned long long)stats->num_decisions);
    fprintf(fp, "  Backtrack Steps           : %llu\n", (unsigned long long)stats->num_backtracks);
    fprintf(fp, "  Unit Propagations (BCP)   : %llu\n", (unsigned long long)stats->num_unit_propagations);
    fprintf(fp, "  Pure Literals Eliminated  : %llu\n", (unsigned long long)stats->num_pure_literals);
    fprintf(fp, "================================================================================\n");

    fclose(fp);
    return LOGISAT_OK;
}
