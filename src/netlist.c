// netlist.c - Netlist graph construction and cycle validation
#include <stdlib.h>
#include <string.h>
#include "netlist.h"

const char *gate_type_to_string(gate_type_t type) {
    switch (type) {
        case GATE_BUF:  return "BUF";
        case GATE_NOT:  return "NOT";
        case GATE_AND:  return "AND";
        case GATE_OR:   return "OR";
        case GATE_NAND: return "NAND";
        case GATE_NOR:  return "NOR";
        case GATE_XOR:  return "XOR";
        case GATE_XNOR: return "XNOR";
        default:        return "UNKNOWN";
    }
}

gate_type_t gate_type_from_string(const char *str) {
    if (!str) return GATE_BUF;
    if (!strcmp(str, "BUF") || !strcmp(str, "buf")) return GATE_BUF;
    if (!strcmp(str, "NOT") || !strcmp(str, "not") || !strcmp(str, "INV")) return GATE_NOT;
    if (!strcmp(str, "AND") || !strcmp(str, "and")) return GATE_AND;
    if (!strcmp(str, "OR")  || !strcmp(str, "or"))  return GATE_OR;
    if (!strcmp(str, "NAND")|| !strcmp(str, "nand"))return GATE_NAND;
    if (!strcmp(str, "NOR") || !strcmp(str, "nor")) return GATE_NOR;
    if (!strcmp(str, "XOR") || !strcmp(str, "xor")) return GATE_XOR;
    if (!strcmp(str, "XNOR")|| !strcmp(str, "xnor"))return GATE_XNOR;
    return GATE_BUF;
}

static bool grow_arr(void **arr, size_t *cap, size_t count, size_t elem_size) {
    if (count >= *cap) {
        size_t ncap = *cap ? *cap * 2 : 8;
        void *p = realloc(*arr, ncap * elem_size);
        if (!p) return false;
        *arr = p;
        *cap = ncap;
    }
    return true;
}

netlist_t *netlist_create(const char *name) {
    netlist_t *nl = (netlist_t *)calloc(1, sizeof(netlist_t));
    if (!nl) return NULL;
    strncpy(nl->name, name ? name : "circuit", sizeof(nl->name) - 1);
    return nl;
}

void netlist_destroy(netlist_t *nl) {
    if (!nl) return;
    free(nl->nets);
    for (size_t i = 0; i < nl->num_gates; ++i) {
        free(nl->gates[i].input_nets);
    }
    free(nl->gates);
    free(nl->primary_inputs);
    free(nl->primary_outputs);
    free(nl);
}

logisat_status_t netlist_add_net(netlist_t *nl, const char *name, bool is_in, bool is_out, int32_t *out_id) {
    if (!nl || !name) return LOGISAT_ERR_NULL_PTR;
    for (size_t i = 0; i < nl->num_nets; ++i) {
        if (strcmp(nl->nets[i].name, name) == 0) {
            if (is_in) nl->nets[i].is_primary_input = true;
            if (is_out) nl->nets[i].is_primary_output = true;
            if (out_id) *out_id = nl->nets[i].id;
            return LOGISAT_OK;
        }
    }

    if (!grow_arr((void**)&nl->nets, &nl->net_capacity, nl->num_nets, sizeof(circuit_net_t))) {
        return LOGISAT_ERR_OUT_OF_MEMORY;
    }
    int32_t id = (int32_t)(++nl->num_nets);
    circuit_net_t *net = &nl->nets[id - 1];
    strncpy(net->name, name, sizeof(net->name) - 1);
    net->id = id;
    net->is_primary_input = is_in;
    net->is_primary_output = is_out;

    if (is_in) {
        if (!grow_arr((void**)&nl->primary_inputs, &nl->pi_capacity, nl->num_primary_inputs, sizeof(int32_t))) {
            return LOGISAT_ERR_OUT_OF_MEMORY;
        }
        nl->primary_inputs[nl->num_primary_inputs++] = id;
    }
    if (is_out) {
        if (!grow_arr((void**)&nl->primary_outputs, &nl->po_capacity, nl->num_primary_outputs, sizeof(int32_t))) {
            return LOGISAT_ERR_OUT_OF_MEMORY;
        }
        nl->primary_outputs[nl->num_primary_outputs++] = id;
    }
    if (out_id) *out_id = id;
    return LOGISAT_OK;
}

