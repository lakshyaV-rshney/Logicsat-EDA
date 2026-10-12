# Architecture & Detailed Design Document (ARD)
## LogiSAT: Digital Logic Circuit Equivalence & Fault Verifier

---

## 1. System Architecture Overview

LogiSAT follows a strictly layered, decoupled modular architecture. High-level verification workflows (Equivalence Checking and ATPG Fault Analysis) are decoupled from the underlying low-level Boolean Satisfiability (SAT) solving engine.

```
+-----------------------------------------------------------------------+
|                         CLI / User Interface                         |
|      (Command-line Flag Parsing, Batch Mode, Interactive Menus)       |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
|                      Verification Engine Layer                        |
|  - Miter Circuit Generator (Equivalence Checking: C1 XOR C2 -> OR)    |
|  - Stuck-At Fault Injector (ATPG: s-a-0 / s-a-1 Modeling)             |
|  - Counterexample / Test Vector Extractor                             |
+-----------------------------------------------------------------------+
                 |                                      |
                 v                                      v
+-------------------------------+      +-------------------------------+
|     Netlist & AST Module      |      |   Tseytin Transform Engine    |
|  - Structural Netlist Parser  |----->|  - Linear-size CNF Encoder    |
|  - Gate Graph & Topology      |      |  - Auxiliary Variable Manager |
|  - Acyclic Cycle Checker      |      |  - Gate-to-Clause Synthesizer |
+-------------------------------+      +-------------------------------+
                                                        |
                                                        v
+-----------------------------------------------------------------------+
|                       DPLL SAT Engine Core                            |
|  - CNF Data Structures (Formula, Clause, Literal, VariableState)      |
|  - Boolean Constraint Propagation (BCP / Fast Unit Propagation)       |
|  - Pure Literal Elimination Rule                                      |
|  - Backtracking Decision Stack & Heuristics (DLIS / MOMs / First-Fit) |
|  - Solver Statistics (Decisions, Backtracks, Propagations, Runtime)   |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
|                         File I/O & Reports                            |
|  - DIMACS CNF Exporter / Importer (.cnf)                              |
|  - Formal Verification Certificate & Timing Reports (.rep)            |
+-----------------------------------------------------------------------+
```

---

## 2. Module Decomposition & Responsibilities

| Module | Header / Source | Primary Responsibility |
| :--- | :--- | :--- |
| **Common / Types** | `include/logisat_types.h`<br>`include/logisat_status.h` | Global status enums, error codes, boolean states, type definitions. |
| **Netlist AST** | `include/netlist.h`<br>`src/netlist.c` | Netlist graph representation: gates, input/output nets, topological sorting, acyclicity validation. |
| **Netlist Parser** | `include/parser.h`<br>`src/parser.c` | Robust file tokenizer and parser for circuit netlist files; strict line-by-line syntax checking. |
| **Tseytin Encoder** | `include/tseytin.h`<br>`src/tseytin.c` | Converts logic gates (AND, OR, NOT, NAND, NOR, XOR, XNOR) into equisatisfiable CNF clauses with auxiliary variables. |
| **CNF Representation** | `include/cnf.h`<br>`src/cnf.c` | Dynamically allocated CNF formula, clause lists, literal indexing, and DIMACS format export/import. |
| **DPLL Solver Core** | `include/solver.h`<br>`src/solver.c` | State-of-the-art DPLL engine: variable assignment stack, BCP unit propagation, backtracking, and heuristics. |
| **Circuit Verifier** | `include/verifier.h`<br>`src/verifier.c` | High-level orchestrator: Miter circuit generation for CEC, stuck-at fault injection for ATPG. |
| **Reporter** | `include/reporter.h`<br>`src/reporter.c` | Formats execution summaries, proofs of equivalence, counterexample vectors, and timing statistics. |

---

## 3. Data Design & Core Data Structures

### 3.1 Status & Value Enums
```c
typedef enum {
    LOGISAT_OK = 0,
    LOGISAT_ERR_NULL_PTR = 1,
    LOGISAT_ERR_OUT_OF_MEMORY = 2,
    LOGISAT_ERR_IO = 3,
    LOGISAT_ERR_PARSE_SYNTAX = 4,
    LOGISAT_ERR_CYCLIC_NETLIST = 5,
    LOGISAT_ERR_INVALID_VARIABLE = 6,
    LOGISAT_ERR_INVALID_CLAUSE = 7,
    LOGISAT_ERR_UNSUPPORTED_GATE = 8
} logisat_status_t;

typedef enum {
    VAL_UNASSIGNED = 0,
    VAL_FALSE = -1,
    VAL_TRUE = 1
} var_value_t;

typedef enum {
    SOLVER_SAT = 10,
    SOLVER_UNSAT = 20,
    SOLVER_UNDETERMINED = 30
} solver_result_t;

typedef enum {
    GATE_BUF,
    GATE_NOT,
    GATE_AND,
    GATE_OR,
    GATE_NAND,
    GATE_NOR,
    GATE_XOR,
    GATE_XNOR
} gate_type_t;
```

### 3.2 CNF & Solver Structures
* **Literal Representation**: Literals are represented as signed 32-bit integers (`int32_t`).
  * Variable $v > 0$ represents positive literal $+v$.
  * Literal $-v$ represents negative literal $\neg v$.
