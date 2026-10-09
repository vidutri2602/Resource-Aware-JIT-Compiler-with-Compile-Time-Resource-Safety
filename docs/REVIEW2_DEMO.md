# TinyRAJIT — Review 2 Terminal Demonstration Guide

This guide provides an ordered, practical terminal script designed for the viva demonstration during Review 2. Every command listed below has been verified and works out-of-the-box.

---

## Prerequisites & Environment Setup

Run inside PowerShell (or Bash) in the repository root directory:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
```

Binary path: `.\build\resourcejit.exe` (or `resourcejit` on Linux/WSL).

---

## Demonstration 1: Lexical Analysis

### Command
```powershell
.\build\resourcejit.exe --tokens examples\arithmetic.tiny
```

### What the Evaluator Observes
```text
FN
IDENTIFIER(main)
LEFT_PAREN
RIGHT_PAREN
ARROW
INT
LEFT_BRACE
LET
IDENTIFIER(x)
COLON
INT
ASSIGN
NUMBER(10)
SEMICOLON
LET
IDENTIFIER(y)
COLON
INT
ASSIGN
NUMBER(20)
SEMICOLON
RETURN
IDENTIFIER(x)
PLUS
IDENTIFIER(y)
SEMICOLON
RIGHT_BRACE
EOF
```

### Compiler Concept Demonstrated
* **Lexical Scanning & Tokenization**: Converts raw source text into a structured sequence of discrete lexical tokens, categorizing keywords (`fn`, `let`, `return`, `Int`), identifiers, operators, punctuation, and assigning line information.
* Explicit `EOF` token generation marks end of translation stream.

### Viva Explanation (Say Aloud)
> *"Here, the Lexer scans the characters, strips whitespace, recognizes keywords and literals using regular patterns, and produces an ordered token stream with source line metadata for the parser."*

---

## Demonstration 2: Abstract Syntax Tree (AST) & Operator Precedence

### Command
```powershell
.\build\resourcejit.exe --ast examples\arithmetic.tiny
```

### What the Evaluator Observes
```text
Function: main -> Int
  Body:
    VariableDeclaration
    ├── Name: x
    ├── Type: Int
    └── 10
    VariableDeclaration
    ├── Name: y
    ├── Type: Int
    └── 20
    Return
        BinaryExpression: +
            x
            y
```

### Compiler Concept Demonstrated
* **Syntax Analysis (Recursive-Descent Parsing)**: Constructing an Abstract Syntax Tree (AST) capturing hierarchical statement and expression relationships using C++ `std::unique_ptr` ownership. Precedence climbing guarantees multiplication binds tighter than addition (`a + b * c` parses as `a + (b * c)`).

### Viva Explanation (Say Aloud)
> *"Our recursive-descent parser constructs a strongly-typed AST where expressions and statements are represented as hierarchical tree nodes. Notice the clear AST dumping that visualizes variable declarations and return expressions."*

---

## Demonstration 3: Semantic Error Detection

### Command
```powershell
.\build\resourcejit.exe --check examples\semantic_error.tiny
```

### What the Evaluator Observes
```text
ERROR [Line 2]:
Semantic Error:
Variable 'y' has not been declared.
```

### Compiler Concept Demonstrated
* **Scoped Symbol Table & Semantic Analysis**: Name resolution detects variables used before declaration or out of scope. Checks arity, type consistency, and assignment validity before code execution.

### Viva Explanation (Say Aloud)
> *"During semantic analysis, the compiler traverses the AST using a scoped symbol table. Because `y` is assigned without a declaration, the analyzer immediately rejects it at line 2 with an informative error message and a non-zero exit code."*

---

## Demonstration 4: Valid Handle Resource Lifecycle

### Command
```powershell
.\build\resourcejit.exe --check examples\resource_valid.tiny
```

### What the Evaluator Observes
```text
Resource Analysis:
h: UNOPENED
h: OPEN
h: OPEN
h: CLOSED

