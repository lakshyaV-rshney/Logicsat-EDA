/**
 * ==============================================================================
 * LogiSAT DPLL SAT Solver Engine
 * ==============================================================================
 * 
 * [SY B.TECH CURRICULUM CONCEPTS USED]:
 * 1. Data Structures & Algorithms:
 *    - Explicit Assignment Stack (Trail): Enables O(1) push and fast backtracking
 *      without copying the entire formula at each decision level.
 *    - Recursion & Backtracking: Depth-First Search over the binary decision tree.
 * 2. Discrete Mathematics:
 *    - Propositional Logic & Conjunctive Normal Form (CNF): Formula is an AND of OR clauses.
 *    - Unit Clause Rule: If a clause has only one unassigned literal and all others
 *      are FALSE, that literal MUST evaluate to TRUE for the formula to be satisfied.
 *    - Pure Literal Rule: If a variable appears with only one polarity across all
 *      unassigned clauses, assigning it that polarity satisfies those clauses safely.
 * 3. Heuristics:
 *    - DLIS (Dynamic Largest Individual Sum): Greedy choice selecting the variable
 *      that occurs most frequently in currently unsatisfied clauses.
 * ==============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "solver.h"

solver_config_t solver_config_default(void) {
    solver_config_t cfg;
    cfg.enable_pure_literal = true;
    cfg.heuristic = HEURISTIC_DLIS;
    cfg.verbose = false;
    return cfg;
}

/**
 * @brief Computes the effective truth value of a literal under current partial assignments.
 * 
 * Literal representation:
 *   +x -> Variable x positive polarity
 *   -x -> Variable x negated polarity
 */
static inline var_value_t get_literal_value(const var_value_t *assignments, int32_t lit) {
    int32_t var = lit > 0 ? lit : -lit;
    var_value_t val = assignments[var];
    if (val == VAL_UNASSIGNED) return VAL_UNASSIGNED;
    if (lit > 0) return val;
    return (val == VAL_TRUE) ? VAL_FALSE : VAL_TRUE;
}

/**
 * @brief Assigns a variable and records it onto the trail stack for backtracking.
 */
static void assign_literal(solver_context_t *ctx, int32_t lit) {
    int32_t var = lit > 0 ? lit : -lit;
    var_value_t val = (lit > 0) ? VAL_TRUE : VAL_FALSE;
    ctx->assignments[var] = val;

    if (ctx->trail_size >= ctx->trail_capacity) {
        size_t new_cap = ctx->trail_capacity * 2;
        int32_t *new_trail = (int32_t *)realloc(ctx->trail, sizeof(int32_t) * new_cap);
        if (new_trail) {
            ctx->trail = new_trail;
            ctx->trail_capacity = new_cap;
        }
    }
    ctx->trail[ctx->trail_size++] = var;
}

/**
 * @brief Backtracks the assignment trail stack down to a previous checkpoint.
 * Resets all assignments made since the checkpoint to VAL_UNASSIGNED.
 */
static void backtrack_to(solver_context_t *ctx, size_t checkpoint) {
    while (ctx->trail_size > checkpoint) {
        int32_t var = ctx->trail[--ctx->trail_size];
        ctx->assignments[var] = VAL_UNASSIGNED;
    }
}

/**
 * @brief Boolean Constraint Propagation (BCP) / Unit Clause Propagation
 * 
 * If a clause has (N-1) literals evaluated to FALSE and 1 literal UNASSIGNED,
 * the remaining literal MUST be assigned TRUE. Repeat until no more unit clauses.
 * 
 * @return true if consistent, false if a conflict (empty clause where all literals are FALSE) occurs.
 */
static bool bcp(solver_context_t *ctx) {
    bool propagated = true;

    while (propagated) {
        propagated = false;

        for (size_t i = 0; i < ctx->formula->num_clauses; ++i) {
            const clause_t *cl = ctx->formula->clauses[i];
            bool clause_satisfied = false;
            size_t unassigned_count = 0;
            int32_t last_unassigned_lit = 0;

            for (size_t j = 0; j < cl->size; ++j) {
                int32_t lit = cl->literals[j];
                var_value_t val = get_literal_value(ctx->assignments, lit);

                if (val == VAL_TRUE) {
                    clause_satisfied = true;
                    break;
                } else if (val == VAL_UNASSIGNED) {
                    unassigned_count++;
                    last_unassigned_lit = lit;
                }
            }

            if (clause_satisfied) continue;

            if (unassigned_count == 0) {
                /* All literals are FALSE in this clause: Conflict detected! */
                if (ctx->config.verbose) {
                    printf("  [DPLL:Conflict] Clause #%zu unsatisfied under current assignments -> Backtrack triggered!\n", i + 1);
                }
                return false;
            }

            if (unassigned_count == 1) {
                /* Unit clause found: exactly one literal remains unassigned */
                assign_literal(ctx, last_unassigned_lit);
                ctx->stats.num_unit_propagations++;
                if (ctx->config.verbose) {
                    int32_t var = last_unassigned_lit > 0 ? last_unassigned_lit : -last_unassigned_lit;
                    printf("  [DPLL:BCP] Unit clause #%zu forced: Var %d = %s\n",
                           i + 1, var, (last_unassigned_lit > 0) ? "TRUE" : "FALSE");
                }
                propagated = true;
                break; /* Restart scan with new unit assignment */
            }
        }
    }

    return true;
}

