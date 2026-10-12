#ifndef LOGISAT_TSEYTIN_H
#define LOGISAT_TSEYTIN_H

#include "netlist.h"
#include "cnf.h"
#include "logisat_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Converts a netlist graph into CNF clauses using Tseytin transformation.
 * 
 * @param nl Input netlist graph
 * @param formula CNF formula to populate with clauses
 * @param net_var_map Optional array mapping net_id to CNF variable ID (indexed 1..nl->num_nets).
 *                    If NULL, 1-to-1 mapping is used.
 * @return LOGISAT_OK on success.
 */
logisat_status_t tseytin_encode_netlist(const netlist_t *nl, cnf_formula_t *formula, const int32_t *net_var_map);

/**
 * @brief Encodes a single logic gate into equivalent CNF clauses.
 */
logisat_status_t tseytin_encode_gate(gate_type_t type, int32_t out_var, const int32_t *in_vars, size_t num_inputs, cnf_formula_t *formula);

#ifdef __cplusplus
}
#endif

#endif /* LOGISAT_TSEYTIN_H */