[OK] Resource analysis passed.
```

### Compiler Concept Demonstrated
* **Typestate / Resource-State Verification**: Compile-time finite state machine tracking for system handles:
  $$\text{UNOPENED} \xrightarrow{\text{open}} \text{OPEN} \xrightarrow{\text{use}} \text{OPEN} \xrightarrow{\text{close}} \text{CLOSED}$$

### Viva Explanation (Say Aloud)
> *"This is our primary research contribution: compile-time resource checking. The compiler simulates the state of handle `h` statically, observing it transition from UNOPENED to OPEN, survive valid uses, and close before the function returns."*

---

## Demonstration 5: Resource Error — Double Close Rejection

### Command
```powershell
.\build\resourcejit.exe --check examples\resource_double_close.tiny
```

### What the Evaluator Observes
```text
ERROR [Line 5]:
Resource Error:
Handle 'h' is already closed.
```

### Compiler Concept Demonstrated
* **State Invariant Enforcement**: Rejects illegal state transition $\text{CLOSED} \xrightarrow{\text{close}} \text{ERROR}$.

### Viva Explanation (Say Aloud)
> *"If a program attempts to close an already-closed handle, the compiler detects this static state violation and prevents double-free vulnerabilities before running any code."*

---

## Demonstration 6: Resource Error — Use-After-Close Rejection

### Command
```powershell
.\build\resourcejit.exe --check examples\resource_use_after_close.tiny
```

### What the Evaluator Observes
```text
ERROR [Line 5]:
Resource Error:
Cannot use closed Handle 'h'.
```

### Compiler Concept Demonstrated
* **Resource Safety & Typestate Checking**: Rejects operation on invalid resource $\text{CLOSED} \xrightarrow{\text{use}} \text{ERROR}$.

### Viva Explanation (Say Aloud)
> *"Here, the programmer calls `use(h)` after `close(h)`. Traditional dynamic systems might crash or leak at runtime; TinyRAJIT detects and rejects this at compile time."*

---

## Demonstration 7: Resource Error — Unclosed Resource Leak

### Command
```powershell
.\build\resourcejit.exe --check examples\resource_unclosed.tiny
```

### What the Evaluator Observes
```text
ERROR [Line 4]:
Resource Error:
Handle 'h' remains open at function exit.
```

### Compiler Concept Demonstrated
* **Function Exit Invariant Checking**: All opened resources must reach state `CLOSED` along all execution paths before the function returns.

### Viva Explanation (Say Aloud)
> *"Resource safety requires that handles are never leaked. The checker verifies that any handle still in state `OPEN` when the return statement is encountered causes compilation failure."*

---

## Demonstration 8: AST-Based Interpreter Execution

### Command
```powershell
.\build\resourcejit.exe --run examples\function.tiny
```

### What the Evaluator Observes
```text
Program Result: 30
```

### Compiler Concept Demonstrated
* **AST Tree-Walk Interpreter**: Evaluates the program using function environments and call stacks, propagating returns and arguments cleanly (`add(10, 20) -> 30`).

### Viva Explanation (Say Aloud)
> *"Our AST interpreter evaluates expressions and statements directly without bytecode overhead, demonstrating complete evaluation semantics for arithmetic, control flow, and function calls."*

---

## Demonstration 9: Runtime Profiling & JIT Policy Decision

### Command
```powershell
.\build\resourcejit.exe --profile examples\hot_function.tiny
```

### What the Evaluator Observes
```text
Program Result: 499500

Function           Calls       Status
----------------------------------------
calculate          1001        HOT
main               1           COLD

[JIT Eligibility]

Function: calculate
Hot: YES
Resource Eligible: YES
Decision: JIT COMPILE

