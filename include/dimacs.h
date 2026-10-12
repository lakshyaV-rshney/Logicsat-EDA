#ifndef LOGISAT_DIMACS_H
#define LOGISAT_DIMACS_H

#include "cnf.h"
#include "logisat_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parses a standard DIMACS CNF file into a cnf_formula_t structure.
 * 
 * Supports:
 *  - Arbitrary comment lines prefixed with 'c'
 *  - Standard problem line 'p cnf <vars> <clauses>'
 *  - Multi-line clauses terminated by literal '0'
 * 
 * @param filepath Path to the .cnf file
 * @param out_formula Pointer to receive the allocated cnf_formula_t pointer
 * @return LOGISAT_OK on success, or appropriate error code.
 */
logisat_status_t dimacs_load_from_file(const char *filepath, cnf_formula_t **out_formula);

/**
 * @brief Exports a cnf_formula_t structure to a standard DIMACS CNF file.
 * 
 * @param filepath Destination file path
 * @param formula CNF formula to serialize
 * @param comment_banner Optional comment text to include at the top of file
 * @return LOGISAT_OK on success, or appropriate error code.
 */
logisat_status_t dimacs_save_to_file(const char *filepath, const cnf_formula_t *formula, const char *comment_banner);

#ifdef __cplusplus
}
#endif

#endif /* LOGISAT_DIMACS_H */
