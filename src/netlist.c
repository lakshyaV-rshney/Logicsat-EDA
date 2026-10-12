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
    if (strcmp(str, "BUF") == 0 || strcmp(str, "buf") == 0) return GATE_BUF;
    if (strcmp(str, "NOT") == 0 || strcmp(str, "not") == 0 || strcmp(str, "INV") == 0) return GATE_NOT;
    if (strcmp(str, "AND") == 0 || strcmp(str, "and") == 0) return GATE_AND;
    if (strcmp(str, "OR") == 0  || strcmp(str, "or") == 0)  return GATE_OR;
    if (strcmp(str, "NAND") == 0 || strcmp(str, "nand") == 0) return GATE_NAND;
    if (strcmp(str, "NOR") == 0  || strcmp(str, "nor") == 0)  return GATE_NOR;
    if (strcmp(str, "XOR") == 0  || strcmp(str, "xor") == 0)  return GATE_XOR;
    if (strcmp(str, "XNOR") == 0 || strcmp(str, "xnor") == 0) return GATE_XNOR;
    return GATE_BUF;
}

netlist_t *netlist_create(const char *name) {
    netlist_t *nl = (netlist_t *)malloc(sizeof(netlist_t));
    if (!nl) return NULL;

    strncpy(nl->name, name ? name : "circuit", sizeof(nl->name) - 1);
    nl->name[sizeof(nl->name) - 1] = '\0';

    nl->nets = (circuit_net_t *)malloc(sizeof(circuit_net_t) * 16);
    nl->num_nets = 0;
    nl->net_capacity = 16;

    nl->gates = (circuit_gate_t *)malloc(sizeof(circuit_gate_t) * 16);
    nl->num_gates = 0;
    nl->gate_capacity = 16;

    nl->primary_inputs = (int32_t *)malloc(sizeof(int32_t) * 8);
    nl->num_primary_inputs = 0;
    nl->pi_capacity = 8;

    nl->primary_outputs = (int32_t *)malloc(sizeof(int32_t) * 8);
    nl->num_primary_outputs = 0;
    nl->po_capacity = 8;

    if (!nl->nets || !nl->gates || !nl->primary_inputs || !nl->primary_outputs) {
        netlist_destroy(nl);
        return NULL;
    }

    return nl;
}

void netlist_destroy(netlist_t *nl) {
    if (!nl) return;
    if (nl->nets) {
        free(nl->nets);
        nl->nets = NULL;
    }
    if (nl->gates) {
        for (size_t i = 0; i < nl->num_gates; ++i) {
            if (nl->gates[i].input_nets) {
                free(nl->gates[i].input_nets);
            }
        }
        free(nl->gates);
        nl->gates = NULL;
    }
    if (nl->primary_inputs) {
        free(nl->primary_inputs);
        nl->primary_inputs = NULL;
    }
    if (nl->primary_outputs) {
        free(nl->primary_outputs);
        nl->primary_outputs = NULL;
    }
    free(nl);
}

