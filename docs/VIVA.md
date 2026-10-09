# TinyRAJIT — Viva Voce Questions & Answers Guide

This guide contains frequently asked questions during compiler design vivas, paired with concise, implementation-specific answers reflecting the exact architecture of TinyRAJIT.

---

## 1. Compiler Architecture & Stages

### Q1: What are the phases in your compiler pipeline?
**Answer:** TinyRAJIT implements a classic multi-phase compiler pipeline:
1. **Lexical Analysis (`Lexer`):** Scans source text into discrete tokens (`Token`).
2. **Syntax Analysis (`Parser`):** Recursive-descent parser builds an Abstract Syntax Tree (`AST`).
3. **Semantic Analysis (`SemanticAnalyzer`):** Uses a scoped `SymbolTable` to enforce static type checks and variable binding rules.
4. **Typestate Resource Safety (`ResourceChecker`):** Compile-time DFA traversal verifying the lifecycle states of `Handle` objects (`UNOPENED`, `OPEN`, `CLOSED`).
5. **AST Interpreter (`Interpreter`):** Tree-walk evaluator with call frames.
6. **Runtime Profiler (`Profiler`):** Instruments function calls and classifies functions as `HOT` or `COLD`.
7. **JIT Policy Engine (`JITPolicy`):** Selects eligible functions (hot + numeric + resource-free).
8. **LLVM Code Generation (`CodeGenerator`):** Emits LLVM IR SSA instructions with `IRBuilder`.
9. **LLVM ORC JIT (`JITCompiler`):** Compiles IR to native machine instructions in memory using `llvm::orc::LLJIT` and executes via function pointer invocation.

### Q2: What is the difference between an Ahead-of-Time (AOT) compiler and a JIT compiler?
**Answer:** An AOT compiler compiles high-level code to native machine code prior to execution, producing a standalone binary on disk. A JIT compiler compiles code to machine code at runtime in memory while the program is running, allowing dynamic profile-guided compilation and selective tiering.

---

## 2. Lexical & Syntax Analysis

### Q3: How does your Lexer work?
**Answer:** The lexer performs single-pass linear scanning over the source character stream. It skips whitespace and comments, identifies multi-character operators (`==`, `<=`, `>=`, `->`) via lookahead, tokenizes numeric literals and identifiers, matches keywords against a known set, and tracks source line numbers to provide accurate diagnostic reporting. Unterminated strings or invalid characters (such as `@`) immediately raise a `LexicalError`.

### Q4: What parsing technique is used in TinyRAJIT?
**Answer:** A handwritten **recursive-descent LL(1) parser**. Grammar rules map directly to recursive C++ member functions (e.g., `parseFunction()`, `parseStatement()`, `parseExpression()`).

### Q5: How do you handle operator precedence and associativity?
**Answer:** Operator precedence is implemented through layered grammar hierarchy:
* Primary expressions (literals, variables, parenthesized expressions, function calls)
* Multiplicative operators (`*`, `/`)
* Additive operators (`+`, `-`)
* Relational and equality operators (`<`, `>`, `<=`, `>=`, `==`)
This ensures $a + b \times c$ is parsed as $a + (b \times c)$ rather than $(a + b) \times c$.

---

## 3. AST & Symbol Table

### Q6: How is the AST represented in C++?
**Answer:** As an object-oriented class hierarchy derived from base structs `Expr` and `Stmt`. Memory ownership is managed strictly via `std::unique_ptr` according to RAII principles, preventing memory leaks without requiring a garbage collector. We also implement a tree-walk printer `dumpAst()` that prints human-readable indented ASTs.

### Q7: How does your Symbol Table implement nested lexical scoping?
**Answer:** `SymbolTable` maintains a stack of scope dictionaries (`std::vector<Scope>`). When entering a function or block, `enterScope()` pushes a new frame; upon exiting, `exitScope()` pops it. Name resolution (`lookup()`) searches from the current innermost scope outward to global scope, enabling lexical shadowing while rejecting duplicate declarations within the same scope.

---

## 4. Semantic Analysis

### Q8: What semantic checks are enforced before code generation or interpretation?
**Answer:**
1. **Name resolution:** Variables must be declared before use.
2. **Duplicate detection:** Multiple declarations of the same identifier in the same scope are prohibited.
3. **Type checking:** Operators require compatible `Int` operands; condition expressions must evaluate to `Int`.
4. **Function signature verification:** Callee must exist, and argument counts/types must match the function signature.
5. **Entry-point requirement:** Every program must define `fn main() -> Int`.

---

## 5. Main Innovation: Resource Typestate Checking

### Q9: What is typestate checking, and why is it useful?
**Answer:** Traditional type systems track *what* an object is (its type), but not *what state* it is in. Typestate extends static checking to track the legal lifecycle states of an object at compile time. In TinyRAJIT, `Handle` represents a stateful resource with three explicit states:
$$\text{UNOPENED} \xrightarrow{\text{open}} \text{OPEN} \xrightarrow{\text{use}} \text{OPEN} \xrightarrow{\text{close}} \text{CLOSED}$$
This catches bugs like double-close, use-after-close, and resource leaks at compile time rather than relying on runtime failures.