* **Clause Structure**:
```c
typedef struct {
    int32_t *literals;      /* Array of signed literal IDs */
    size_t size;            /* Number of literals in clause */
    size_t capacity;        /* Allocated capacity */
} clause_t;

typedef struct {
    clause_t **clauses;     /* Array of pointers to clauses */
    size_t num_clauses;     /* Clause count */
    size_t capacity;        /* Allocated capacity */
    size_t num_vars;        /* Total number of distinct variables */
} cnf_formula_t;

typedef struct {
    var_value_t *assignments; /* Array of variable values indexed 1..num_vars */
    size_t num_vars;
    
    /* Solver Performance Metrics */
    uint64_t num_decisions;
    uint64_t num_backtracks;
    uint64_t num_unit_propagations;
    double solve_time_ms;
} solver_state_t;
```

### 3.3 Netlist Graph Structures
```c
typedef struct {
    char name[64];
    int32_t net_id;
    bool is_primary_input;
    bool is_primary_output;
} net_t;

typedef struct {
    gate_type_t type;
    int32_t output_net;
    int32_t *input_nets;
    size_t num_inputs;
} gate_t;

typedef struct {
    net_t *nets;
    size_t num_nets;
    gate_t *gates;
    size_t num_gates;
    int32_t *primary_inputs;
    size_t num_primary_inputs;
    int32_t *primary_outputs;
    size_t num_primary_outputs;
} netlist_t;
```

---

## 4. Algorithmic Specifications

### 4.1 Tseytin Transformation (Linear-Size CNF Generation)
For each gate with output $z$ and inputs $a, b$:
* **NOT Gate** ($z = \neg a$):
  $$ (z \lor a) \land (\neg z \lor \neg a) $$
* **AND Gate** ($z = a \land b$):
  $$ (\neg a \lor \neg b \lor z) \land (a \lor \neg z) \land (b \lor \neg z) $$
* **OR Gate** ($z = a \lor b$):
  $$ (a \lor b \lor \neg z) \land (\neg a \lor z) \land (\neg b \lor z) $$
* **XOR Gate** ($z = a \oplus b$):
  $$ (\neg a \lor \neg b \lor \neg z) \land (a \lor b \lor \neg z) \land (a \lor \neg b \lor z) \land (\neg a \lor b \lor z) $$

### 4.2 DPLL Solver Engine (With Modular Optimization Hooks)
```
function DPLL(formula, assignment):
    // 1. Boolean Constraint Propagation (BCP)
    loop:
        if formula contains an empty/conflicting clause:
            return UNSAT
        if formula contains a unit clause (L):
            assign L = TRUE in assignment
            simplify formula with L
            continue
        break

    // 2. Pure Literal Elimination
    for each pure literal P in formula:
        assign P = TRUE in assignment
        simplify formula with P

    // 3. Termination Checks
    if formula has no remaining clauses:
        return SAT (Valid assignment found)

    // 4. Branching Decision (Heuristic: First-Fit / DLIS / MOM)
    choose unassigned variable V
    
    // 5. Backtracking Search
    // Try V = TRUE
    if DPLL(formula with V=TRUE, assignment) == SAT:
        return SAT
        
    // Try V = FALSE
    return DPLL(formula with V=FALSE, assignment)
```

---

## 5. Optimization Roadmap & Extensibility Strategy

The engine is engineered with strict modularity to allow clean, plug-and-play algorithmic upgrades:

1. **Phase 1 (Baseline)**: Classic Recursive DPLL with static clause reduction and dynamic assignment array.
2. **Phase 2 (Fast BCP & Unit Propagation)**:
   * Optimized literal lookup table / occurrence list per variable.
   * Clause evaluation caching.
3. **Phase 3 (Two Watched Literals - 2WL)**:
   * Replaces explicit formula copying/modification with Chaff/MiniSat-style Two Watched Literals, reducing propagation cost from $\mathcal{O}(\text{total literals})$ to $\mathcal{O}(\text{occurrences of falsified literal})$.
4. **Phase 4 (Variable Selection Heuristics)**:
   * Dynamic Largest Individual Sum (DLIS).
   * Maximum Occurrences in clauses of Minimum size (MOM's heuristic).
   * Conflict-history / Activity decay (VSIDS-lite).

---

## 6. Verification & Google Test (gTest) Test Strategy

The test suite will reside in `test/` and run automatically via `make test`:
* **`test_parser.cpp`**: Netlist parsing, valid token recognition, syntax error rejection, cycle detection.
* **`test_cnf.cpp`**: CNF creation, clause addition, clause destruction, memory leak checks.
* **`test_tseytin.cpp`**: Verification of generated CNF clauses against gate truth tables for all 8 gate types.
* **`test_solver.cpp`**: 
  * Unit propagation verification.
  * Known SAT instances (Horn clauses, 3-SAT satisfiable).
  * Known UNSAT instances (Pigeonhole principle, $A \land \neg A$).
  * Random 3-SAT benchmarks at phase transition ($4.26$ clause-to-variable ratio).
* **`test_verifier.cpp`**:
  * Functional equivalence of De Morgan's laws ($ \neg(A \land B) \equiv \neg A \lor \neg B $).
  * Functional equivalence of XOR implementations ($ (A \lor B) \land \neg(A \land B) \equiv A \oplus B $).
  * Detection of injected stuck-at-0 and stuck-at-1 faults with counterexample extraction.