logisat_status_t netlist_add_net(netlist_t *nl, const char *name, bool is_input, bool is_output, int32_t *out_id) {
    if (!nl || !name) return LOGISAT_ERR_NULL_PTR;

    /* Check if net already exists */
    for (size_t i = 0; i < nl->num_nets; ++i) {
        if (strcmp(nl->nets[i].name, name) == 0) {
            if (is_input) nl->nets[i].is_primary_input = true;
            if (is_output) nl->nets[i].is_primary_output = true;
            if (out_id) *out_id = nl->nets[i].id;
            return LOGISAT_OK;
        }
    }

    /* Grow nets array if needed */
    if (nl->num_nets >= nl->net_capacity) {
        size_t new_cap = nl->net_capacity * 2;
        circuit_net_t *new_nets = (circuit_net_t *)realloc(nl->nets, sizeof(circuit_net_t) * new_cap);
        if (!new_nets) return LOGISAT_ERR_OUT_OF_MEMORY;
        nl->nets = new_nets;
        nl->net_capacity = new_cap;
    }

    int32_t assigned_id = (int32_t)(nl->num_nets + 1);
    circuit_net_t *net = &nl->nets[nl->num_nets++];
    strncpy(net->name, name, sizeof(net->name) - 1);
    net->name[sizeof(net->name) - 1] = '\0';
    net->id = assigned_id;
    net->is_primary_input = is_input;
    net->is_primary_output = is_output;

    if (is_input) {
        if (nl->num_primary_inputs >= nl->pi_capacity) {
            size_t new_cap = nl->pi_capacity * 2;
            int32_t *new_pi = (int32_t *)realloc(nl->primary_inputs, sizeof(int32_t) * new_cap);
            if (!new_pi) return LOGISAT_ERR_OUT_OF_MEMORY;
            nl->primary_inputs = new_pi;
            nl->pi_capacity = new_cap;
        }
        nl->primary_inputs[nl->num_primary_inputs++] = assigned_id;
    }

    if (is_output) {
        if (nl->num_primary_outputs >= nl->po_capacity) {
            size_t new_cap = nl->po_capacity * 2;
            int32_t *new_po = (int32_t *)realloc(nl->primary_outputs, sizeof(int32_t) * new_cap);
            if (!new_po) return LOGISAT_ERR_OUT_OF_MEMORY;
            nl->primary_outputs = new_po;
            nl->po_capacity = new_cap;
        }
        nl->primary_outputs[nl->num_primary_outputs++] = assigned_id;
    }

    if (out_id) *out_id = assigned_id;
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

logisat_status_t netlist_add_gate(netlist_t *nl, gate_type_t type, int32_t output_net, const int32_t *inputs, size_t num_inputs) {
    if (!nl || (!inputs && num_inputs > 0)) return LOGISAT_ERR_NULL_PTR;
    if (output_net <= 0 || (size_t)output_net > nl->num_nets) return LOGISAT_ERR_INVALID_VARIABLE;

    if (nl->num_gates >= nl->gate_capacity) {
        size_t new_cap = nl->gate_capacity * 2;
        circuit_gate_t *new_gates = (circuit_gate_t *)realloc(nl->gates, sizeof(circuit_gate_t) * new_cap);
        if (!new_gates) return LOGISAT_ERR_OUT_OF_MEMORY;
        nl->gates = new_gates;
        nl->gate_capacity = new_cap;
    }

    circuit_gate_t *g = &nl->gates[nl->num_gates++];
    g->type = type;
    g->output_net = output_net;
    g->num_inputs = num_inputs;
    g->input_nets = (int32_t *)malloc(sizeof(int32_t) * (num_inputs > 0 ? num_inputs : 1));
    if (!g->input_nets) return LOGISAT_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < num_inputs; ++i) {
        g->input_nets[i] = inputs[i];
    }

    return LOGISAT_OK;
}

/* Cycle detection DFS */
static bool dfs_has_cycle(int32_t node, const netlist_t *nl, int *state) {
    state[node] = 1; /* visiting */

    /* Find gates whose input is this node */
    for (size_t i = 0; i < nl->num_gates; ++i) {
        const circuit_gate_t *g = &nl->gates[i];
        bool is_input_of_gate = false;
        for (size_t j = 0; j < g->num_inputs; ++j) {
            if (g->input_nets[j] == node) {
                is_input_of_gate = true;
                break;
            }
        }

        if (is_input_of_gate) {
            int32_t next = g->output_net;
            if (state[next] == 1) return true; /* Cycle found! */
            if (state[next] == 0 && dfs_has_cycle(next, nl, state)) return true;
        }
    }

    state[node] = 2; /* visited */
    return false;
}

logisat_status_t netlist_validate_acyclic(const netlist_t *nl) {
    if (!nl) return LOGISAT_ERR_NULL_PTR;
    if (nl->num_nets == 0) return LOGISAT_OK;

    int *state = (int *)calloc(nl->num_nets + 1, sizeof(int));
    if (!state) return LOGISAT_ERR_OUT_OF_MEMORY;

    for (size_t i = 1; i <= nl->num_nets; ++i) {
        if (state[i] == 0) {
            if (dfs_has_cycle((int32_t)i, nl, state)) {
                free(state);
                return LOGISAT_ERR_CYCLIC_NETLIST;
            }
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
