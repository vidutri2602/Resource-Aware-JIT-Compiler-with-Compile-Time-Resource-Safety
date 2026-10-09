# TinyRAJIT — Review 2 Empirical Evidence Document

This document records the empirical verification and automated test execution results across every module of the TinyRAJIT compiler system.

---

## 1. Test Execution Summary

Build system: **CMake 4.4.3 + Ninja 1.13.2**  
Compiler: **GCC 16.2.0 (MSYS2 UCRT64)**  
Backend: **LLVM 22.1.8 (Target: X86 / x86_64-w64-windows-gnu)**  

Test command:
```powershell
ctest --test-dir build --output-on-failure
```

### Empirical CTest Log
```text
Test project D:/VIDUTRI/studying/VIT/5sem/CD/lab/Resource-Aware-JIT-Compiler-with-Compile-Time-Resource-Safety/build
    Start 1: lexer_test
1/7 Test #1: lexer_test .......................   Passed    0.02 sec
    Start 2: parser_test
2/7 Test #2: parser_test ......................   Passed    0.02 sec
    Start 3: semantic_test
3/7 Test #3: semantic_test ....................   Passed    0.02 sec
    Start 4: resource_test
4/7 Test #4: resource_test ....................   Passed    0.02 sec
    Start 5: interpreter_test
5/7 Test #5: interpreter_test .................   Passed    0.02 sec
    Start 6: profiler_test
6/7 Test #6: profiler_test ....................   Passed    0.03 sec
    Start 7: jit_test
7/7 Test #7: jit_test .........................   Passed    0.07 sec

100% tests passed out of 7
Total Test time (real) = 0.22 sec
```

---

## 2. Module-by-Module Verification & Evidence

### 2.1 Lexical Analysis (`lexer_test.exe`)
* **Source:** `src/Lexer.cpp`, `include/Lexer.h`, `include/Token.h`
* **Test binary:** `.\build\lexer_test.exe`
* **Execution Output:**
  ```text
  PASS: keywords identifiers numbers
  PASS: operators and keywords
  PASS: invalid character
  PASS: strings and line numbers
  All lexer tests passed.
  ```
* **Coverage:**
  * Keywords (`fn`, `let`, `if`, `else`, `while`, `return`, `Int`, `Handle`)
  * Identifier names and integer literals
  * Multi-character tokens (`==`, `<=`, `>=`, `->`)
  * Lexical error diagnostics (unexpected character `@`, unterminated strings)
  * Accurate line numbering

### 2.2 Syntax Analysis & AST (`parser_test.exe`)
* **Source:** `src/Parser.cpp`, `src/AST.cpp`, `include/Parser.h`, `include/AST.h`
* **Test binary:** `.\build\parser_test.exe`
* **Execution Output:**
  ```text
  PASS: valid program parse
  PASS: syntax error recovery/detection
  PASS: operator precedence (+ vs *)
  PASS: if and while statements
  All parser tests passed.
  ```
* **Coverage:**
  * Arithmetic expression precedence: verified $a + b \times c \equiv a + (b \times c)$
  * Block statements, assignments, declarations, function headers
  * Error reporting for missing punctuation (e.g. missing semicolon or mismatched braces)

### 2.3 Scoped Semantic Analysis (`semantic_test.exe`)
* **Source:** `src/SemanticAnalyzer.cpp`, `src/SymbolTable.cpp`
* **Test binary:** `.\build\semantic_test.exe`
* **Execution Output:**
  ```text
  PASS: valid semantics
  PASS: undeclared variable rejected
  PASS: duplicate variable rejected
  PASS: type mismatch rejected
  PASS: unknown function rejected
  PASS: call arity mismatch rejected
  All semantic tests passed.
  ```
* **Coverage:**
  * Scoped variable lookup across nested scopes
  * Function signature checking (arity and parameter types)
  * Prevention of duplicate bindings in the same scope
  * Rejection of undeclared variables

