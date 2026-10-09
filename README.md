# Resource-Aware Profiling-Guided JIT Compiler (TinyRAJIT)

A Computer Science Engineering Compiler Design project: a custom language (`TinyRAJIT`) featuring a complete frontend, compile-time **Handle** typestate verification, an AST tree-walking interpreter, a runtime profiler, and native **LLVM ORC JIT** compilation for numeric functions.

---

## 1. Project Title

**Resource-Aware Profiling-Guided JIT Compiler**  
Language: **TinyRAJIT**  
Driver Binary: `resourcejit`

---

## 2. Abstract & Objective

TinyRAJIT compiles and executes a strongly-typed procedural language. The source code is lexed into tokens, parsed into an Abstract Syntax Tree (AST), checked with a scoped symbol table and semantic type rules, and analyzed by a custom **typestate resource checker** enforcing a strict protocol on `Handle` types (`open` $\to$ `use` $\to$ `close`). Valid programs execute via an AST interpreter. An integrated runtime profiler monitors function execution frequency; functions identified as hot and purely numeric are dynamically selected by a JIT policy engine, compiled into verified **LLVM IR**, and natively executed using the **LLVM ORC JIT** engine.

---

## 3. Problem Statement

Standard pedagogical compilers often terminate at syntax analysis or basic interpretation, without addressing static safety for stateful operating system resources (such as file handles, sockets, or hardware buffers) or demonstrating modern runtime optimization. Real-world runtimes require both compile-time safety invariants and tiered execution strategies. TinyRAJIT demonstrates an end-to-end compiler addressing both problems: static typestate safety and profiling-guided JIT compilation.

---

## 4. Compiler Architecture

```text
SOURCE CODE (.tiny)
        ↓
     LEXER             → Token Stream with Line Tracking
        ↓
     PARSER            → Recursive-Descent LL(1) AST Construction
        ↓
  SYMBOL TABLE         → Scoped Environments (Global, Function, Block)
        ↓
SEMANTIC ANALYZER      → Type Checking & Variable Binding
        ↓
 RESOURCE CHECKER      → Typestate Analysis (UNOPENED -> OPEN -> CLOSED)
        ↓
    INTERPRETER        → AST Evaluation & Call Stack
        ↓
    PROFILER           → Invocation Frequency Counter (HOT/COLD)
        ↓
   JIT POLICY          → Candidate Selection (Hot + Numeric + Resource-Free)
        ↓
 CODE GENERATOR        → LLVM IR Emission (IRBuilder, SSA Form)
        ↓
 LLVM ORC JIT          → LLJIT Machine Code Materialization & Execution
        ↓
 NATIVE EXECUTION      → Direct Host CPU Execution
```

---

## 5. Language Specification & Grammar

TinyRAJIT supports two user-visible types: `Int` and `Handle` (`String` exists exclusively as an argument to `open`).

### Formal Grammar (EBNF)
```text
program               → function+
function              → "fn" IDENTIFIER "(" parameters? ")" "->" type block
parameters            → parameter ("," parameter)*
parameter             → IDENTIFIER ":" type
type                  → "Int" | "Handle"
block                 → "{" statement* "}"
statement             → variableDeclaration | assignment | ifStatement
                      | whileStatement | returnStatement | expressionStatement
variableDeclaration   → "let" IDENTIFIER ":" type "=" expression ";"
assignment            → IDENTIFIER "=" expression ";"
ifStatement           → "if" "(" expression ")" block ("else" block)?
whileStatement        → "while" "(" expression ")" block
returnStatement       → "return" expression ";"
expressionStatement   → call ";"
expression            → comparison
comparison            → addition (("==" | "<" | "<=" | ">" | ">=") addition)*
addition              → multiplication (("+" | "-") multiplication)*
multiplication        → unary (("*" | "/") unary)*
unary                 → "-" unary | primary
primary               → NUMBER | STRING | IDENTIFIER | call | "(" expression ")"
call                  → IDENTIFIER "(" arguments? ")"
arguments             → expression ("," expression)*
```

*Operator Precedence:* Multiplication and division bind tighter than addition and subtraction. Comparisons have lowest arithmetic precedence. `a + b * c` parses as `a + (b * c)`.

---

## 6. Core Modules

