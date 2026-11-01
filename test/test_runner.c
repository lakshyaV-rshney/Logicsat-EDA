// test_runner.c - Unit test suite for LogiSAT
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_framework.h"
#include "cnf.h"
#include "dimacs.h"
#include "netlist.h"
#include "parser.h"
#include "tseytin.h"
#include "solver.h"
#include "verifier.h"

TEST(SuiteCNF, CreateAndAddLiterals) {
    clause_t *clause = clause_create(2);
    ASSERT_TRUE(clause != NULL);
    EXPECT_EQ(clause->size, 0);

    EXPECT_EQ(clause_add_literal(clause, 1), LOGISAT_OK);
    EXPECT_EQ(clause_add_literal(clause, -2), LOGISAT_OK);
    EXPECT_EQ(clause_add_literal(clause, 3), LOGISAT_OK);
    EXPECT_EQ(clause->size, 3);
    EXPECT_EQ(clause->literals[0], 1);
    EXPECT_EQ(clause->literals[1], -2);
    EXPECT_EQ(clause->literals[2], 3);

    clause_destroy(clause);
}

TEST(SuiteCNF, FormulaAddAndClone) {
    cnf_formula_t *formula = cnf_formula_create(4, 2);
    ASSERT_TRUE(formula != NULL);

    int32_t c1[2] = { 1, 2 };
    int32_t c2[2] = { -1, 3 };
    EXPECT_EQ(cnf_formula_add_clause_lits(formula, c1, 2), LOGISAT_OK);
    EXPECT_EQ(cnf_formula_add_clause_lits(formula, c2, 2), LOGISAT_OK);
    EXPECT_EQ(formula->num_clauses, 2);

    cnf_formula_t *cloned = cnf_formula_clone(formula);
    ASSERT_TRUE(cloned != NULL);
    EXPECT_EQ(cloned->num_clauses, 2);
    EXPECT_EQ(cloned->clauses[0]->literals[0], 1);
    EXPECT_EQ(cloned->clauses[1]->literals[1], 3);

    cnf_formula_destroy(formula);
    cnf_formula_destroy(cloned);
}

TEST(SuiteDIMACS, SaveAndReloadRoundTrip) {
    cnf_formula_t *orig = cnf_formula_create(3, 2);
    int32_t c1[2] = { 1, -2 };
    int32_t c2[3] = { 2, -3, 1 };
    cnf_formula_add_clause_lits(orig, c1, 2);
    cnf_formula_add_clause_lits(orig, c2, 3);

    const char *tmp_path = "tmp_test_roundtrip.cnf";
    EXPECT_EQ(dimacs_save_to_file(tmp_path, orig, "Test Banner"), LOGISAT_OK);

    cnf_formula_t *loaded = NULL;
    EXPECT_EQ(dimacs_load_from_file(tmp_path, &loaded), LOGISAT_OK);
    ASSERT_TRUE(loaded != NULL);

    EXPECT_EQ(loaded->num_clauses, 2);
    EXPECT_EQ(loaded->clauses[0]->size, 2);
    EXPECT_EQ(loaded->clauses[0]->literals[0], 1);
    EXPECT_EQ(loaded->clauses[0]->literals[1], -2);
    EXPECT_EQ(loaded->clauses[1]->size, 3);

    cnf_formula_destroy(orig);
    cnf_formula_destroy(loaded);
    remove(tmp_path);
}

TEST(SuiteSolver, TrivialSAT) {
    cnf_formula_t *f = cnf_formula_create(2, 1);
    int32_t c1[2] = { 1, -2 };
    cnf_formula_add_clause_lits(f, c1, 2);

    var_value_t assignments[3];
    solver_result_t res = solver_solve(f, NULL, assignments, NULL);
    EXPECT_EQ(res, SOLVER_SAT);

    cnf_formula_destroy(f);
}

