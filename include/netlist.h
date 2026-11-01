// netlist.h - Structural gate-level netlist data structure and validation
#ifndef LOGISAT_NETLIST_H
#define LOGISAT_NETLIST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "logisat_status.h"
#include "logisat_types.h"

#define MAX_NET_NAME_LEN 64

typedef struct {
    char name[MAX_NET_NAME_LEN];
    int32_t id;
    bool is_primary_input;
    bool is_primary_output;
} circuit_net_t;

typedef struct {
    gate_type_t type;
    int32_t output_net;
    int32_t *input_nets;
    size_t num_inputs;
} circuit_gate_t;

typedef struct {
    char name[128];
    circuit_net_t *nets;
    size_t num_nets;
    size_t net_capacity;

    circuit_gate_t *gates;
    size_t num_gates;
    size_t gate_capacity;

    int32_t *primary_inputs;
    size_t num_primary_inputs;
    size_t pi_capacity;

    int32_t *primary_outputs;
    size_t num_primary_outputs;
    size_t po_capacity;
} netlist_t;

netlist_t *netlist_create(const char *name);
void netlist_destroy(netlist_t *nl);
netlist_t *netlist_clone(const netlist_t *src);

logisat_status_t netlist_add_net(netlist_t *nl, const char *name, bool is_input, bool is_output, int32_t *out_id);
logisat_status_t netlist_find_net(const netlist_t *nl, const char *name, int32_t *out_id);
const char *netlist_get_net_name(const netlist_t *nl, int32_t net_id);

logisat_status_t netlist_add_gate(netlist_t *nl, gate_type_t type, int32_t output_net, const int32_t *inputs, size_t num_inputs);
logisat_status_t netlist_validate_acyclic(const netlist_t *nl);

const char *gate_type_to_string(gate_type_t type);
gate_type_t gate_type_from_string(const char *str);

#endif