### 2.4 Compile-Time Resource Checking (`resource_test.exe`)
* **Source:** `src/ResourceChecker.cpp`, `include/ResourceChecker.h`
* **Test binary:** `.\build\resource_test.exe`
* **Execution Output:**
  ```text
  PASS: valid resource lifecycle
  PASS: double close rejected
  PASS: use after close rejected
  PASS: use before open rejected
  PASS: leak at function return rejected
  All resource checker tests passed.
  ```
* **Coverage:**
  * Valid lifecycle: $\text{UNOPENED} \to \text{OPEN} \to \text{CLOSED}$
  * Invariant violations: Double close, use after close, use before open
  * Path checking: rejection of unclosed handles at function exit
  * Conservative branching: state mismatch across if/else branches flagged

### 2.5 AST-Based Interpreter (`interpreter_test.exe`)
* **Source:** `src/Interpreter.cpp`, `include/Interpreter.h`
* **Test binary:** `.\build\interpreter_test.exe`
* **Execution Output:**
  ```text
  PASS: basic arithmetic and variables
  PASS: function call and return propagation
  PASS: conditionals (if-else)
  PASS: loops (while sum)
  PASS: runtime error detection
  All interpreter tests passed.
  ```
* **Coverage:**
  * Variable environment and call frame stack
  * Integer binary evaluation (`+`, `-`, `*`, `/`)
  * While loops with mutation
  * Conditionals and return value propagation

### 2.6 Runtime Profiler & JIT Policy (`profiler_test.exe`)
* **Source:** `src/Profiler.cpp`, `src/JITPolicy.cpp`
* **Test binary:** `.\build\profiler_test.exe`
* **Execution Output:**
  ```text
  PASS: call counting
  PASS: hot function detection
  PASS: runtime hot loop heat
  All profiler tests passed.
  ```
* **Coverage:**
  * Call counting during interpretation
  * Hot threshold trigger (default 1000, configurable)
  * Distinction between HOT and COLD functions
  * Policy filtering: only pure numeric functions qualify for JIT compilation; handle-bearing functions are restricted to interpreter

### 2.7 LLVM IR Generation & Native ORC JIT (`jit_test.exe`)
* **Source:** `src/CodeGenerator.cpp`, `src/JITCompiler.cpp`
* **Test binary:** `.\build\jit_test.exe`
* **Execution Output:**
  ```text
  PASS: numeric eligibility calculation
  PASS: LLVM IR generation & verification
  PASS: JIT compilation tracking
  PASS: JIT native execution (multiply_add 6*7+8 = 50)
  PASS: JIT loop & branch native execution
  PASS: Interpreter and JIT parity check (fib(10) == 55)
  PASS: Handle resource function excluded from JIT
  PASS: uncompiled function invocation rejection
  All JIT tests passed.
  ```
* **Coverage:**
  * LLVM IRBuilder SSA emission
  * `llvm::verifyFunction` validation
  * LLVM ORC JIT dynamic symbol materialization on Windows
  * Native execution of arithmetic, conditional branching, and loops
  * Parity verification against the interpreter (Fibonacci, loops, arithmetic)

---

## 3. Interpreter vs JIT Parity Results

| Program | Interpreter Result | Native JIT Result | Parity Status |
| :--- | :---: | :---: | :---: |
| `examples/arithmetic.tiny` | `30` | `30` | **IDENTICAL** |
| `examples/function.tiny` | `30` | `30` | **IDENTICAL** |
| `examples/if_else.tiny` | `1` | `1` | **IDENTICAL** |
| `examples/loop.tiny` | `45` | `45` | **IDENTICAL** |
| `examples/hot_function.tiny` | `499500` | `499500` | **IDENTICAL** |
| `fib(10)` in `jit_test` | `55` | `55` | **IDENTICAL** |

All tests and demonstration programs produce consistent, reproducible output across execution tiers.
