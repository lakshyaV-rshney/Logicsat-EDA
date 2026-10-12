#ifndef LOGISAT_VERIFIER_H
#define LOGISAT_VERIFIER_H

#include "netlist.h"
#include "cnf.h"
#include "solver.h"
#include "logisat_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool is_equivalent;
    bool solved;
    solver_stats_t stats;
    
    /* In case of difference / inequivalence, counterexample input vector */
    size_t num_inputs;
    char **input_names;
    var_value_t *counterexample_inputs;
} cec_result_t;

typedef struct {
    bool is_testable;
    bool solved;
    solver_stats_t stats;
    
    /* In case testable, input test pattern vector */
    size_t num_inputs;
    char **input_names;
    var_value_t *test_vector;
} atpg_result_t;

/**
 * @brief Performs Combinational Equivalence Checking (CEC) between two netlists.
 */
logisat_status_t verifier_check_equivalence(const netlist_t *nl1,
                                            const netlist_t *nl2,
                                            const solver_config_t *config,
                                            cec_result_t *out_result);

void verifier_free_cec_result(cec_result_t *res);

/**
 * @brief Automatic Test Pattern Generation for a Single Stuck-At Fault on a designated net.
 */
logisat_status_t verifier_generate_test_pattern(const netlist_t *nl,
                                                const char *target_net,
                                                fault_type_t fault_type,
                                                const solver_config_t *config,
                                                atpg_result_t *out_result);

void verifier_free_atpg_result(atpg_result_t *res);

#ifdef __cplusplus
}
#endif

#endif /* LOGISAT_VERIFIER_H */
