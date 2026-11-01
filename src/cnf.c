// cnf.c - CNF formula and clause operations
#include <stdlib.h>
#include <string.h>
#include "cnf.h"

clause_t *clause_create(size_t initial_capacity) {
    if (initial_capacity == 0) {
        initial_capacity = 4;
    }
    clause_t *clause = (clause_t *)malloc(sizeof(clause_t));
    if (!clause) return NULL;

    clause->literals = (int32_t *)malloc(sizeof(int32_t) * initial_capacity);
    if (!clause->literals) {
        free(clause);
        return NULL;
    }
    clause->size = 0;
    clause->capacity = initial_capacity;
    return clause;
}

void clause_destroy(clause_t *clause) {
    if (!clause) return;
    if (clause->literals) {
        free(clause->literals);
        clause->literals = NULL;
    }
    free(clause);
}

logisat_status_t clause_add_literal(clause_t *clause, int32_t literal) {
    if (!clause) return LOGISAT_ERR_NULL_PTR;
    if (literal == 0) return LOGISAT_ERR_INVALID_VARIABLE;

    if (clause->size >= clause->capacity) {
        size_t new_cap = clause->capacity * 2;
        int32_t *new_lits = (int32_t *)realloc(clause->literals, sizeof(int32_t) * new_cap);
        if (!new_lits) return LOGISAT_ERR_OUT_OF_MEMORY;
        clause->literals = new_lits;
        clause->capacity = new_cap;
    }

    clause->literals[clause->size++] = literal;
    return LOGISAT_OK;
}

clause_t *clause_clone(const clause_t *src) {
    if (!src) return NULL;
    clause_t *copy = clause_create(src->size > 0 ? src->size : 4);
    if (!copy) return NULL;

    for (size_t i = 0; i < src->size; ++i) {
        copy->literals[i] = src->literals[i];
    }
    copy->size = src->size;
    return copy;
}

cnf_formula_t *cnf_formula_create(size_t initial_vars, size_t initial_clauses) {
    if (initial_clauses == 0) initial_clauses = 16;
    cnf_formula_t *formula = (cnf_formula_t *)malloc(sizeof(cnf_formula_t));
    if (!formula) return NULL;

    formula->clauses = (clause_t **)malloc(sizeof(clause_t *) * initial_clauses);
    if (!formula->clauses) {
        free(formula);
        return NULL;
    }
    formula->num_clauses = 0;
    formula->capacity = initial_clauses;
    formula->num_vars = initial_vars;
    return formula;
}

void cnf_formula_destroy(cnf_formula_t *formula) {
    if (!formula) return;
    if (formula->clauses) {
        for (size_t i = 0; i < formula->num_clauses; ++i) {
            clause_destroy(formula->clauses[i]);
        }
        free(formula->clauses);
        formula->clauses = NULL;
    }
    free(formula);
}

logisat_status_t cnf_formula_add_clause(cnf_formula_t *formula, clause_t *clause) {
    if (!formula || !clause) return LOGISAT_ERR_NULL_PTR;

    if (formula->num_clauses >= formula->capacity) {
        size_t new_cap = formula->capacity * 2;
        clause_t **new_clauses = (clause_t **)realloc(formula->clauses, sizeof(clause_t *) * new_cap);
        if (!new_clauses) return LOGISAT_ERR_OUT_OF_MEMORY;
        formula->clauses = new_clauses;
        formula->capacity = new_cap;
    }

    formula->clauses[formula->num_clauses++] = clause;

    for (size_t i = 0; i < clause->size; ++i) {
        int32_t lit = clause->literals[i];
        size_t var = (size_t)(lit > 0 ? lit : -lit);
        if (var > formula->num_vars) {
            formula->num_vars = var;
        }
    }

    return LOGISAT_OK;
}

logisat_status_t cnf_formula_add_clause_lits(cnf_formula_t *formula, const int32_t *literals, size_t count) {
    if (!formula || (!literals && count > 0)) return LOGISAT_ERR_NULL_PTR;

    clause_t *clause = clause_create(count);
    if (!clause) return LOGISAT_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < count; ++i) {
        logisat_status_t status = clause_add_literal(clause, literals[i]);
        if (status != LOGISAT_OK) {
            clause_destroy(clause);
            return status;
        }
    }

    return cnf_formula_add_clause(formula, clause);
}

cnf_formula_t *cnf_formula_clone(const cnf_formula_t *src) {
    if (!src) return NULL;
    cnf_formula_t *copy = cnf_formula_create(src->num_vars, src->num_clauses);
    if (!copy) return NULL;

    for (size_t i = 0; i < src->num_clauses; ++i) {
        clause_t *cloned_clause = clause_clone(src->clauses[i]);
        if (!cloned_clause) {
            cnf_formula_destroy(copy);
            return NULL;
        }
        cnf_formula_add_clause(copy, cloned_clause);
    }

    copy->num_vars = src->num_vars;
    return copy;
}

void cnf_formula_update_var_count(cnf_formula_t *formula) {
    if (!formula) return;
    size_t max_var = 0;
    for (size_t i = 0; i < formula->num_clauses; ++i) {
        const clause_t *cl = formula->clauses[i];
        for (size_t j = 0; j < cl->size; ++j) {
            int32_t lit = cl->literals[j];
            size_t var = (size_t)(lit > 0 ? lit : -lit);
            if (var > max_var) max_var = var;
        }
    }
    formula->num_vars = max_var;
}
