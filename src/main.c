// main.c - CLI entry point and interactive driver for LogiSAT
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
    printf("  LogiSAT: Digital Logic Equivalence & Fault Verifier                 \n");
    printf("======================================================================\n\n");
}

static void print_usage(const char *prog) {
    printf("Usage: %s [mode] [args] [--output rep.txt] [-v|--verbose]\n\n", prog);
    printf("Modes:\n");
    printf("  --equiv <c1.net> <c2.net>    Combinational Equivalence Checking (CEC)\n");
    printf("  --fault <c.net> <wire> <0|1> Generate ATPG test vector for stuck-at fault\n");
    printf("  --explain <c.net>            Analyze netlist DAG and show Tseytin CNF clauses\n");
    printf("  --dimacs <file.cnf>          Solve standard DIMACS CNF SAT problem\n");
    printf("  --to-cnf <c.net> <out.cnf>   Convert circuit netlist to DIMACS CNF\n");
    printf("  --help, -h                   Display this help manual\n\n");
    printf("Running '%s' without flags launches the interactive terminal.\n", prog);
}

static void run_explain_mode(const char *path) {
    netlist_t *nl = NULL;
    if (parser_parse_netlist_file(path, &nl) != LOGISAT_OK) {
        fprintf(stderr, "[!] Failed to parse circuit: %s\n", path);
        return;
    }
    printf("\n--- CIRCUIT: %s (Inputs: %zu, Outputs: %zu, Gates: %zu) ---\n",
           nl->name, nl->num_primary_inputs, nl->num_primary_outputs, nl->num_gates);
    printf("DAG Cycle Check: %s\n", netlist_validate_acyclic(nl) == LOGISAT_OK ? "PASS (Acyclic feedforward DAG)" : "FAIL (Cycle detected!)");

    cnf_formula_t *f = cnf_formula_create(nl->num_nets, nl->num_gates * 4);
    tseytin_encode_netlist(nl, f, NULL);
    printf("Tseytin CNF Formula: %zu Variables, %zu Clauses\n", f->num_vars, f->num_clauses);
    for (size_t i = 0; i < f->num_clauses && i < 15; ++i) {
        printf("  Clause %2zu: ( ", i + 1);
        for (size_t j = 0; j < f->clauses[i]->size; ++j) {
            int32_t lit = f->clauses[i]->literals[j];
            printf("%s%s ", lit < 0 ? "~" : "", netlist_get_net_name(nl, lit > 0 ? lit : -lit));
            if (j + 1 < f->clauses[i]->size) printf("v ");
        }
        printf(")\n");
    }
    if (f->num_clauses > 15) printf("  ... [%zu clauses total]\n", f->num_clauses);
    cnf_formula_destroy(f);
    netlist_destroy(nl);
}

static void run_equiv_mode(const char *c1, const char *c2, const char *rep, bool verb) {
    netlist_t *nl1 = NULL, *nl2 = NULL;
    if (parser_parse_netlist_file(c1, &nl1) || parser_parse_netlist_file(c2, &nl2)) {
        fprintf(stderr, "[!] Parse error in input netlists\n");
        netlist_destroy(nl1);
        netlist_destroy(nl2);
        return;
    }

    printf("[*] Equivalence Checking: %s vs %s (verbose=%s)\n", c1, c2, verb ? "ON" : "OFF");
    cec_result_t res;
    solver_config_t cfg = solver_config_default();
    cfg.verbose = verb;

    if (verifier_check_equivalence(nl1, nl2, &cfg, &res) == LOGISAT_OK) {
        printf("\n========================================================\n");
        printf("  VERDICT: %s\n", res.is_equivalent ? "EQUIVALENT [PROVEN - UNSAT]" : "NON-EQUIVALENT [DISPROVEN - SAT]");
        printf("========================================================\n");
        if (!res.is_equivalent) {
            printf("Counterexample: ");
            for (size_t i = 0; i < res.num_inputs; ++i) {
                printf("%s=%d ", res.input_names[i], res.counterexample_inputs[i] == VAL_TRUE ? 1 : 0);
            }
            printf("\n");
        }
        printf("Metrics: Time: %.3f ms | Decisions: %llu | Backtracks: %llu\n",
               res.stats.solve_time_ms, (unsigned long long)res.stats.num_decisions, (unsigned long long)res.stats.num_backtracks);
        if (rep) {
            reporter_write_cec_report(rep, nl1, nl2, &res);
            printf("[+] Report saved: %s\n", rep);
        }
        verifier_free_cec_result(&res);
    }
    netlist_destroy(nl1);
    netlist_destroy(nl2);
}

static void run_fault_mode(const char *c, const char *net, int fval, const char *rep, bool verb) {
    netlist_t *nl = NULL;
    if (parser_parse_netlist_file(c, &nl) != LOGISAT_OK) {
        fprintf(stderr, "[!] Parse error: %s\n", c);
        return;
    }

    printf("[*] ATPG Stuck-At-%d on net '%s' (verbose=%s)\n", fval, net, verb ? "ON" : "OFF");
    atpg_result_t res;
    solver_config_t cfg = solver_config_default();
    cfg.verbose = verb;

    if (verifier_generate_test_pattern(nl, net, fval ? FAULT_STUCK_AT_1 : FAULT_STUCK_AT_0, &cfg, &res) == LOGISAT_OK) {
        printf("\n========================================================\n");
        printf("  VERDICT: %s\n", res.is_testable ? "FAULT IS TESTABLE [SAT]" : "REDUNDANT / UNDETECTABLE [UNSAT]");
        printf("========================================================\n");
        if (res.is_testable) {
            printf("Test Vector: ");
            for (size_t i = 0; i < res.num_inputs; ++i) {
                printf("%s=%d ", res.input_names[i], res.test_vector[i] == VAL_TRUE ? 1 : 0);
            }
            printf("\n");
        }
        printf("Metrics: Time: %.3f ms | Decisions: %llu | Backtracks: %llu\n",
               res.stats.solve_time_ms, (unsigned long long)res.stats.num_decisions, (unsigned long long)res.stats.num_backtracks);
        if (rep) {
            reporter_write_atpg_report(rep, nl, net, fval ? FAULT_STUCK_AT_1 : FAULT_STUCK_AT_0, &res);
            printf("[+] Report saved: %s\n", rep);
        }
        verifier_free_atpg_result(&res);
    }
    netlist_destroy(nl);
}

