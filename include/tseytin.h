// tseytin.h - Tseytin CNF transformation engine
#ifndef LOGISAT_TSEYTIN_H
#define LOGISAT_TSEYTIN_H

#include "netlist.h"
#include "cnf.h"
#include "logisat_status.h"

logisat_status_t tseytin_encode_netlist(const netlist_t *nl, cnf_formula_t *formula, const int32_t *net_var_map);
logisat_status_t tseytin_encode_gate(gate_type_t type, int32_t out_var, const int32_t *in_vars, size_t num_inputs, cnf_formula_t *formula);

#endif
