// logisat_status.h - Status and error codes
#ifndef LOGISAT_STATUS_H
#define LOGISAT_STATUS_H

typedef enum {
    LOGISAT_OK                      = 0,
    LOGISAT_ERR_NULL_PTR            = 1,
    LOGISAT_ERR_OUT_OF_MEMORY       = 2,
    LOGISAT_ERR_FILE_IO             = 3,
    LOGISAT_ERR_PARSE_SYNTAX        = 4,
    LOGISAT_ERR_CYCLIC_NETLIST      = 5,
    LOGISAT_ERR_INVALID_VARIABLE    = 6,
    LOGISAT_ERR_INVALID_CLAUSE      = 7,
    LOGISAT_ERR_UNSUPPORTED_GATE    = 8,
    LOGISAT_ERR_DIMACS_FORMAT       = 9,
    LOGISAT_ERR_NETLIST_INCOMPATIBLE = 10
} logisat_status_t;

static inline const char *logisat_status_to_string(logisat_status_t status) {
    switch (status) {
        case LOGISAT_OK:
            return "Operation completed successfully";
        case LOGISAT_ERR_NULL_PTR:
            return "Null pointer argument encountered";
        case LOGISAT_ERR_OUT_OF_MEMORY:
            return "Out of memory / allocation failure";
        case LOGISAT_ERR_FILE_IO:
            return "File input/output error";
        case LOGISAT_ERR_PARSE_SYNTAX:
            return "Syntax error during parsing";
        case LOGISAT_ERR_CYCLIC_NETLIST:
            return "Cyclic combinational feedback loop detected";
        case LOGISAT_ERR_INVALID_VARIABLE:
            return "Invalid variable identifier";
        case LOGISAT_ERR_INVALID_CLAUSE:
            return "Malformed clause structure";
        case LOGISAT_ERR_UNSUPPORTED_GATE:
            return "Unsupported or unrecognized logic gate type";
        case LOGISAT_ERR_DIMACS_FORMAT:
            return "Invalid DIMACS CNF format";
        case LOGISAT_ERR_NETLIST_INCOMPATIBLE:
            return "Netlists have mismatched I/O interfaces";
        default:
            return "Unknown error";
    }
}

#endif
