#ifndef LOGISAT_REPORTER_H
#define LOGISAT_REPORTER_H

#include "netlist.h"
#include "verifier.h"
#include "cnf.h"
#include "logisat_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generates an engineering verification report for Combinational Equivalence Checking.
 */
logisat_status_t reporter_write_cec_report(const char *filepath,
                                           const netlist_t *nl1,
                                           const netlist_t *nl2,
                                           const cec_result_t *res);

/**
 * @brief Generates an engineering ATPG stuck-at fault test pattern report.
 */
logisat_status_t reporter_write_atpg_report(const char *filepath,
                                            const netlist_t *nl,
                                            const char *target_net,
                                            fault_type_t fault_type,
                                            const atpg_result_t *res);

/**
 * @brief Generates a DIMACS CNF benchmark evaluation report.
 */
logisat_status_t reporter_write_dimacs_summary(const char *filepath,
                                               const char *dimacs_file,
                                               const cnf_formula_t *formula,
                                               solver_result_t res,
                                               const solver_stats_t *stats,
                                               const var_value_t *assignments);

#ifdef __cplusplus
}
#endif

#endif /* LOGISAT_REPORTER_H */
