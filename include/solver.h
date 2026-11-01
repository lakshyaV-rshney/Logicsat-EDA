// solver.h - DPLL SAT solver engine
#ifndef LOGISAT_SOLVER_H
#define LOGISAT_SOLVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "cnf.h"
#include "logisat_status.h"
#include "logisat_types.h"

typedef enum {
    HEURISTIC_FIRST_UNASSIGNED = 0,
    HEURISTIC_DLIS,
    HEURISTIC_MOMS
} branching_heuristic_t;

typedef struct {
    bool enable_pure_literal;
    branching_heuristic_t heuristic;
    bool verbose;
} solver_config_t;

typedef struct {
    uint64_t num_decisions;
    uint64_t num_backtracks;
    uint64_t num_unit_propagations;
    uint64_t num_pure_literals;
    double solve_time_ms;
} solver_stats_t;

typedef struct {
    const cnf_formula_t *formula;
    var_value_t *assignments;
    size_t num_vars;

    int32_t *trail;
    size_t trail_size;
    size_t trail_capacity;

    solver_config_t config;
    solver_stats_t stats;
} solver_context_t;

solver_config_t solver_config_default(void);

solver_result_t solver_solve(const cnf_formula_t *formula,
                             const solver_config_t *config,
                             var_value_t *out_assignments,
                             solver_stats_t *out_stats);

#endif
