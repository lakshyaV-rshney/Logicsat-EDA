// logisat_types.h - Core enumerations and types
#ifndef LOGISAT_TYPES_H
#define LOGISAT_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    VAL_FALSE       = -1,
    VAL_UNASSIGNED  =  0,
    VAL_TRUE        =  1
} var_value_t;

typedef enum {
    SOLVER_SAT          = 10,
    SOLVER_UNSAT        = 20,
    SOLVER_UNDETERMINED = 30
} solver_result_t;

typedef enum {
    GATE_BUF = 0,
    GATE_NOT,
    GATE_AND,
    GATE_OR,
    GATE_NAND,
    GATE_NOR,
    GATE_XOR,
    GATE_XNOR
} gate_type_t;

typedef enum {
    FAULT_NONE = 0,
    FAULT_STUCK_AT_0,
    FAULT_STUCK_AT_1
} fault_type_t;

#endif
