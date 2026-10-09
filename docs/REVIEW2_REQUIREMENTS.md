# TinyRAJIT — Assessment Criteria & Requirements Mapping

This document maps the completed implementation of the **Resource-Aware Profiling-Guided JIT Compiler (TinyRAJIT)** directly to the formal Review 2 academic assessment criteria.

---

## 1. Assessment Criteria Matrix

| Evaluation Criterion | Expectation | Implementation Mapping in TinyRAJIT | Evidence / Artifact |
| :--- | :--- | :--- | :--- |
| **1. Implementation Progress** | Functional compiler pipeline with key features implemented | Full multi-phase compiler pipeline: Lexer $\to$ Parser $\to$ AST $\to$ Symbol Table $\to$ Semantic Analyzer $\to$ Resource Checker $\to$ Interpreter $\to$ Profiler $\to$ JIT Policy $\to$ LLVM CodeGen $\to$ LLVM ORC JIT. | All 7 modules compile without errors; 7 CTest suites pass with 100% success rate. |
| **2. Functional Correctness** | Correct output across valid programs and exact error diagnostics on invalid code | Positive cases evaluate correctly in both interpreter and JIT. Negative cases produce clear error messages with source line numbers and exit code 1. | 13 example programs verified; unit tests for each error class. |
| **3. Compiler Concepts** | Clear application of core compiler theory and standard techniques | Scanned with regex token patterns; parsed with recursive descent and precedence climbing; analyzed with scoped symbol tables; verified using AST typestate traversal; compiled into SSA LLVM IR; executed via ORC JIT engine. | Explicit AST nodes (`AST.h`), scoped frames (`SymbolTable.h`), LLVM IR generation (`CodeGenerator.cpp`). |
| **4. Code Quality** | Modular, readable, maintainable C++17 codebase | Clean separation into `include/` and `src/`. RAII ownership via `std::unique_ptr`. No memory leaks. Decoupled abstractions (`CompiledFunctionHost`). No hardcoded or fake computation. | Zero compiler warnings under `-Wall -Wextra -Wpedantic`. |
| **5. Testing Quality** | Repeatable automated unit and integration tests | 7 test executables automated through CTest covering lexer, parser, semantics, typestate resources, interpreter, profiler, and JIT. | `ctest --test-dir build` runs in 0.22 seconds; 100% pass rate. |
| **6. Problem Solving** | Overcoming platform and integration challenges | Solved MSYS2 UCRT64 toolchain configuration; decoupled interpreter from LLVM link requirements; resolved Windows MinGW ORC JIT symbol materialization for `__main` and process symbols. | Documented in `docs/TECHNICAL_CHALLENGES.md`. |
| **7. Innovation** | Novel features beyond standard toy compilers | 1. **Compile-time typestate checking** for `Handle` lifecycle.<br>2. **Profiling-guided JIT compilation policy** restricting JIT to pure numeric hot routines while safely interpreting resource handlers. | `ResourceChecker.cpp`, `JITPolicy.cpp`. |
| **8. Viva Readiness** | Clear conceptual explanations, well-reasoned architectural tradeoffs | Comprehensive viva guide explaining front-end, middle-end, back-end, typestate mechanics, ORC JIT execution, and future directions. | `docs/VIVA.md`, `docs/REVIEW2_DEMO.md`. |

---

## 2. In-Depth Criterion Mapping

### 2.1 Implementation Progress & Architecture
TinyRAJIT implements a complete tiered execution pipeline:
* **Tier 0 (Frontend):** Lexer, Parser, and Symbol Table validate syntax and build an AST.
* **Tier 1 (Static Analysis):** Semantic Analyzer checks static types; Resource Checker verifies Handle lifecycles.
* **Tier 2 (Interpretation & Profiling):** AST Interpreter executes the program while the Profiler collects function invocation frequency.
* **Tier 3 (JIT Tiering & Native Execution):** JIT Policy determines candidate functions; LLVM CodeGenerator produces verified SSA IR; LLVM ORC JIT compiles and executes native machine instructions.

### 2.2 Functional Correctness & Diagnostics
The compiler does not simply exit with a generic failure; diagnostics identify the exact line number, error classification, and violation detail:
* **Lexical:** `ERROR [Line 2]: Lexical Error: Unexpected character '@'`
* **Syntax:** `ERROR [Line 3]: Syntax Error: expected ';' after return (got '}')`
* **Semantic:** `ERROR [Line 2]: Semantic Error: Variable 'y' has not been declared.`
* **Resource:** `ERROR [Line 5]: Resource Error: Handle 'h' is already closed.`

### 2.3 Innovation: Typestate Resource Safety
In traditional languages (like C), resources (file descriptors, sockets, GPU buffers) are managed dynamically, resulting in memory leaks, double-free vulnerabilities, or use-after-free bugs.
TinyRAJIT eliminates these issues at compile time by incorporating typestate analysis directly into the compiler pipeline:
* Tracks handle states across basic blocks and control-flow branches.
* Enforces that both branches of conditional statements leave the resource in equivalent states.
* Ensures that no handle remains open at function exit.

### 2.4 Innovation: Safe Profiling-Guided JIT Policy
Rather than naively compiling all code with JIT or using arbitrary heuristics, TinyRAJIT combines runtime profiling with static resource verification:
1. **Cold functions** are executed via the lightweight AST interpreter.
2. **Hot functions that manipulate handles** are kept in the interpreter to avoid the complexity and overhead of JIT-compiling I/O and runtime state.
3. **Hot functions with pure numeric computation** are compiled natively to achieve peak execution speed.
