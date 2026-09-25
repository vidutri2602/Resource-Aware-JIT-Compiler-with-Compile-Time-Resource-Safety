# Resource-Aware Profiling-Guided JIT Compiler (TinyRAJIT)

A student Compiler Design project: a small language (`TinyRAJIT`) with a full frontend, compile-time **Handle** protocol checking, a tree-walking interpreter, runtime profiling, and LLVM ORC JIT for numeric functions.

This is a teaching compiler, not a production toolchain.

---

## 1. Project Title

**Resource-Aware Profiling-Guided JIT Compiler**  
Language: **TinyRAJIT**  
Driver: `resourcejit`

## 2. Abstract

TinyRAJIT compiles and runs a tiny C-like language. Source is lexed and parsed into an AST, checked with a symbol table and type rules, then checked by a **resource-state analyzer** for `Handle` values (`open` / `use` / `close`). Valid programs run on an interpreter. A profiler counts function activity; hot, resource-free numeric functions may be compiled with **LLVM IR** and executed through **LLVM ORC JIT**.

## 3. Problem Statement

Introductory compilers often stop at parsing or interpretation. Real systems also need static safety and a story for “hot” code. This project demonstrates those compiler-design stages on one small language, with **resource safety** as the distinctive static analysis.

## 4. Motivation

File-like resources have a simple protocol: open, use while open, close once. Encoding that protocol in a compiler pass shows how semantic analysis can prevent classes of bugs before execution. Pairing that with a profiler and LLVM JIT shows how an interpreter and native code can coexist.

## 5. Objectives

- Implement lexical analysis, recursive-descent parsing, and an AST.
- Maintain a scoped symbol table and type checks.
- Analyze `Handle` states conservatively across `if`/`while`.
- Interpret programs after all static checks succeed.
- Profile calls and loop activity; decide JIT eligibility.
- Emit LLVM IR and run eligible functions with ORC JIT.
- Provide CLI flags, examples, tests, and a prototype benchmark.

## 6. Scope

**In scope:** `Int` and `Handle`, functions, `if`/`else`, `while`, arithmetic, comparisons, builtins `open`/`use`/`close`, interpreter, profiler, LLVM IR + ORC JIT for **numeric** functions.

**Out of scope:** classes, arrays, structs, generics, exceptions, GC, concurrency, multiple resource types, custom machine-code emitters, production optimizations.

## 7. Compiler Architecture

```text
SOURCE CODE
     ↓
LEXER
     ↓
TOKENS
     ↓
PARSER
     ↓
AST
     ↓
SYMBOL TABLE
     ↓
SEMANTIC ANALYSIS
     ↓
RESOURCE CHECKER
     ↓
INTERPRETER
     ↓
PROFILER
     ↓
JIT POLICY
     ↓
LLVM IR
     ↓
LLVM ORC JIT
     ↓
NATIVE CODE
```

`--jit` compiles resource-eligible numeric functions up front (student-sized policy: no on-stack replacement mid-loop). `--profile` still prints hotness and the same eligibility rules.

## 8. Language Specification

Types: `Int`, `Handle`.

```text
let x: Int = 10;
let h: Handle = open("data.txt");
x = x + 1;
if (x > 5) { ... } else { ... }
while (i < n) { ... }
fn add(a: Int, b: Int) -> Int { return a + b; }
use(h); close(h);
```

Operators: `+ - * /` and `== < > <= >=`. Comments: `#` or `//`.

Every program must define `fn main() -> Int` with no parameters.

