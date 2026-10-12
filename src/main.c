#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logisat_status.h"
#include "logisat_types.h"
#include "cnf.h"
#include "dimacs.h"
#include "netlist.h"
#include "parser.h"
#include "tseytin.h"
#include "solver.h"
#include "verifier.h"
#include "reporter.h"

static void print_banner(void) {
    printf("======================================================================\n");
    printf("  LogiSAT: Digital Logic Equivalence & Fault Verifier (DPLL-EDA)     \n");
    printf("  Automated Software Engineering System - Mini Project Submission     \n");
    printf("======================================================================\n\n");
}

static void print_usage(const char *prog_name) {
    printf("Usage: %s [options]\n\n", prog_name);
    printf("Modes:\n");
    printf("  --dimacs <file.cnf> [--output <rep.txt>]         Solve a DIMACS CNF SAT problem\n");
    printf("  --equiv <c1.net> <c2.net> [--output <rep.txt>]  Check combinational equivalence (CEC)\n");
    printf("  --fault <c.net> <net> <0|1> [--output <rep.txt>] Generate test pattern for stuck-at fault\n");
    printf("  --to-cnf <c.net> <out.cnf>                      Convert netlist to DIMACS CNF\n");
    printf("  --help                                          Display this help manual\n\n");
    printf("Running '%s' without flags launches interactive engineer mode.\n", prog_name);
}

static void run_dimacs_mode(const char *dimacs_path, const char *output_rep) {
    printf("[*] Loading DIMACS CNF file: %s ...\n", dimacs_path);
    cnf_formula_t *formula = NULL;
    logisat_status_t st = dimacs_load_from_file(dimacs_path, &formula);
    if (st != LOGISAT_OK) {
        fprintf(stderr, "[!] Error loading DIMACS file: %s\n", logisat_status_to_string(st));
        return;
    }

    printf("    Variables: %zu, Clauses: %zu\n", formula->num_vars, formula->num_clauses);
    printf("[*] Solving formula via DPLL engine ...\n");

    var_value_t *assignments = (var_value_t *)calloc(formula->num_vars + 1, sizeof(var_value_t));
    solver_stats_t stats;
    solver_config_t cfg = solver_config_default();

    solver_result_t res = solver_solve(formula, &cfg, assignments, &stats);

    if (res == SOLVER_SAT) {
        printf("\n>>> RESULT: SATISFIABLE (SAT) <<<\n");
        printf("Satisfying model: ");
        size_t show_count = formula->num_vars < 15 ? formula->num_vars : 15;
        for (size_t i = 1; i <= show_count; ++i) {
            printf("%d ", assignments[i] == VAL_TRUE ? (int)i : -(int)i);
        }
        if (formula->num_vars > 15) printf("... [truncated]");
        printf(" 0\n");
    } else {
        printf("\n>>> RESULT: UNSATISFIABLE (UNSAT) <<<\n");
    }

    printf("\nExecution Statistics:\n");
    printf("  Solve Time          : %.3f ms\n", stats.solve_time_ms);
    printf("  Decisions           : %llu\n", (unsigned long long)stats.num_decisions);
    printf("  Backtracks          : %llu\n", (unsigned long long)stats.num_backtracks);
    printf("  Unit Propagations   : %llu\n", (unsigned long long)stats.num_unit_propagations);
    printf("  Pure Literals       : %llu\n", (unsigned long long)stats.num_pure_literals);

    if (output_rep) {
        reporter_write_dimacs_summary(output_rep, dimacs_path, formula, res, &stats, assignments);
        printf("[+] Diagnostic report written to: %s\n", output_rep);
    }

    free(assignments);
    cnf_formula_destroy(formula);
}

