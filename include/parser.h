// parser.h - Netlist file parser
#ifndef LOGISAT_PARSER_H
#define LOGISAT_PARSER_H

#include "netlist.h"
#include "logisat_status.h"

logisat_status_t parser_parse_netlist_file(const char *filepath, netlist_t **out_netlist);

#endif