## 9. Grammar

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
comparison            → addition (("=="|"<"|"<="|">"|">=") addition)*
addition              → multiplication (("+"|"-") multiplication)*
multiplication        → unary (("*"|"/") unary)*
unary                 → "-" unary | primary
primary               → NUMBER | STRING | IDENTIFIER | call | "(" expression ")"
call                  → IDENTIFIER "(" arguments? ")"
```

`*` binds tighter than `+`, so `a + b * c` is `a + (b * c)`.

## 10. Lexer

`Lexer` (`include/Lexer.h`, `src/Lexer.cpp`) skips whitespace/comments, classifies keywords, identifiers, integers, strings, operators, and punctuation, and tracks line numbers. Invalid characters raise `LexicalError`.

## 11. Parser

`Parser` is recursive descent. Parse errors include the line number and the unexpected lexeme.

## 12. AST

Nodes: `NumberExpr`, `StringExpr`, `VariableExpr`, `BinaryExpr`, `CallExpr`, `VariableDeclStmt`, `AssignmentStmt`, `ExpressionStmt`, `ReturnStmt`, `IfStmt`, `WhileStmt`, `FunctionDecl`, `Program`. Ownership uses `std::unique_ptr`. `--ast` prints a tree via `dumpAst`.

## 13. Symbol Table

`SymbolTable` supports `insert`, `lookup`, `exists`, `enterScope`, `exitScope`. Symbols record name, type, kind (variable/function), scope, and parameter types.

## 14. Semantic Analysis

`SemanticAnalyzer` checks: declaration before use, duplicate names, unknown functions, argument count, assignment/return types, and that `open`/`use`/`close` have the right operand types (`open` takes a string; `use`/`close` take `Handle`).

## 15. Resource-State Analysis

Core project feature. States: `UNOPENED → open() → OPEN → close() → CLOSED`.

| Rule | Result |
| --- | --- |
| `open` then `use` then `close` | valid |
| `close` twice | error: already closed |
| `use` after `close` | error: cannot use closed Handle |
| `use` while `UNOPENED` | error: has not been opened |
| still `OPEN` at `return` / function end | error: remains open |

**If/else:** both branches are analyzed; mismatched Handle states are rejected (conservative).  
**While:** if the loop body would change a Handle state, the program is rejected (conservative; not a full data-flow solver).  
Handle **parameters** are treated as already `OPEN` (caller responsibility). Functions that use Handles are **not** LLVM-JIT candidates.

## 16. Interpreter

Tree-walking evaluator for integers, variables, arithmetic, comparisons, control flow, calls, and Handle builtins. Runs only after lexer, parser, semantics, and resource analysis succeed. Division by zero is a runtime error.

## 17. Profiler

`recordFunctionCall`, `getCallCount`, `isHot`. Default `HOT_THRESHOLD = 1000` (override with `--threshold N`). Function entries are counted; each `while` iteration also records heat for the current function so `calculate(1000)` appears HOT.

## 18. JIT Policy

A function is compiled only if it is **numeric** (Int-only, no Handle builtins) **and** resource-eligible (no Handle usage, including callees). Printed as:

```text
[JIT Eligibility]
Function: calculate
Hot: YES
Resource Eligible: YES
Decision: JIT COMPILE
```

`--jit` compiles all resource-eligible numeric functions (not only those already hot), which is appropriate for a one-shot student driver.

## 19. LLVM IR

`CodeGenerator` uses `llvm::IRBuilder` for constants, locals, arithmetic, comparisons, `return`, `if`, `while`, and calls among eligible functions. `--print-ir` dumps the module.

## 20. LLVM ORC JIT

`JITCompiler` initializes native targets, builds `llvm::orc::LLJIT`, adds the IR module, looks up a symbol, and invokes it. Only the numeric subset is compiled; Handle code stays in the interpreter.

## 21. Project Structure

```text
ResourceAwareJIT/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── examples/
│   ├── hello.tiny
│   ├── arithmetic.tiny
│   ├── function.tiny
│   ├── if_else.tiny
│   ├── loop.tiny
│   ├── resource_valid.tiny
│   ├── resource_double_close.tiny
│   ├── resource_use_after_close.tiny
│   ├── resource_unclosed.tiny
│   └── hot_function.tiny
├── include/          Token, Lexer, AST, Parser, SymbolTable, SemanticAnalyzer,
│                     ResourceChecker, Interpreter, Profiler, JITPolicy,
│                     CodeGenerator, JITCompiler, Error
├── src/              matching .cpp files + main.cpp
└── tests/            lexer, parser, semantic, resource, interpreter, profiler
```

## 22. Build Instructions

**Expected LLVM:** 14 through 21 (needs `LLVMConfig.cmake`, `IRBuilder`, and `orc::LLJIT`). Do not hardcode `C:/LLVM/...`; CMake uses `find_package(LLVM REQUIRED CONFIG)`.

Ubuntu/Debian:

```bash
sudo apt install cmake g++ llvm-dev
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