static void run_equiv_mode(const char *c1_path, const char *c2_path, const char *output_rep) {
    printf("[*] Parsing Circuit 1: %s ...\n", c1_path);
    netlist_t *nl1 = NULL;
    logisat_status_t st1 = parser_parse_netlist_file(c1_path, &nl1);
    if (st1 != LOGISAT_OK) {
        fprintf(stderr, "[!] Parse error in %s: %s\n", c1_path, logisat_status_to_string(st1));
        return;
    }

    printf("[*] Parsing Circuit 2: %s ...\n", c2_path);
    netlist_t *nl2 = NULL;
    logisat_status_t st2 = parser_parse_netlist_file(c2_path, &nl2);
    if (st2 != LOGISAT_OK) {
        fprintf(stderr, "[!] Parse error in %s: %s\n", c2_path, logisat_status_to_string(st2));
        netlist_destroy(nl1);
        return;
    }

    printf("[*] Constructing Miter circuit and solving equivalence ...\n");
    cec_result_t cec_res;
    solver_config_t cfg = solver_config_default();
    logisat_status_t st = verifier_check_equivalence(nl1, nl2, &cfg, &cec_res);

    if (st != LOGISAT_OK) {
        fprintf(stderr, "[!] Verification error: %s\n", logisat_status_to_string(st));
        netlist_destroy(nl1);
        netlist_destroy(nl2);
        return;
    }

    if (cec_res.is_equivalent) {
        printf("\n========================================================\n");
        printf("  VERDICT: CIRCUITS ARE FUNCTIONALLY EQUIVALENT [PROVEN] \n");
        printf("========================================================\n");
    } else {
        printf("\n========================================================\n");
        printf("  VERDICT: CIRCUITS ARE NON-EQUIVALENT [DISPROVEN]      \n");
        printf("========================================================\n");
        printf("Counterexample input vector found:\n");
        for (size_t i = 0; i < cec_res.num_inputs; ++i) {
            printf("  %s = %d\n", cec_res.input_names[i], cec_res.counterexample_inputs[i] == VAL_TRUE ? 1 : 0);
        }
    }

    printf("\nMetrics: Time: %.3f ms | Decisions: %llu | Backtracks: %llu\n",
           cec_res.stats.solve_time_ms,
           (unsigned long long)cec_res.stats.num_decisions,
           (unsigned long long)cec_res.stats.num_backtracks);

    if (output_rep) {
        reporter_write_cec_report(output_rep, nl1, nl2, &cec_res);
        printf("[+] Verification report exported to: %s\n", output_rep);
    }

    verifier_free_cec_result(&cec_res);
    netlist_destroy(nl1);
    netlist_destroy(nl2);
}

static void run_fault_mode(const char *c_path, const char *target_net, int fault_val, const char *output_rep) {
    printf("[*] Parsing Circuit: %s ...\n", c_path);
    netlist_t *nl = NULL;
    logisat_status_t st = parser_parse_netlist_file(c_path, &nl);
    if (st != LOGISAT_OK) {
        fprintf(stderr, "[!] Parse error: %s\n", logisat_status_to_string(st));
        return;
    }

    fault_type_t ftype = (fault_val == 0) ? FAULT_STUCK_AT_0 : FAULT_STUCK_AT_1;
    printf("[*] Testing stuck-at-%d fault on net '%s' ...\n", fault_val, target_net);

    atpg_result_t atpg_res;
    solver_config_t cfg = solver_config_default();
    st = verifier_generate_test_pattern(nl, target_net, ftype, &cfg, &atpg_res);

    if (st != LOGISAT_OK) {
        fprintf(stderr, "[!] ATPG error: %s\n", logisat_status_to_string(st));
        netlist_destroy(nl);
        return;
    }

    if (atpg_res.is_testable) {
        printf("\n========================================================\n");
        printf("  VERDICT: FAULT IS TESTABLE                            \n");
        printf("========================================================\n");
        printf("Test pattern vector:\n");
        for (size_t i = 0; i < atpg_res.num_inputs; ++i) {
            printf("  %s = %d\n", atpg_res.input_names[i], atpg_res.test_vector[i] == VAL_TRUE ? 1 : 0);
        }
    } else {
        printf("\n========================================================\n");
        printf("  VERDICT: REDUNDANT / UNDETECTABLE FAULT               \n");
        printf("========================================================\n");
    }

    printf("\nMetrics: Time: %.3f ms | Decisions: %llu | Backtracks: %llu\n",
           atpg_res.stats.solve_time_ms,
           (unsigned long long)atpg_res.stats.num_decisions,
           (unsigned long long)atpg_res.stats.num_backtracks);

    if (output_rep) {
        reporter_write_atpg_report(output_rep, nl, target_net, ftype, &atpg_res);
        printf("[+] ATPG report exported to: %s\n", output_rep);
    }

    verifier_free_atpg_result(&atpg_res);
    netlist_destroy(nl);
}

