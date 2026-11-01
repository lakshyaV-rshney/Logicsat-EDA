// parser.c - Netlist lexical analyzer and parser
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"

#define MAX_LINE_LEN 2048

static char *trim_whitespace(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

logisat_status_t parser_parse_netlist_file(const char *filepath, netlist_t **out_netlist) {
    if (!filepath || !out_netlist) return LOGISAT_ERR_NULL_PTR;
    *out_netlist = NULL;

    FILE *fp = fopen(filepath, "r");
    if (!fp) return LOGISAT_ERR_FILE_IO;

    char line[MAX_LINE_LEN];
    netlist_t *nl = NULL;

    while (fgets(line, sizeof(line), fp)) {
        char *p = trim_whitespace(line);
        if (*p == '\0' || *p == '#' || (p[0] == '/' && p[1] == '/')) {
            continue;
        }

        char *keyword = strtok(p, " \t\r\n");
        if (!keyword) continue;

        if (strcmp(keyword, "CIRCUIT") == 0 || strcmp(keyword, "circuit") == 0) {
            char *name = strtok(NULL, " \t\r\n");
            if (!nl) {
                nl = netlist_create(name ? name : "circuit");
                if (!nl) {
                    fclose(fp);
                    return LOGISAT_ERR_OUT_OF_MEMORY;
                }
            }
        } else if (strcmp(keyword, "INPUT") == 0 || strcmp(keyword, "input") == 0) {
            if (!nl) nl = netlist_create("unnamed");
            char *net_name = strtok(NULL, " \t\r\n");
            while (net_name) {
                int32_t id = 0;
                netlist_add_net(nl, net_name, true, false, &id);
                net_name = strtok(NULL, " \t\r\n");
            }
        } else if (strcmp(keyword, "OUTPUT") == 0 || strcmp(keyword, "output") == 0) {
            if (!nl) nl = netlist_create("unnamed");
            char *net_name = strtok(NULL, " \t\r\n");
            while (net_name) {
                int32_t id = 0;
                netlist_add_net(nl, net_name, false, true, &id);
                net_name = strtok(NULL, " \t\r\n");
            }
        } else if (strcmp(keyword, "GATE") == 0 || strcmp(keyword, "gate") == 0) {
            if (!nl) nl = netlist_create("unnamed");
            char *inst_name = strtok(NULL, " \t\r\n");
            char *type_str = strtok(NULL, " \t\r\n");
            char *out_name = strtok(NULL, " \t\r\n");
            (void)inst_name;

            if (!type_str || !out_name) {
                fclose(fp);
                netlist_destroy(nl);
                return LOGISAT_ERR_PARSE_SYNTAX;
            }

            gate_type_t gtype = gate_type_from_string(type_str);
            int32_t out_id = 0;
            netlist_add_net(nl, out_name, false, false, &out_id);

            int32_t in_ids[32];
            size_t in_count = 0;
            char *in_name = strtok(NULL, " \t\r\n");
            while (in_name && in_count < 32) {
                int32_t in_id = 0;
                netlist_add_net(nl, in_name, false, false, &in_id);
                in_ids[in_count++] = in_id;
                in_name = strtok(NULL, " \t\r\n");
            }

            if (in_count == 0) {
                fclose(fp);
                netlist_destroy(nl);
                return LOGISAT_ERR_PARSE_SYNTAX;
            }

            netlist_add_gate(nl, gtype, out_id, in_ids, in_count);
        } else if (strcmp(keyword, "END") == 0 || strcmp(keyword, "end") == 0) {
            break;
        }
    }

    fclose(fp);

    if (!nl) {
        return LOGISAT_ERR_PARSE_SYNTAX;
    }

    logisat_status_t cycle_check = netlist_validate_acyclic(nl);
    if (cycle_check != LOGISAT_OK) {
        netlist_destroy(nl);
        return cycle_check;
    }

    *out_netlist = nl;
    return LOGISAT_OK;
}