| Module | Source / Header | Responsibility |
| :--- | :--- | :--- |
| **Lexer** | `src/Lexer.cpp`, `include/Lexer.h` | Scans source text, categorizes tokens, tracks line numbers, rejects invalid characters. |
| **Parser & AST** | `src/Parser.cpp`, `include/AST.h` | Builds strongly-typed AST with `std::unique_ptr` ownership; provides `dumpAst()`. |
| **Symbol Table** | `src/SymbolTable.cpp`, `include/SymbolTable.h` | Manages nested lexical scopes, type bindings, and function signatures. |
| **Semantic Analyzer** | `src/SemanticAnalyzer.cpp`, `include/SemanticAnalyzer.h` | Type checking, undeclared variable detection, call arity, and `main()` enforcement. |
| **Resource Checker** | `src/ResourceChecker.cpp`, `include/ResourceChecker.h` | Typestate checker: verifies `UNOPENED -> OPEN -> CLOSED`, detects leaks and double-close. |
| **Interpreter** | `src/Interpreter.cpp`, `include/Interpreter.h` | Tree-walk evaluator for integer arithmetic, variables, loops, branches, and mock handles. |
| **Profiler** | `src/Profiler.cpp`, `include/Profiler.h` | Counts runtime invocations, determines hotness against configurable threshold. |
| **JIT Policy** | `src/JITPolicy.cpp`, `include/JITPolicy.h` | Filters candidates: only hot, pure numeric functions without handles qualify for JIT. |
| **Code Generator** | `src/CodeGenerator.cpp`, `include/CodeGenerator.h` | Generates verified SSA LLVM IR using `llvm::IRBuilder`. |
| **JIT Compiler** | `src/JITCompiler.cpp`, `include/JITCompiler.h` | Instantiates `llvm::orc::LLJIT`, materializes native code, invokes function pointers. |

---

## 7. Project Structure

```text
TinyRAJIT/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── include/
│   ├── AST.h
│   ├── CodeGenerator.h
│   ├── Error.h
│   ├── Interpreter.h
│   ├── JITCompiler.h
│   ├── JITPolicy.h
│   ├── Lexer.h
│   ├── Parser.h
│   ├── Profiler.h
│   ├── ResourceChecker.h
│   ├── SemanticAnalyzer.h
│   ├── SymbolTable.h
│   └── Token.h
├── src/
│   ├── AST.cpp
│   ├── CodeGenerator.cpp
│   ├── Interpreter.cpp
│   ├── JITCompiler.cpp
│   ├── JITPolicy.cpp
│   ├── Lexer.cpp
│   ├── NoLLVMBackend.cpp
│   ├── Parser.cpp
│   ├── Profiler.cpp
│   ├── ResourceChecker.cpp
│   ├── SemanticAnalyzer.cpp
│   ├── SymbolTable.cpp
│   └── main.cpp
├── examples/
│   ├── arithmetic.tiny
│   ├── function.tiny
│   ├── hello.tiny
│   ├── hot_function.tiny
│   ├── if_else.tiny
│   ├── lexical_error.tiny
│   ├── loop.tiny
│   ├── resource_double_close.tiny
│   ├── resource_unclosed.tiny
│   ├── resource_use_after_close.tiny
│   ├── resource_valid.tiny
│   ├── semantic_error.tiny
│   └── syntax_error.tiny
├── tests/
│   ├── interpreter_test.cpp
│   ├── jit_test.cpp
│   ├── lexer_test.cpp
│   ├── parser_test.cpp
│   ├── profiler_test.cpp
│   ├── resource_test.cpp
│   ├── semantic_test.cpp
│   └── test_support.h
└── docs/
    ├── IMPLEMENTATION_STATUS.md
    ├── REVIEW2_DEMO.md
    ├── REVIEW2_EVIDENCE.md
    ├── REVIEW2_REQUIREMENTS.md
    ├── TECHNICAL_CHALLENGES.md
    └── VIVA.md
```

---

## 8. Build Instructions

### Prerequisites
* C++17 compatible compiler (GCC 15+, Clang 14+, or MSVC 2019+)
* CMake $\ge$ 3.16 and Ninja (or Make)
* LLVM 14 through 22 (`LLVMConfig.cmake`, `IRBuilder`, `orc::LLJIT`)

