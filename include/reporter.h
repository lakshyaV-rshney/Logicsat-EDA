// reporter.h - Verification reporting utilities
#ifndef LOGISAT_REPORTER_H
#define LOGISAT_REPORTER_H

#include "netlist.h"
#include "verifier.h"
#include "cnf.h"
#include "logisat_status.h"

logisat_status_t reporter_write_cec_report(const char *filepath,
                                           const netlist_t *nl1,
                                           const netlist_t *nl2,
                                           const cec_result_t *res);

logisat_status_t reporter_write_atpg_report(const char *filepath,
                                            const netlist_t *nl,
                                            const char *target_net,
                                            fault_type_t fault_type,
                                            const atpg_result_t *res);

logisat_status_t reporter_write_dimacs_summary(const char *filepath,
                                               const char *dimacs_file,
                                               const cnf_formula_t *formula,
                                               solver_result_t res,
                                               const solver_stats_t *stats,
                                               const var_value_t *assignments);

#endif
