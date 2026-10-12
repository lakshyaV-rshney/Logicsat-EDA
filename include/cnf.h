#ifndef LOGISAT_CNF_H
#define LOGISAT_CNF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "logisat_status.h"
#include "logisat_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t *literals;
    size_t size;
    size_t capacity;
} clause_t;

typedef struct {
    clause_t **clauses;
    size_t num_clauses;
    size_t capacity;
    size_t num_vars;
} cnf_formula_t;

/* Clause Management */
clause_t *clause_create(size_t initial_capacity);
void clause_destroy(clause_t *clause);
logisat_status_t clause_add_literal(clause_t *clause, int32_t literal);
clause_t *clause_clone(const clause_t *src);

/* CNF Formula Management */
cnf_formula_t *cnf_formula_create(size_t initial_vars, size_t initial_clauses);
void cnf_formula_destroy(cnf_formula_t *formula);
logisat_status_t cnf_formula_add_clause(cnf_formula_t *formula, clause_t *clause);
logisat_status_t cnf_formula_add_clause_lits(cnf_formula_t *formula, const int32_t *literals, size_t count);
cnf_formula_t *cnf_formula_clone(const cnf_formula_t *src);
void cnf_formula_update_var_count(cnf_formula_t *formula);

#ifdef __cplusplus
}
#endif

#endif /* LOGISAT_CNF_H */
