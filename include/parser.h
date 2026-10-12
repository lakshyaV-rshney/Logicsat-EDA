#ifndef LOGISAT_PARSER_H
#define LOGISAT_PARSER_H

#include "netlist.h"
#include "logisat_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parses a structural gate-level netlist file into an acyclic netlist_t graph.
 * 
 * @param filepath Path to the .net netlist file.
 * @param out_netlist Pointer to receive allocated netlist structure.
 * @return LOGISAT_OK on success, or parser error.
 */
logisat_status_t parser_parse_netlist_file(const char *filepath, netlist_t **out_netlist);

#ifdef __cplusplus
}
#endif

#endif /* LOGISAT_PARSER_H */