static void run_to_cnf_mode(const char *c_path, const char *out_cnf) {
    printf("[*] Parsing Circuit: %s ...\n", c_path);
    netlist_t *nl = NULL;
    logisat_status_t st = parser_parse_netlist_file(c_path, &nl);
    if (st != LOGISAT_OK) {
        fprintf(stderr, "[!] Parse error: %s\n", logisat_status_to_string(st));
        return;
    }

    printf("[*] Applying Tseytin Transformation ...\n");
    cnf_formula_t *formula = cnf_formula_create(nl->num_nets, nl->num_gates * 4);
    st = tseytin_encode_netlist(nl, formula, NULL);
    if (st != LOGISAT_OK) {
        fprintf(stderr, "[!] Transform error: %s\n", logisat_status_to_string(st));
        cnf_formula_destroy(formula);
        netlist_destroy(nl);
        return;
    }

    st = dimacs_save_to_file(out_cnf, formula, nl->name);
    if (st == LOGISAT_OK) {
        printf("[+] Exported DIMACS CNF to: %s (%zu variables, %zu clauses)\n",
               out_cnf, formula->num_vars, formula->num_clauses);
    } else {
        fprintf(stderr, "[!] Error saving DIMACS file: %s\n", logisat_status_to_string(st));
    }

    cnf_formula_destroy(formula);
    netlist_destroy(nl);
}

static void run_interactive_mode(void) {
    print_banner();
    printf("Interactive Engineering Menu:\n");
    printf("1. Solve DIMACS CNF benchmark file\n");
    printf("2. Check Combinational Equivalence between two netlists\n");
    printf("3. Generate Stuck-At Fault test pattern (ATPG)\n");
    printf("4. Convert Netlist to DIMACS CNF\n");
    printf("5. Exit\n\n");
    printf("Select option [1-5]: ");

    int choice = 0;
    if (scanf("%d", &choice) != 1) return;

    char file1[256], file2[256], rep[256], net[64];
    int fault_val = 0;

    switch (choice) {
        case 1:
            printf("Enter DIMACS CNF file path: ");
            scanf("%255s", file1);
            printf("Enter output report path (or '-' to skip): ");
            scanf("%255s", rep);
            run_dimacs_mode(file1, strcmp(rep, "-") == 0 ? NULL : rep);
            break;
        case 2:
            printf("Enter Circuit 1 path: ");
            scanf("%255s", file1);
            printf("Enter Circuit 2 path: ");
            scanf("%255s", file2);
            printf("Enter output report path (or '-' to skip): ");
            scanf("%255s", rep);
            run_equiv_mode(file1, file2, strcmp(rep, "-") == 0 ? NULL : rep);
            break;
        case 3:
            printf("Enter Circuit path: ");
            scanf("%255s", file1);
            printf("Enter target net name: ");
            scanf("%63s", net);
            printf("Enter stuck-at value (0 or 1): ");
            scanf("%d", &fault_val);
            printf("Enter output report path (or '-' to skip): ");
            scanf("%255s", rep);
            run_fault_mode(file1, net, fault_val, strcmp(rep, "-") == 0 ? NULL : rep);
            break;
        case 4:
            printf("Enter Netlist path: ");
            scanf("%255s", file1);
            printf("Enter destination CNF path: ");
            scanf("%255s", file2);
            run_to_cnf_mode(file1, file2);
            break;
        default:
            printf("Exiting.\n");
            break;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        run_interactive_mode();
        return 0;
    }

    const char *rep_file = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            rep_file = argv[i + 1];
            break;
        }
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_banner();
        print_usage(argv[0]);
        return 0;
    } else if (strcmp(argv[1], "--dimacs") == 0 && argc >= 3) {
        run_dimacs_mode(argv[2], rep_file);
        return 0;
    } else if (strcmp(argv[1], "--equiv") == 0 && argc >= 4) {
        run_equiv_mode(argv[2], argv[3], rep_file);
        return 0;
    } else if (strcmp(argv[1], "--fault") == 0 && argc >= 5) {
        int fval = atoi(argv[4]);
        run_fault_mode(argv[2], argv[3], fval, rep_file);
        return 0;
    } else if (strcmp(argv[1], "--to-cnf") == 0 && argc >= 4) {
        run_to_cnf_mode(argv[2], argv[3]);
        return 0;
    } else {
        fprintf(stderr, "Unknown or invalid command options.\n\n");
        print_usage(argv[0]);
        return 1;
    }
}
