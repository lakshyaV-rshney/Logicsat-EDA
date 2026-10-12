# Product Requirements Document (PRD) / Project Proposal
## Digital Logic Circuit Equivalence & Stuck-At Fault Verifier (LogiSAT-EDA)

---

## 1. Project Information
* **Project Title**: LogiSAT: Automated Digital Circuit Equivalence & Stuck-At Fault Verifier Powered by a DPLL SAT Engine
* **Engineering Domain**: Electronic Design Automation (EDA) / Semiconductor Hardware Verification
* **Course**: Programming Fundamentals and Software Engineering (Sem 3)
* **Implementation Language**: C (ISO C99 / C11)
* **Target Build & Test Automation**: GNU Make, Google Test (gTest)

---

## 2. Engineering Scenario

### 2.1 Scenario Description
In semiconductor design and Electronic Design Automation (EDA), hardware description designs (such as combinational logic blocks, ALUs, and multiplexers) undergo synthesis and gate-level optimizations. During these transformations, two critical engineering challenges occur:
1. **Combinational Equivalence Checking (CEC)**: Guaranteeing that an optimized gate-level netlist is mathematically identical in behavior to the golden reference specification for all possible input combinations ($2^N$ test vectors).
2. **Manufacturing Fault Analysis (Stuck-At Fault Testing)**: In physical silicon fabrication, hardware defects (such as short circuits to ground or power lines) manifest as physical nets permanently stuck at logical `0` (s-a-0) or logical `1` (s-a-1). Automatic Test Pattern Generation (ATPG) is required to determine whether a fault is testable and find a specific binary input vector that propagates the defect to an observable primary output.

Currently, brute-force simulation (truth tables) scales exponentially ($\mathcal{O}(2^N)$), rendering circuits with more than 20 inputs practically unverifiable via exhaustive testing.

### 2.2 Problem Statement
In **semiconductor chip design and hardware verification**, **digital design and test engineers** currently face **exponential verification bottlenecks when verifying optimized gate netlists and finding physical fault detection vectors**. The current naive simulation method results in **unacceptable compute latency, missed corner-case design bugs, and undetected fabrication defects**. The proposed software, **LogiSAT**, will provide **gate-level netlist parsing, Tseytin-transformation-based CNF translation, an optimized DPLL Boolean Satisfiability (SAT) solver engine, and automated fault/miter test pattern generation** so that users can **formally prove combinational circuit equivalence and generate stuck-at fault test patterns in polynomial-to-sub-exponential average time**.

### 2.3 Proposed Solution
**LogiSAT** is a modular, high-performance C command-line verification utility that:
1. Parses standard structural gate-level netlist files describing inputs, outputs, and logic gates (`AND`, `OR`, `NOT`, `NAND`, `NOR`, `XOR`, `XNOR`).
2. Implements a **Miter circuit generator** for equivalence checking (joining two circuits with XOR gates at outputs feed into an OR gate).
3. Implements **Stuck-At Fault injection** on target nets (`s-a-0` and `s-a-1`).
4. Employs the **Tseytin Transformation** to translate the circuit and its fault conditions into Conjunctive Normal Form (CNF) with linear formula growth $\mathcal{O}(N)$.
5. Solves the resulting SAT instances using a modular **DPLL (Davis-Putnam-Logemann-Loveland) Engine** featuring recursive backtracking, Boolean Constraint Propagation (BCP / Unit Propagation), and Pure Literal Elimination.
6. Reports mathematical proof of equivalence (UNSAT) or generates the exact diagnostic counterexample/test-vector (SAT).

---

## 3. Target Users

| User Type | Responsibility | How the User Will Use the Application |
| :--- | :--- | :--- |
| **Digital Logic Designer** | Designs and optimizes gate-level combinational modules. | Ingests reference and optimized netlist files to formally verify functional equivalence before tape-out. |
| **DFT (Design-for-Test) Engineer** | Generates manufacturing test patterns for automatic test equipment (ATE). | Injects stuck-at-0 and stuck-at-1 faults on specific internal circuit wires and extracts input test vectors that expose faults. |
| **Verification & Quality Auditor** | Inspects test coverage and functional correctness proofs. | Generates automated diagnostic reports and compliance certificates from CI/Makefile pipelines. |

---

## 4. Project Scope

