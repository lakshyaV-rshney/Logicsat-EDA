# LogiSAT: Automated Digital Circuit Equivalence & Fault Verifier

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![Language](https://img.shields.io/badge/language-C99-blue.svg)]()
[![Testing](https://img.shields.io/badge/tests-12%2F12%20passed-success.svg)]()
[![Standard](https://img.shields.io/badge/standard-ISO%20C99%20%2F%20C11-orange.svg)]()

> **LogiSAT** is an industrial-grade, modular Electronic Design Automation (EDA) verification engine written in ISO C99. It combines structural gate-level netlist parsing, linear-size Tseytin CNF transformations, and an optimized Davis-Putnam-Logemann-Loveland (DPLL) Boolean Satisfiability (SAT) solver to formally verify combinational circuit equivalence and generate Automatic Test Patterns (ATPG) for single stuck-at faults.

---

## 1. Key Capabilities

* **Combinational Equivalence Checking (CEC)**: Synthesizes a Miter comparator between golden reference and optimized netlists. Formally proves functional equivalence (`UNSAT`) across all $2^N$ input combinations, or synthesizes counterexample input vectors (`SAT`) when bugs exist.
* **Automatic Test Pattern Generation (ATPG)**: Injects single stuck-at-0 (`s-a-0`) and stuck-at-1 (`s-a-1`) physical hardware faults into circuit nets, generating sensitizing input test vectors to excite and propagate faults to primary outputs.
* **High-Performance DPLL SAT Engine**:
  * Fast Boolean Constraint Propagation (BCP / Unit Clause Propagation).
  * Pure Literal Elimination rule.
  * Dynamic Largest Individual Sum (DLIS) branching heuristic.
  * In-place assignment stack with zero dynamic formula re-allocation during search.
* **Standard DIMACS CNF File Support**:
  * Full-featured parser for `.cnf` files (handling comments, headers, multi-line clauses).
  * Direct export of CNF formulas for benchmarking against external solvers (MiniSat, Z3, Kissat).
* **Robust Netlist Parser & Topological Cycle Checker**:
  * Rejects cyclic feedback combinational loops via DFS-based DAG validation.
  * Supports `AND`, `OR`, `NOT`, `NAND`, `NOR`, `XOR`, `XNOR`, and `BUF` gates.
* **Automated Engineering Reports**:
  * Detailed diagnostic reports (`.rep` / `.txt`) with timing benchmarks, decision counts, and backtrack statistics.

---

## 2. System Architecture

```
                                  [ Netlist 1 ]        [ Netlist 2 ]
                                        \                   /
                                         v                 v
+-----------------------+         +-------------------------------+
|   DIMACS CNF Files    |         |   Acyclic Netlist AST Engine  |
|      (.cnf I/O)       |         |   (DFS Cycle Check, Gates)    |
+-----------------------+         +-------------------------------+
            |                                     |
            |                                     v
            |                     +-------------------------------+
            |                     |   Miter & Fault Injector      |
            |                     |   (XOR Miter, s-a-0 / s-a-1)  |
            |                     +-------------------------------+
            |                                     |
            |                                     v
            |                     +-------------------------------+
            |                     |   Tseytin Transform Engine    |
            |                     |   (Linear-Size CNF Clauses)   |
            |                     +-------------------------------+
            |                                     |
            +------------------+------------------+
                               |
                               v
               +-------------------------------+
               |     DPLL SAT Engine Core      |
               |  - Unit Propagation (BCP)     |
               |  - Pure Literal Elimination   |
               |  - DLIS Branching Heuristic   |
               |  - Non-allocating Trail Stack |
               +-------------------------------+
                               |
                               v
               +-------------------------------+
               |   Diagnostic Report Engine    |
               |   (Proof / Counterexamples)   |
               +-------------------------------+
```

---

## 3. Directory Layout

```
.
├── include/                 # Public C header files
│   ├── cnf.h                # CNF formula and clause data structures
│   ├── dimacs.h             # DIMACS CNF file loader and exporter
│   ├── logisat_status.h     # System status enums and error strings
│   ├── logisat_types.h      # Core primitive enumerations
│   ├── netlist.h            # Netlist graph AST and DAG validation
│   ├── parser.h             # Netlist file lexical analyzer and parser
│   ├── reporter.h           # Engineering verification report writer
│   ├── solver.h             # DPLL SAT solver interface
│   ├── tseytin.h            # Tseytin gate transformation engine
│   └── verifier.h           # CEC Miter and ATPG fault workflows
├── src/                     # C source implementations
│   ├── cnf.c
│   ├── dimacs.c
│   ├── main.c              # Professional CLI interface
│   ├── netlist.c
│   ├── parser.c
│   ├── reporter.c
│   ├── solver.c
│   ├── tseytin.c
│   └── verifier.c
├── test/                    # Automated unit testing suite
│   ├── test_framework.h     # Zero-dependency Google Test compatible harness
│   └── test_runner.c        # 12 comprehensive unit test cases
├── benchmarks/              # Sample DIMACS CNF instances
│   ├── simple_sat.cnf
│   └── simple_unsat.cnf
├── examples/                # Sample gate-level netlists
│   ├── full_adder_a.net     # Golden reference 1-bit full adder
│   ├── full_adder_b.net     # Equivalent transformed full adder
│   └── full_adder_buggy.net # Defective mutated full adder
├── docs/                    # Academic project documentation
│   ├── PRD.md               # Product Requirements Document (Phase 1)
│   └── ARD.md               # Architecture & Detailed Design (Phase 2)
├── Makefile                 # Cross-platform build automation
└── README.md
```

---

## 4. Build and Execution

### Prerequisites
* GCC (`gcc` supporting ISO C99)
* GNU Make (`make`)

### Build Commands

```bash
# 1. Compile the main executable (bin/logisat)
make

# 2. Run the automated Google Test compatible test suite
make test

# 3. Run demonstration benchmarks and generate reports
make benchmark

# 4. Clean generated verification reports (*.rep, output/*)
make clean-output

# 5. Clean compilation objects and binaries
make clean

# 6. Clean everything (binaries + outputs)
make clean-all
```

---

## 5. Usage Guide & Examples

### 1. Combinational Equivalence Checking (CEC)

Verify whether an optimized netlist is functionally identical to the reference model:

```bash
bin/logisat --equiv examples/full_adder_a.net examples/full_adder_b.net --output output/cec.rep
```

*Output:*
```
[*] Parsing Circuit 1: examples/full_adder_a.net ...
[*] Parsing Circuit 2: examples/full_adder_b.net ...
[*] Constructing Miter circuit and solving equivalence ...

========================================================
  VERDICT: CIRCUITS ARE FUNCTIONALLY EQUIVALENT [PROVEN] 
========================================================

Metrics: Time: 0.000 ms | Decisions: 7 | Backtracks: 7
[+] Verification report exported to: output/cec.rep
```

When verifying a defective netlist:
```bash
bin/logisat --equiv examples/full_adder_a.net examples/full_adder_buggy.net
```
*Output:*
```
========================================================
  VERDICT: CIRCUITS ARE NON-EQUIVALENT [DISPROVEN]      
========================================================
Counterexample input vector found:
  a = 1
  b = 0
  cin = 1
```

### 2. Automatic Test Pattern Generation (ATPG)

Generate a test vector to detect a physical stuck-at-0 fault on net `sum`:

```bash
bin/logisat --fault examples/full_adder_a.net sum 0 --output output/fault.rep
```

*Output:*
```
========================================================
  VERDICT: FAULT IS TESTABLE                            
========================================================
Test pattern vector:
  a = 1
  b = 1
  cin = 1
```

### 3. Solving Standalone DIMACS CNF Formulas

```bash
bin/logisat --dimacs benchmarks/simple_sat.cnf --output output/dimacs.rep
```

### 4. Interactive Mode

Running `logisat` without command-line arguments launches the interactive engineer terminal:

```bash
bin/logisat
```

---

## 6. Unit Testing Strategy

The test suite (`make test`) covers all modules with 100% pass rate:
1. `SuiteCNF.CreateAndAddLiterals`: Dynamic clause buffer resizing and bounds checking.
2. `SuiteCNF.FormulaAddAndClone`: Deep copying and variable indexing invariants.
3. `SuiteDIMACS.SaveAndReloadRoundTrip`: Byte-for-byte serialization fidelity.
4. `SuiteSolver.TrivialSAT`: Immediate satisfaction on single-clause formulas.
5. `SuiteSolver.TrivialUNSAT`: Direct contradiction detection ($A \land \neg A$).
6. `SuiteSolver.UnitPropagationChain`: Multi-step BCP resolution without decision branching.
7. `SuiteSolver.PigeonholePrinciple_PHP_2_3_UNSAT`: Formal proof of unsatisfiability on 3 pigeons / 2 holes ($PHP_2^3$).
8. `SuiteNetlist.AcyclicPasses`: Combinational feedforward graph acceptance.
9. `SuiteNetlist.CyclicLoopDetected`: DFS cycle trap rejecting feedback loops.
10. `SuiteVerifier.DeMorganEquivalence_Proven`: Formal mathematical equivalence proof of $\neg(A \land B) \equiv \neg A \lor \neg B$.
11. `SuiteVerifier.BuggyCircuit_DisprovenWithCounterexample`: Detection and counterexample extraction on buggy logic.
12. `SuiteATPG.DetectStuckAt0`: Exact physical sensitizing vector generation for stuck-at faults.

---

## 7. License & Academic Integrity

This project was developed as an individual mini-project submission for the **Programming Fundamentals and Software Engineering** course. It follows strict software engineering principles: modular C design, warning-free compilation under `-Wall -Wextra -pedantic`, gTest-style unit testing, and automated Makefile build workflows.