static void run_dimacs_mode(const char *path, const char *rep, bool verb) {
    cnf_formula_t *f = NULL;
    if (dimacs_load_from_file(path, &f) != LOGISAT_OK) {
        fprintf(stderr, "[!] Load error: %s\n", path);
        return;
    }

    printf("[*] Solving DIMACS: %s (%zu vars, %zu clauses)\n", path, f->num_vars, f->num_clauses);
    var_value_t *assign = (var_value_t *)calloc(f->num_vars + 1, sizeof(var_value_t));
    solver_stats_t stats;
    solver_config_t cfg = solver_config_default();
    cfg.verbose = verb;

    solver_result_t res = solver_solve(f, &cfg, assign, &stats);
    printf(">>> RESULT: %s <<<\n", res == SOLVER_SAT ? "SATISFIABLE (SAT)" : "UNSATISFIABLE (UNSAT)");
    printf("Metrics: Time: %.3f ms | Decisions: %llu | Backtracks: %llu\n",
           stats.solve_time_ms, (unsigned long long)stats.num_decisions, (unsigned long long)stats.num_backtracks);

    if (rep) {
        reporter_write_dimacs_summary(rep, path, f, res, &stats, assign);
        printf("[+] Report saved: %s\n", rep);
    }
    free(assign);
    cnf_formula_destroy(f);
}

static void run_to_cnf_mode(const char *in_path, const char *out_path) {
    netlist_t *nl = NULL;
    if (parser_parse_netlist_file(in_path, &nl) != LOGISAT_OK) return;
    cnf_formula_t *f = cnf_formula_create(nl->num_nets, nl->num_gates * 4);
    tseytin_encode_netlist(nl, f, NULL);
    dimacs_save_to_file(out_path, f, nl->name);
    printf("[+] Exported %zu variables and %zu clauses to %s\n", f->num_vars, f->num_clauses, out_path);
    cnf_formula_destroy(f);
    netlist_destroy(nl);
}

static void run_interactive_mode(void) {
    print_banner();
    printf("Menu:\n1. Solve DIMACS CNF\n2. Equivalence Check (CEC)\n3. Stuck-At ATPG\n4. Convert to CNF\n5. Explain Circuit\n6. Exit\nChoice [1-6]: ");
    int choice = 0;
    if (scanf("%d", &choice) != 1) return;
    char f1[256], f2[256], net[64], v[8];
    int fval = 0;
    bool verb = false;

    if (choice >= 1 && choice <= 3) {
        printf("Verbose step-by-step trace? (y/n): ");
        if (scanf("%7s", v) == 1) {
            verb = (v[0] == 'y' || v[0] == 'Y');
        }
    }
    switch (choice) {
        case 1:
            printf("File: ");
            if (scanf("%255s", f1) == 1) run_dimacs_mode(f1, NULL, verb);
            break;
        case 2:
            printf("Circuit 1: ");
            if (scanf("%255s", f1) == 1) {
                printf("Circuit 2: ");
                if (scanf("%255s", f2) == 1) run_equiv_mode(f1, f2, NULL, verb);
            }
            break;
        case 3:
            printf("Circuit: ");
            if (scanf("%255s", f1) == 1) {
                printf("Net: ");
                if (scanf("%63s", net) == 1) {
                    printf("Stuck-at (0/1): ");
                    if (scanf("%d", &fval) == 1) run_fault_mode(f1, net, fval, NULL, verb);
                }
            }
            break;
        case 4:
            printf("Netlist: ");
            if (scanf("%255s", f1) == 1) {
                printf("Output CNF: ");
                if (scanf("%255s", f2) == 1) run_to_cnf_mode(f1, f2);
            }
            break;
        case 5:
            printf("Circuit: ");
            if (scanf("%255s", f1) == 1) run_explain_mode(f1);
            break;
        default:
            break;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        run_interactive_mode();
        return 0;
    }
    const char *rep = NULL;
    bool verb = false;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--output") && i + 1 < argc) rep = argv[++i];
        if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) verb = true;
    }

    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
        print_banner();
        print_usage(argv[0]);
    } else if (!strcmp(argv[1], "--explain") && argc >= 3) {
        run_explain_mode(argv[2]);
    } else if (!strcmp(argv[1], "--equiv") && argc >= 4) {
        run_equiv_mode(argv[2], argv[3], rep, verb);
    } else if (!strcmp(argv[1], "--fault") && argc >= 5) {
        run_fault_mode(argv[2], argv[3], atoi(argv[4]), rep, verb);
    } else if (!strcmp(argv[1], "--dimacs") && argc >= 3) {
        run_dimacs_mode(argv[2], rep, verb);
    } else if (!strcmp(argv[1], "--to-cnf") && argc >= 4) {
        run_to_cnf_mode(argv[2], argv[3]);
    } else {
        print_usage(argv[0]);
        return 1;
    }
    return 0;
}