TEST(SuiteSolver, TrivialUNSAT) {
    cnf_formula_t *f = cnf_formula_create(1, 2);
    int32_t c1[1] = { 1 };
    int32_t c2[1] = { -1 };
    cnf_formula_add_clause_lits(f, c1, 1);
    cnf_formula_add_clause_lits(f, c2, 1);

    solver_result_t res = solver_solve(f, NULL, NULL, NULL);
    EXPECT_EQ(res, SOLVER_UNSAT);

    cnf_formula_destroy(f);
}

TEST(SuiteSolver, UnitPropagationChain) {
    cnf_formula_t *f = cnf_formula_create(3, 3);
    int32_t c1[1] = { 1 };
    int32_t c2[2] = { -1, 2 };
    int32_t c3[2] = { -2, 3 };
    cnf_formula_add_clause_lits(f, c1, 1);
    cnf_formula_add_clause_lits(f, c2, 2);
    cnf_formula_add_clause_lits(f, c3, 2);

    var_value_t assignments[4];
    solver_stats_t stats;
    solver_result_t res = solver_solve(f, NULL, assignments, &stats);

    EXPECT_EQ(res, SOLVER_SAT);
    EXPECT_EQ(assignments[1], VAL_TRUE);
    EXPECT_EQ(assignments[2], VAL_TRUE);
    EXPECT_EQ(assignments[3], VAL_TRUE);
    EXPECT_TRUE(stats.num_unit_propagations >= 2);

    cnf_formula_destroy(f);
}

TEST(SuiteSolver, PigeonholePrinciple_PHP_2_3_UNSAT) {
    cnf_formula_t *f = cnf_formula_create(6, 9);
    int32_t p1[2] = { 1, 2 };
    int32_t p2[2] = { 3, 4 };
    int32_t p3[2] = { 5, 6 };
    cnf_formula_add_clause_lits(f, p1, 2);
    cnf_formula_add_clause_lits(f, p2, 2);
    cnf_formula_add_clause_lits(f, p3, 2);

    int32_t h1_1[2] = { -1, -3 };
    int32_t h1_2[2] = { -1, -5 };
    int32_t h1_3[2] = { -3, -5 };
    cnf_formula_add_clause_lits(f, h1_1, 2);
    cnf_formula_add_clause_lits(f, h1_2, 2);
    cnf_formula_add_clause_lits(f, h1_3, 2);

    int32_t h2_1[2] = { -2, -4 };
    int32_t h2_2[2] = { -2, -6 };
    int32_t h2_3[2] = { -4, -6 };
    cnf_formula_add_clause_lits(f, h2_1, 2);
    cnf_formula_add_clause_lits(f, h2_2, 2);
    cnf_formula_add_clause_lits(f, h2_3, 2);

    solver_result_t res = solver_solve(f, NULL, NULL, NULL);
    EXPECT_EQ(res, SOLVER_UNSAT);

    cnf_formula_destroy(f);
}

TEST(SuiteNetlist, AcyclicPasses) {
    netlist_t *nl = netlist_create("dag");
    int32_t in1, in2, mid, out;
    netlist_add_net(nl, "a", true, false, &in1);
    netlist_add_net(nl, "b", true, false, &in2);
    netlist_add_net(nl, "m", false, false, &mid);
    netlist_add_net(nl, "y", false, true, &out);

    int32_t in_pair[2] = { in1, in2 };
    netlist_add_gate(nl, GATE_AND, mid, in_pair, 2);
    int32_t not_in[1] = { mid };
    netlist_add_gate(nl, GATE_NOT, out, not_in, 1);

    EXPECT_EQ(netlist_validate_acyclic(nl), LOGISAT_OK);
    netlist_destroy(nl);
}

TEST(SuiteNetlist, CyclicLoopDetected) {
    netlist_t *nl = netlist_create("cycle");
    int32_t n1, n2;
    netlist_add_net(nl, "net1", false, false, &n1);
    netlist_add_net(nl, "net2", false, false, &n2);

    int32_t g1_in[1] = { n1 };
    netlist_add_gate(nl, GATE_NOT, n2, g1_in, 1);

    int32_t g2_in[1] = { n2 };
    netlist_add_gate(nl, GATE_NOT, n1, g2_in, 1);

    EXPECT_EQ(netlist_validate_acyclic(nl), LOGISAT_ERR_CYCLIC_NETLIST);
    netlist_destroy(nl);
}