/**
 * @brief Pure Literal Elimination rule.
 * 
 * If a variable occurs in unresolved clauses with only positive (or only negative) polarity,
 * it can be immediately assigned without causing any conflicts.
 */
static void eliminate_pure_literals(solver_context_t *ctx) {
    if (!ctx->config.enable_pure_literal) return;

    int *pos_counts = (int *)calloc(ctx->num_vars + 1, sizeof(int));
    int *neg_counts = (int *)calloc(ctx->num_vars + 1, sizeof(int));
    if (!pos_counts || !neg_counts) {
        free(pos_counts);
        free(neg_counts);
        return;
    }

    for (size_t i = 0; i < ctx->formula->num_clauses; ++i) {
        const clause_t *cl = ctx->formula->clauses[i];
        bool clause_satisfied = false;

        for (size_t j = 0; j < cl->size; ++j) {
            if (get_literal_value(ctx->assignments, cl->literals[j]) == VAL_TRUE) {
                clause_satisfied = true;
                break;
            }
        }

        if (clause_satisfied) continue;

        for (size_t j = 0; j < cl->size; ++j) {
            int32_t lit = cl->literals[j];
            int32_t var = lit > 0 ? lit : -lit;
            if (ctx->assignments[var] == VAL_UNASSIGNED) {
                if (lit > 0) pos_counts[var]++;
                else neg_counts[var]++;
            }
        }
    }

    for (size_t v = 1; v <= ctx->num_vars; ++v) {
        if (ctx->assignments[v] == VAL_UNASSIGNED) {
            if (pos_counts[v] > 0 && neg_counts[v] == 0) {
                assign_literal(ctx, (int32_t)v);
                ctx->stats.num_pure_literals++;
                if (ctx->config.verbose) {
                    printf("  [DPLL:PureLit] Pure positive literal: Var %zu = TRUE\n", v);
                }
            } else if (neg_counts[v] > 0 && pos_counts[v] == 0) {
                assign_literal(ctx, -(int32_t)v);
                ctx->stats.num_pure_literals++;
                if (ctx->config.verbose) {
                    printf("  [DPLL:PureLit] Pure negative literal: Var %zu = FALSE\n", v);
                }
            }
        }
    }

    free(pos_counts);
    free(neg_counts);
}

/**
 * @brief Checks if all clauses in the formula are currently satisfied.
 */
static bool is_formula_satisfied(solver_context_t *ctx) {
    for (size_t i = 0; i < ctx->formula->num_clauses; ++i) {
        const clause_t *cl = ctx->formula->clauses[i];
        bool satisfied = false;
        for (size_t j = 0; j < cl->size; ++j) {
            if (get_literal_value(ctx->assignments, cl->literals[j]) == VAL_TRUE) {
                satisfied = true;
                break;
            }
        }
        if (!satisfied) return false;
    }
    return true;
}

/**
 * @brief Selects next branching variable using DLIS heuristic (most frequent in unsatisfied clauses).
 */
static int32_t select_branching_variable(solver_context_t *ctx) {
    int32_t best_var = 0;
    int max_count = -1;
    int *counts = (int *)calloc(ctx->num_vars + 1, sizeof(int));
    if (!counts) return 0;

    for (size_t i = 0; i < ctx->formula->num_clauses; ++i) {
        const clause_t *cl = ctx->formula->clauses[i];
        bool satisfied = false;
        for (size_t j = 0; j < cl->size; ++j) {
            if (get_literal_value(ctx->assignments, cl->literals[j]) == VAL_TRUE) {
                satisfied = true;
                break;
            }
        }
        if (satisfied) continue;

        for (size_t j = 0; j < cl->size; ++j) {
            int32_t lit = cl->literals[j];
            int32_t var = lit > 0 ? lit : -lit;
            if (ctx->assignments[var] == VAL_UNASSIGNED) counts[var]++;
        }
    }

    for (size_t v = 1; v <= ctx->num_vars; ++v) {
        if (ctx->assignments[v] == VAL_UNASSIGNED && counts[v] > max_count) {
            max_count = counts[v];
            best_var = (int32_t)v;
        }
    }

    free(counts);
    return best_var;
}

