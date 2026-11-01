// solver.c - DPLL Boolean satisfiability solver engine
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

static inline var_value_t get_literal_value(const var_value_t *assignments, int32_t lit) {
    int32_t var = lit > 0 ? lit : -lit;
    var_value_t val = assignments[var];
    if (val == VAL_UNASSIGNED) return VAL_UNASSIGNED;
    if (lit > 0) return val;
    return (val == VAL_TRUE) ? VAL_FALSE : VAL_TRUE;
}

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

static void backtrack_to(solver_context_t *ctx, size_t checkpoint) {
    while (ctx->trail_size > checkpoint) {
        int32_t var = ctx->trail[--ctx->trail_size];
        ctx->assignments[var] = VAL_UNASSIGNED;
    }
}

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
                if (ctx->config.verbose) {
                    printf("  [DPLL:Conflict] Clause #%zu unsatisfied -> Backtrack\n", i + 1);
                }
                return false;
            }

            if (unassigned_count == 1) {
                assign_literal(ctx, last_unassigned_lit);
                ctx->stats.num_unit_propagations++;
                if (ctx->config.verbose) {
                    int32_t var = last_unassigned_lit > 0 ? last_unassigned_lit : -last_unassigned_lit;
                    printf("  [DPLL:BCP] Unit clause #%zu forced: Var %d = %s\n",
                           i + 1, var, (last_unassigned_lit > 0) ? "TRUE" : "FALSE");
                }
                propagated = true;
                break;
            }
        }
    }

    return true;
}

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
                    printf("  [DPLL:PureLit] Var %zu = TRUE\n", v);
                }
            } else if (neg_counts[v] > 0 && pos_counts[v] == 0) {
                assign_literal(ctx, -(int32_t)v);
                ctx->stats.num_pure_literals++;
                if (ctx->config.verbose) {
                    printf("  [DPLL:PureLit] Var %zu = FALSE\n", v);
                }
            }
        }
    }

    free(pos_counts);
    free(neg_counts);
}

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

static bool dpll_solve_recursive(solver_context_t *ctx) {
    size_t checkpoint = ctx->trail_size;

    if (!bcp(ctx)) {
        backtrack_to(ctx, checkpoint);
        return false;
    }

    eliminate_pure_literals(ctx);

    if (is_formula_satisfied(ctx)) {
        if (ctx->config.verbose) {
            printf("  [DPLL:Solved] All clauses satisfied\n");
        }
        return true;
    }

    int32_t var = select_branching_variable(ctx);
    if (var == 0) {
        backtrack_to(ctx, checkpoint);
        return false;
    }

    ctx->stats.num_decisions++;
    if (ctx->config.verbose) {
        printf("[DPLL:Decision #%llu] Var %d = TRUE (Trail: %zu)\n",
               (unsigned long long)ctx->stats.num_decisions, var, ctx->trail_size);
    }

    assign_literal(ctx, var);
    if (dpll_solve_recursive(ctx)) {
        return true;
    }
    backtrack_to(ctx, checkpoint);

    ctx->stats.num_backtracks++;
    if (ctx->config.verbose) {
        printf("[DPLL:Backtrack #%llu] Var %d = FALSE (Trail: %zu)\n",
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

    if (formula->num_clauses == 0) {
        if (out_stats) memset(out_stats, 0, sizeof(solver_stats_t));
        return SOLVER_SAT;
    }

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
