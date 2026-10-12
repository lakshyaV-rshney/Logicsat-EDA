#ifndef LOGISAT_SOLVER_H
#define LOGISAT_SOLVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "cnf.h"
#include "logisat_status.h"
#include "logisat_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HEURISTIC_FIRST_UNASSIGNED = 0,
    HEURISTIC_DLIS, /* Dynamic Largest Individual Sum */
    HEURISTIC_MOMS  /* Maximum Occurrences in clauses of Minimum Size */
} branching_heuristic_t;

typedef struct {
    bool enable_pure_literal;
    branching_heuristic_t heuristic;
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
    var_value_t *assignments; /* 1-based indexing, size = formula->num_vars + 1 */
    size_t num_vars;

    /* Backtracking Trail / Assignment Stack */
    int32_t *trail;
    size_t trail_size;
    size_t trail_capacity;

    solver_config_t config;
    solver_stats_t stats;
} solver_context_t;

/**
 * @brief Initializes a default solver configuration.
 */
solver_config_t solver_config_default(void);

/**
 * @brief Solves a CNF formula using the DPLL engine.
 * 
 * @param formula Input CNF formula
 * @param config Solver options (or pass NULL for defaults)
 * @param out_assignments Pre-allocated array of size formula->num_vars + 1 to store variable assignments (or NULL)
 * @param out_stats Pointer to store execution performance statistics (or NULL)
 * @return SOLVER_SAT or SOLVER_UNSAT.
 */
solver_result_t solver_solve(const cnf_formula_t *formula,
                             const solver_config_t *config,
                             var_value_t *out_assignments,
                             solver_stats_t *out_stats);

#ifdef __cplusplus
}
#endif

#endif /* LOGISAT_SOLVER_H */