Function: main
Hot: NO
Resource Eligible: YES
Decision: INTERPRETER
```

### Compiler Concept Demonstrated
* **Profiling-Guided Optimization (PGO) & Tiering Policy**: Runtime instrumentation tracks call counts. The JIT policy checks both frequency (exceeds hot threshold 1000) and resource safety (pure numeric, no Handle operations) before selecting candidate functions for native compilation.

### Viva Explanation (Say Aloud)
> *"Here the runtime profiler monitored executions: function `calculate` was invoked 1,001 times, crossing our 1,000 threshold and becoming HOT. The JIT policy confirms `calculate` is pure numeric, marking it for JIT COMPILE, while `main` remains cold and is interpreted."*

---

## Demonstration 10: LLVM IR Generation & Verification

### Command
```powershell
.\build\resourcejit.exe --print-ir examples\function.tiny
```

### What the Evaluator Observes
```llvm
; ModuleID = 'tinyrjit'
source_filename = "tinyrjit"

define i32 @add(i32 %a, i32 %b) {
entry:
  %b2 = alloca i32, align 4
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  store i32 %b, ptr %b2, align 4
  %a3 = load i32, ptr %a1, align 4
  %b4 = load i32, ptr %b2, align 4
  %addtmp = add i32 %a3, %b4
  ret i32 %addtmp
}

define i32 @main() {
entry:
  %calltmp = call i32 @add(i32 10, i32 20)
  ret i32 %calltmp
}
```

### Compiler Concept Demonstrated
* **Intermediate Representation (IR) Emission**: Emits valid, SSA-form LLVM IR using `llvm::IRBuilder`, verified via `llvm::verifyFunction()`.

### Viva Explanation (Say Aloud)
> *"Our code generator translates eligible AST nodes into real LLVM Intermediate Representation. It allocates stack slots, handles loads and stores, and emits SSA instructions verified by LLVM's function verifier."*

---

## Demonstration 11: LLVM ORC JIT Native Execution

### Command
```powershell
.\build\resourcejit.exe --jit examples\function.tiny
```

### What the Evaluator Observes
```text
Program Result: 30
```

### Compiler Concept Demonstrated
* **On-Request Compilation (ORC JIT)**: The LLVM ORC JIT compiles LLVM IR into native x86_64 machine instructions in memory, looks up the function pointer, invokes it with native ABI calling conventions, and outputs the result.

### Viva Explanation (Say Aloud)
> *"Finally, LLVM ORC JIT compiles the IR into machine code in memory. The runtime calls into native compiled code, matching the interpreter result of 30, proving full tier-2 JIT capability."*

---

## Summary Checklist for Viva

| Demo # | Stage | Command | Result |
| :---: | :--- | :--- | :--- |
| **1** | Lexer | `resourcejit --tokens examples\arithmetic.tiny` | Exact token stream |
| **2** | Parser / AST | `resourcejit --ast examples\arithmetic.tiny` | Structured AST hierarchy |
| **3** | Semantics | `resourcejit --check examples\semantic_error.tiny` | Rejects undeclared var (exit: 1) |
| **4** | Resource Valid | `resourcejit --check examples\resource_valid.tiny` | Lifecycle verified (exit: 0) |
| **5** | Double Close | `resourcejit --check examples\resource_double_close.tiny` | Rejects double close (exit: 1) |
| **6** | Use-After-Close | `resourcejit --check examples\resource_use_after_close.tiny` | Rejects use after close (exit: 1) |
| **7** | Unclosed Handle | `resourcejit --check examples\resource_unclosed.tiny` | Rejects leak at exit (exit: 1) |
| **8** | Interpreter | `resourcejit --run examples\function.tiny` | Returns 30 |
| **9** | Profiler & Policy | `resourcejit --profile examples\hot_function.tiny` | HOT decision table |
| **10** | LLVM IR | `resourcejit --print-ir examples\function.tiny` | Verified LLVM IR |
| **11** | ORC JIT | `resourcejit --jit examples\function.tiny` | Native execution returns 30 |