logisat_status_t netlist_find_net(const netlist_t *nl, const char *name, int32_t *out_id) {
    if (!nl || !name) return LOGISAT_ERR_NULL_PTR;
    for (size_t i = 0; i < nl->num_nets; ++i) {
        if (strcmp(nl->nets[i].name, name) == 0) {
            if (out_id) *out_id = nl->nets[i].id;
            return LOGISAT_OK;
        }
    }
    return LOGISAT_ERR_INVALID_VARIABLE;
}

const char *netlist_get_net_name(const netlist_t *nl, int32_t net_id) {
    if (!nl || net_id <= 0 || (size_t)net_id > nl->num_nets) return "?";
    return nl->nets[net_id - 1].name;
}

logisat_status_t netlist_add_gate(netlist_t *nl, gate_type_t type, int32_t out_net, const int32_t *inputs, size_t n_in) {
    if (!nl || (!inputs && n_in > 0)) return LOGISAT_ERR_NULL_PTR;
    if (out_net <= 0 || (size_t)out_net > nl->num_nets) return LOGISAT_ERR_INVALID_VARIABLE;
    if (!grow_arr((void**)&nl->gates, &nl->gate_capacity, nl->num_gates, sizeof(circuit_gate_t))) {
        return LOGISAT_ERR_OUT_OF_MEMORY;
    }

    circuit_gate_t *g = &nl->gates[nl->num_gates++];
    g->type = type;
    g->output_net = out_net;
    g->num_inputs = n_in;
    g->input_nets = (int32_t *)malloc(sizeof(int32_t) * (n_in > 0 ? n_in : 1));
    if (!g->input_nets) return LOGISAT_ERR_OUT_OF_MEMORY;
    for (size_t i = 0; i < n_in; ++i) {
        g->input_nets[i] = inputs[i];
    }
    return LOGISAT_OK;
}

static bool dfs_has_cycle(int32_t node, const netlist_t *nl, int *state) {
    state[node] = 1;

    for (size_t i = 0; i < nl->num_gates; ++i) {
        const circuit_gate_t *g = &nl->gates[i];
        bool is_in = false;
        for (size_t j = 0; j < g->num_inputs; ++j) {
            if (g->input_nets[j] == node) {
                is_in = true;
                break;
            }
        }
        if (is_in) {
            int32_t next = g->output_net;
            if (state[next] == 1) return true;
            if (state[next] == 0 && dfs_has_cycle(next, nl, state)) return true;
        }
    }

    state[node] = 2;
    return false;
}

logisat_status_t netlist_validate_acyclic(const netlist_t *nl) {
    if (!nl) return LOGISAT_ERR_NULL_PTR;
    if (nl->num_nets == 0) return LOGISAT_OK;

    int *state = (int *)calloc(nl->num_nets + 1, sizeof(int));
    if (!state) return LOGISAT_ERR_OUT_OF_MEMORY;

    for (size_t i = 1; i <= nl->num_nets; ++i) {
        if (state[i] == 0 && dfs_has_cycle((int32_t)i, nl, state)) {
            free(state);
            return LOGISAT_ERR_CYCLIC_NETLIST;
        }
    }

    free(state);
    return LOGISAT_OK;
}

netlist_t *netlist_clone(const netlist_t *src) {
    if (!src) return NULL;
    netlist_t *copy = netlist_create(src->name);
    if (!copy) return NULL;
    for (size_t i = 0; i < src->num_nets; ++i) {
        int32_t id = 0;
        netlist_add_net(copy, src->nets[i].name, src->nets[i].is_primary_input, src->nets[i].is_primary_output, &id);
    }
    for (size_t i = 0; i < src->num_gates; ++i) {
        netlist_add_gate(copy, src->gates[i].type, src->gates[i].output_net, src->gates[i].input_nets, src->gates[i].num_inputs);
    }
    return copy;
}