MSYS2 UCRT64:

```bash
pacman -S mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-llvm
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

If CMake cannot find LLVM, pass `-DLLVM_DIR=/path/to/lib/cmake/llvm`.

## 23. Run Instructions

```bash
resourcejit examples/hello.tiny
resourcejit --tokens examples/arithmetic.tiny
resourcejit --ast examples/arithmetic.tiny
resourcejit --check examples/resource_valid.tiny
resourcejit --run examples/function.tiny
resourcejit --profile examples/hot_function.tiny
resourcejit --print-ir examples/function.tiny
resourcejit --jit examples/function.tiny
resourcejit --benchmark examples/loop.tiny
resourcejit --threshold 100 examples/hot_function.tiny
```

## 24. Example Programs

| File | Role |
| --- | --- |
| `hello.tiny` | return 42 |
| `arithmetic.tiny` | locals and `+` |
| `function.tiny` | `add(10,20)` → 30 |
| `if_else.tiny` | branch |
| `loop.tiny` | `calculate(10)` → 45 |
| `resource_valid.tiny` | open/use/close |
| `resource_double_close.tiny` | rejected |
| `resource_use_after_close.tiny` | rejected |
| `resource_unclosed.tiny` | rejected |
| `hot_function.tiny` | profile `calculate(1000)` |

## 25. Test Cases

| Test | Coverage |
| --- | --- |
| `lexer_test` | keywords, identifiers, numbers, operators, `@` |
| `parser_test` | function, bad syntax, precedence, if, while |
| `semantic_test` | undeclared, duplicate, type mismatch, bad call |
| `resource_test` | valid, double close, use-after-close, use-before-open, leak |
| `interpreter_test` | arithmetic, variables, functions, if, loops |
| `profiler_test` | counts and HOT detection |

Each test prints `PASS:` / `FAIL:` and exits non-zero on failure.

## 26. Benchmarking

`--benchmark` is a **prototype measurement**. It times one interpreter run, JIT compilation, and one JIT execution of `main` when `main` is eligible. Numbers are measured on the host; they are not claimed as research results.

```text
Prototype measurement (not a production benchmark)
Interpreter Time: X ms
JIT Compilation Time: Y ms
JIT Execution Time: Z ms
```

## 27. Limitations

- One resource type (`Handle`); no real OS file I/O (open is a protocol object).
- Conservative `if`/`while` resource analysis.
- No interprocedural Handle tracking beyond “function uses Handles”.
- JIT subset is Int-only; at most four JIT arguments in the invoke helper.
- No SSA optimizations, inlining pipeline, or deoptimization.
- Profiler heat includes loop iterations; it is not hardware performance counters.

## 28. Future Scope

Smarter path-sensitive resource DFG, real file descriptors, on-stack replacement after a hotness threshold, and a larger language subset (booleans as a distinct type, more LLVM types).

---

## Phase 2 requirement mapping

| Academic requirement | This project |
| --- | --- |
| Lexical Analysis | `Lexer` |
| Syntax Analysis | Recursive-descent `Parser` |
| AST | AST classes + `dumpAst` |
| Semantic Analysis | `SemanticAnalyzer` |
| Symbol Table | `SymbolTable` |
| Intermediate Code | LLVM IR (`CodeGenerator`) |
| Interpreter / Execution Engine | `Interpreter` |
| Error Handling | Lexer + Parser + Semantic + Resource + Runtime + JIT |
| Additional project feature | `ResourceChecker` |
| Additional runtime feature | `Profiler` |
| Advanced extension | LLVM ORC JIT (`JITCompiler`) |