### Q10: How does `ResourceChecker` handle branches (if/else)?
**Answer:** `ResourceChecker` performs conservative branch analysis: it clones the resource state environment before the branch, evaluates the `then` branch and `else` branch independently, and compares the resulting states. If a handle is `OPEN` on one branch and `CLOSED` on the other, compilation is rejected because the handle's state would be ambiguous at join points.

### Q11: How does `ResourceChecker` handle while loops?
**Answer:** Conservatively: because a while loop body may execute zero or many times, any operation inside the loop that mutates a handle's state (such as closing or opening it) is rejected. Handles used inside a loop must remain open before and after loop iterations.

---

## 6. Interpreter & Profiler

### Q12: How does the interpreter execute code?
**Answer:** The interpreter performs a recursive post-order tree walk over AST nodes. It maintains an execution environment (`std::unordered_map<std::string, Value>`) for local variables, creates fresh environments for function calls, and handles control-flow statements (`if`, `while`, `return`).

### Q13: What does the profiler do?
**Answer:** The profiler instruments the interpreter. Whenever a function is invoked, the profiler increments that function's call counter. If the call count reaches or exceeds the configurable `hotThreshold` (default 1000), the function is classified as `HOT`.

---

## 7. JIT Policy & Tiered Compilation

### Q14: What is the JIT compilation policy?
**Answer:** A function is eligible for JIT compilation only if:
1. It is classified as `HOT` by the profiler.
2. It is purely numeric (parameters, return values, and operations are `Int`).
3. It does **not** declare, receive, or manipulate `Handle` resources.
4. All functions it calls recursively or directly also satisfy these constraints.
Cold functions or handle-manipulating functions remain in the interpreter.

### Q15: Why exclude `Handle` operations from JIT compilation?
**Answer:** This separation of concerns simplifies native code generation, keeps native execution pure and verifiable, avoids complex runtime C ABI marshalling for mock resources in JIT memory, and prevents resource management errors in native code.

---

## 8. LLVM IR & ORC JIT

### Q16: How does LLVM IR generation work in `CodeGenerator`?
**Answer:** `CodeGenerator` creates an `llvm::LLVMContext` and `llvm::Module`. It traverses the eligible AST nodes and uses `llvm::IRBuilder<>` to emit LLVM SSA instructions:
* Variables are allocated using `CreateAlloca` in the entry block.
* Assignments use `CreateStore`; variable reads use `CreateLoad`.
* Arithmetic operations map to `CreateAdd`, `CreateSub`, `CreateMul`, `CreateSDiv`.
* Control flow creates basic blocks (`CreateCondBr`, `CreateBr`).
* Each generated function is formally verified using `llvm::verifyFunction()`.

### Q17: How does LLVM ORC JIT execute code natively?
**Answer:**
1. Initializes native X86 target machines (`InitializeNativeTarget()`, etc.).
2. Creates an `llvm::orc::LLJIT` instance via `LLJITBuilder`.
3. Wraps the LLVM module in a `ThreadSafeModule` and adds it to the JIT session via `addIRModule()`.
4. Resolves the requested symbol address using `jit->lookup(name)`.
5. Casts the resulting 64-bit address (`ExecutorAddr`) to a native C function pointer matching the signature (e.g. `int32_t (*)()`).
6. Invokes the function pointer directly on the host CPU.

### Q18: What specific challenge did you resolve with MinGW on Windows during JIT execution?
**Answer:** On Windows MinGW, LLVM's X86 codegen for `main` automatically inserts a call to `__main` for C runtime initialization. Since ORC JIT operates in an isolated JITDylib, `__main` was initially unresolved. We resolved this by registering `DynamicLibrarySearchGenerator::GetForCurrentProcess` and defining an absolute symbol for `__main` in `MainJITDylib` pointing to a harmless no-op routine.

---

## 9. Limitations & Future Scope

### Q19: What are the current limitations of TinyRAJIT?
**Answer:**
* **Type system:** Restricted to `Int` and `Handle` (strings exist only as literal arguments to `open()`).
* **Resource checking:** Conservative branch and loop analysis rather than full interprocedural path-sensitive dataflow analysis.
* **On-Stack Replacement (OSR):** Hot loops inside a cold function are not transitioned mid-execution to native code; tiering happens at function call boundaries.

### Q20: What are natural extensions for future work?
**Answer:**
1. Supporting user-defined types or structs.
2. Interprocedural typestate analysis passing handles across function parameters with borrow/move semantics.
3. On-Stack Replacement (OSR) for long-running while loops.
4. An optimizing middle-end pass pipeline (mem2reg, constant folding, dead-code elimination) before JIT emission.