### Windows (MSYS2 UCRT64 — Verified Environment)
```powershell
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
cmake -G Ninja -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### Linux / Ubuntu / WSL2
```bash
sudo apt update && sudo apt install -y cmake g++ ninja-build llvm-dev
cmake -G Ninja -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### Frontend-Only Build (No LLVM installed)
```bash
cmake -S . -B build -DTINYRAJIT_ENABLE_JIT=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

---

## 9. Command-Line Interface (CLI)

```bash
# Print discrete token stream
resourcejit --tokens examples/arithmetic.tiny

# Print Abstract Syntax Tree
resourcejit --ast examples/arithmetic.tiny

# Perform static semantic and typestate resource analysis
resourcejit --check examples/resource_valid.tiny

# Run via AST Interpreter (default)
resourcejit --run examples/function.tiny
resourcejit examples/function.tiny

# Profile runtime execution and inspect JIT policy decision
resourcejit --profile examples/hot_function.tiny
resourcejit --threshold 100 --profile examples/hot_function.tiny

# Emit verified SSA LLVM IR
resourcejit --print-ir examples/function.tiny

# Compile and execute natively using LLVM ORC JIT
resourcejit --jit examples/function.tiny

# Measure interpreter vs JIT execution timing
resourcejit --benchmark examples/loop.tiny
```

---

## 10. Automated Test Suite (CTest)

TinyRAJIT includes 7 dedicated test executables:

```bash
ctest --test-dir build --output-on-failure
```

| Test Target | Covered Functionality | Status |
| :--- | :--- | :---: |
| `lexer_test` | Keywords, identifiers, literals, operators, `@` error, string termination | **PASSED** |
| `parser_test` | Declarations, precedence (`+` vs `*`), loops, conditionals, syntax errors | **PASSED** |
| `semantic_test` | Scoped lookup, undeclared variables, duplicates, call arity, type checking | **PASSED** |
| `resource_test` | Valid lifecycle, double-close, use-after-close, leaks at return, branch checks | **PASSED** |
| `interpreter_test` | Arithmetic, call frames, return propagation, loops, conditionals | **PASSED** |
| `profiler_test` | Invocations counting, hot threshold detection, loop heat propagation | **PASSED** |
| `jit_test` | IR emission, LLVM verifier, native ORC JIT execution, interpreter parity | **PASSED** |

**Pass Rate:** 100% (7/7 tests passed in 0.22 seconds).

---

## 11. Review 2 Demonstration Sequence

For the viva demonstration, execute the following commands in order:

1. **Lexical Analysis:**  
   `.\build\resourcejit.exe --tokens examples\arithmetic.tiny`
2. **AST & Operator Precedence:**  
   `.\build\resourcejit.exe --ast examples\arithmetic.tiny`
3. **Semantic Error Detection:**  
   `.\build\resourcejit.exe --check examples\semantic_error.tiny`
4. **Valid Resource Typestate:**  
   `.\build\resourcejit.exe --check examples\resource_valid.tiny`
5. **Resource Error (Double Close):**  
   `.\build\resourcejit.exe --check examples\resource_double_close.tiny`
6. **Resource Error (Use-After-Close):**  
   `.\build\resourcejit.exe --check examples\resource_use_after_close.tiny`
7. **Resource Error (Leak at Exit):**  
   `.\build\resourcejit.exe --check examples\resource_unclosed.tiny`
8. **AST Interpreter Execution:**  
   `.\build\resourcejit.exe --run examples\function.tiny`
9. **Runtime Profiling & Policy:**  
   `.\build\resourcejit.exe --profile examples\hot_function.tiny`
10. **LLVM IR Generation:**  
    `.\build\resourcejit.exe --print-ir examples\function.tiny`
11. **LLVM ORC JIT Native Execution:**  
    `.\build\resourcejit.exe --jit examples\function.tiny`

*(Refer to [`docs/REVIEW2_DEMO.md`](docs/REVIEW2_DEMO.md) for spoken explanations during the viva).*

---

## 12. Limitations & Future Scope

### Current Limitations
* Type system restricted to `Int` and `Handle`.
* Conservative typestate analysis on loops (disallows state mutations inside `while` bodies).
* JIT compiles at whole-function boundaries rather than supporting On-Stack Replacement (OSR) for loops.

### Future Scope
* Interprocedural typestate dataflow analysis supporting handle transfer with move semantics.
* On-Stack Replacement (OSR) for native compilation of long-running loops.
* LLVM optimization pass pipeline (constant propagation, dead-code elimination, vectorization).