### 4.1 In-Scope Features
1. **Netlist Parsing**: Ingestion and strict syntax validation of structural gate-level netlists with cycle detection (combinational feed-forward acyclic verification).
2. **Logic Gate Coverage**: Support for 2-input and multi-input `AND`, `OR`, `NAND`, `NOR`, `XOR`, `XNOR`, and single-input `NOT` / `BUF` gates.
3. **Tseytin Transformation Engine**: Dynamic generation of equisatisfiable CNF formulas using auxiliary Tseytin variables.
4. **Miter Construction (Equivalence Checking)**: Automated synthesis of miter circuits to verify equivalence between golden and synthesized netlists.
5. **Fault Injection & ATPG**: Modeling single stuck-at faults (SSAF) and generating satisfying test vectors.
6. **Core DPLL Solver**: Backtracking solver with:
   * Fast Unit Clause Detection & Boolean Constraint Propagation (BCP).
   * Pure Literal Elimination.
   * Backtracking decision stack.
7. **File I/O**: Direct export and import of standard DIMACS CNF files (`.cnf`) for standalone SAT benchmarking, and structured test report generation (`.rep` / `.txt`).
8. **Interactive CLI & Batch Mode**: Command-line interface supporting single-run flags and interactive menus for engineer diagnostics.

### 4.2 Out-of-Scope Features
1. Sequential logic elements (flip-flops, latches, clocked registers, and timing closures).
2. Transistor-level / analog SPICE simulations.
3. Graphical User Interface (GUI) (the tool strictly adheres to UNIX-style CLI and automated Make pipelines).

### 4.3 Assumptions & Constraints
* **Assumptions**: Combinational circuits are strictly directed acyclic graphs (DAGs). Fan-out is supported; feedback loops are flagged as illegal.
* **Constraints**: 
  * Written in strict ANSI/ISO C (C99/C11 standards compliant).
  * No external non-standard runtime dependencies (standard C runtime library only for core solver).
  * Build automation restricted to standard POSIX `make`.
  * Unit tests integrated with Google Test (gTest) using C++ wrapper linkage (`extern "C"`).

---

## 5. Requirements

### 5.1 Functional Requirements (FR)

| ID | Requirement Description | Priority | Verification Method |
| :--- | :--- | :--- | :--- |
| **FR-01** | The system shall parse and validate structural gate netlist files, verifying valid port definitions and gate declarations. | High | gTest / Unit test |
| **FR-02** | The system shall detect syntax errors, duplicate net names, and cyclic dependencies in netlist inputs without crashing. | High | Negative test cases |
| **FR-03** | The system shall generate equisatisfiable CNF clauses using Tseytin transformation for all supported gate types. | High | Unit test against truth tables |
| **FR-04** | The system shall construct a Miter equivalence circuit between two compatible netlist models. | High | Integration demonstration |
| **FR-05** | The system shall inject stuck-at-0 and stuck-at-1 faults on designated circuit nets. | High | Test vector validation |
| **FR-06** | The core SAT solver shall implement recursive DPLL with unit propagation (BCP) and pure literal rule. | High | Standard DIMACS benchmark tests |
| **FR-07** | The system shall return `UNSAT` when two circuits are equivalent and `SAT` with a distinguishing input vector when inequivalent. | High | Equivalence test suite |
| **FR-08** | The system shall output standard DIMACS CNF files from any netlist or miter circuit for external validation. | Medium | File comparison test |
| **FR-09** | The system shall generate a detailed diagnostic verification report detailing solve time, decisions, backtracks, and input vectors. | Medium | Demonstration & File I/O test |

### 5.2 Non-Functional Requirements (NFR)

| ID | Non-Functional Requirement | Verification Method |
| :--- | :--- | :--- |
| **NFR-01** | The codebase shall compile with zero warnings using `-Wall -Wextra -pedantic` in GCC/Clang. | Compiler build output |
| **NFR-02** | The application shall maintain a modular architecture with clean separation of parser, transform, solver, and CLI. | Architectural code review |
| **NFR-03** | Core algorithms shall be verified using Google Test (gTest) with high branch and statement coverage. | gTest test runner execution |
| **NFR-04** | Memory safety: Dynamic allocations must have deterministic deallocation with zero memory leaks under Valgrind/ASan. | Valgrind / ASan run |
| **NFR-05** | Build and test pipelines must be fully automated via a single root `Makefile` (`make`, `make test`, `make clean`). | Automated build verification |
| **NFR-06** | All public C API functions must return explicit status/error enumerations (`logisat_status_t`) without abrupt program termination. | Defensive API unit tests |