/**
 * @brief Core recursive DPLL engine implementing Depth-First Search with backtracking.
 */
static bool dpll_solve_recursive(solver_context_t *ctx) {
    /* Save trail checkpoint for backtracking */
    size_t checkpoint = ctx->trail_size;

    /* 1. Unit Propagation (BCP) */
    if (!bcp(ctx)) {
        backtrack_to(ctx, checkpoint);
        return false;
    }

    /* 2. Pure Literal Elimination */
    eliminate_pure_literals(ctx);

    /* 3. Check if all clauses are already satisfied */
    if (is_formula_satisfied(ctx)) {
        if (ctx->config.verbose) {
            printf("  [DPLL:Solved] All clauses satisfied successfully!\n");
        }
        return true;
    }

    /* 4. Branching decision */
    int32_t var = select_branching_variable(ctx);
    if (var == 0) {
        /* No variables left; formula was verified as unsatisfied */
        backtrack_to(ctx, checkpoint);
        return false;
    }

    ctx->stats.num_decisions++;
    if (ctx->config.verbose) {
        printf("[DPLL:Decision #%llu] Guessing Var %d = TRUE (Trail depth: %zu)\n",
               (unsigned long long)ctx->stats.num_decisions, var, ctx->trail_size);
    }

    /* Branch 1: Try var = TRUE */
    assign_literal(ctx, var);
    if (dpll_solve_recursive(ctx)) {
        return true;
    }
    backtrack_to(ctx, checkpoint);

    /* Branch 2: Backtrack and try var = FALSE */
    ctx->stats.num_backtracks++;
    if (ctx->config.verbose) {
        printf("[DPLL:Backtrack #%llu] Inverting branch: Var %d = FALSE (Trail reset to %zu)\n",
               (unsigned long long)ctx->stats.num_backtracks, var, checkpoint);
    }
    assign_literal(ctx, -var);
    if (dpll_solve_recursive(ctx)) {
        return true;
    }
    backtrack_to(ctx, checkpoint);

    return false;
}

solver_result_t solver_solve(const cnf_formula_t *formula,
                             const solver_config_t *config,
                             var_value_t *out_assignments,
                             solver_stats_t *out_stats) {
    if (!formula) return SOLVER_UNDETERMINED;

    /* Handle empty formula */
    if (formula->num_clauses == 0) {
        if (out_stats) memset(out_stats, 0, sizeof(solver_stats_t));
        return SOLVER_SAT;
    }

    /* Check for explicit empty clause: ( ) is immediately UNSAT */
    for (size_t i = 0; i < formula->num_clauses; ++i) {
        if (formula->clauses[i]->size == 0) {
            if (out_stats) memset(out_stats, 0, sizeof(solver_stats_t));
            return SOLVER_UNSAT;
        }
    }

    solver_context_t ctx;
    ctx.formula = formula;
    ctx.num_vars = formula->num_vars;
    ctx.assignments = (var_value_t *)calloc(formula->num_vars + 1, sizeof(var_value_t));
    if (!ctx.assignments) return SOLVER_UNDETERMINED;

    ctx.trail_capacity = (formula->num_vars + 1) * 2;
    if (ctx.trail_capacity < 32) ctx.trail_capacity = 32;
    ctx.trail = (int32_t *)malloc(sizeof(int32_t) * ctx.trail_capacity);
    if (!ctx.trail) {
        free(ctx.assignments);
        return SOLVER_UNDETERMINED;
    }
    ctx.trail_size = 0;

    ctx.config = config ? *config : solver_config_default();
    memset(&ctx.stats, 0, sizeof(solver_stats_t));

    clock_t start = clock();
    bool satisfied = dpll_solve_recursive(&ctx);
    clock_t end = clock();

    ctx.stats.solve_time_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;

    if (satisfied && out_assignments) {
        for (size_t i = 1; i <= formula->num_vars; ++i) {
            /* Any unassigned variable can default to FALSE (or arbitrary valid truth value) */
            out_assignments[i] = (ctx.assignments[i] != VAL_UNASSIGNED) ? ctx.assignments[i] : VAL_FALSE;
        }
    }

    if (out_stats) {
        *out_stats = ctx.stats;
    }

    free(ctx.assignments);
    free(ctx.trail);

    return satisfied ? SOLVER_SAT : SOLVER_UNSAT;
}