TEST(SuiteVerifier, DeMorganEquivalence_Proven) {
    netlist_t *nl1 = netlist_create("nand_gate");
    int32_t a1, b1, y1;
    netlist_add_net(nl1, "a", true, false, &a1);
    netlist_add_net(nl1, "b", true, false, &b1);
    netlist_add_net(nl1, "y", false, true, &y1);
    int32_t in1[2] = { a1, b1 };
    netlist_add_gate(nl1, GATE_NAND, y1, in1, 2);

    netlist_t *nl2 = netlist_create("de_morgan");
    int32_t a2, b2, na, nb, y2;
    netlist_add_net(nl2, "a", true, false, &a2);
    netlist_add_net(nl2, "b", true, false, &b2);
    netlist_add_net(nl2, "na", false, false, &na);
    netlist_add_net(nl2, "nb", false, false, &nb);
    netlist_add_net(nl2, "y", false, true, &y2);
    int32_t ina[1] = { a2 };
    netlist_add_gate(nl2, GATE_NOT, na, ina, 1);
    int32_t inb[1] = { b2 };
    netlist_add_gate(nl2, GATE_NOT, nb, inb, 1);
    int32_t in_or[2] = { na, nb };
    netlist_add_gate(nl2, GATE_OR, y2, in_or, 2);

    cec_result_t res;
    EXPECT_EQ(verifier_check_equivalence(nl1, nl2, NULL, &res), LOGISAT_OK);
    EXPECT_TRUE(res.is_equivalent);

    verifier_free_cec_result(&res);
    netlist_destroy(nl1);
    netlist_destroy(nl2);
}

TEST(SuiteVerifier, BuggyCircuit_DisprovenWithCounterexample) {
    netlist_t *nl1 = netlist_create("and_gate");
    int32_t a1, b1, y1;
    netlist_add_net(nl1, "a", true, false, &a1);
    netlist_add_net(nl1, "b", true, false, &b1);
    netlist_add_net(nl1, "y", false, true, &y1);
    int32_t in1[2] = { a1, b1 };
    netlist_add_gate(nl1, GATE_AND, y1, in1, 2);

    netlist_t *nl2 = netlist_create("buggy_or");
    int32_t a2, b2, y2;
    netlist_add_net(nl2, "a", true, false, &a2);
    netlist_add_net(nl2, "b", true, false, &b2);
    netlist_add_net(nl2, "y", false, true, &y2);
    int32_t in2[2] = { a2, b2 };
    netlist_add_gate(nl2, GATE_OR, y2, in2, 2);

    cec_result_t res;
    EXPECT_EQ(verifier_check_equivalence(nl1, nl2, NULL, &res), LOGISAT_OK);
    EXPECT_FALSE(res.is_equivalent);
    EXPECT_TRUE(res.num_inputs == 2);

    verifier_free_cec_result(&res);
    netlist_destroy(nl1);
    netlist_destroy(nl2);
}

TEST(SuiteATPG, DetectStuckAt0) {
    netlist_t *nl = netlist_create("and_circuit");
    int32_t a, b, y;
    netlist_add_net(nl, "a", true, false, &a);
    netlist_add_net(nl, "b", true, false, &b);
    netlist_add_net(nl, "y", false, true, &y);
    int32_t in[2] = { a, b };
    netlist_add_gate(nl, GATE_AND, y, in, 2);

    atpg_result_t res;
    EXPECT_EQ(verifier_generate_test_pattern(nl, "a", FAULT_STUCK_AT_0, NULL, &res), LOGISAT_OK);
    EXPECT_TRUE(res.is_testable);

    for (size_t i = 0; i < res.num_inputs; ++i) {
        if (strcmp(res.input_names[i], "a") == 0 || strcmp(res.input_names[i], "b") == 0) {
            EXPECT_EQ(res.test_vector[i], VAL_TRUE);
        }
    }

    verifier_free_atpg_result(&res);
    netlist_destroy(nl);
}

int main(void) {
    return run_all_unit_tests();
}
