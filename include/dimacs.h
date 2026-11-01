// dimacs.h - DIMACS CNF file format parser and writer
#ifndef LOGISAT_DIMACS_H
#define LOGISAT_DIMACS_H

#include "cnf.h"
#include "logisat_status.h"

logisat_status_t dimacs_load_from_file(const char *filepath, cnf_formula_t **out_formula);
logisat_status_t dimacs_save_to_file(const char *filepath, const cnf_formula_t *formula, const char *comment_banner);

#endif
