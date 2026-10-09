# TinyRAJIT — Technical Challenges and Engineering Solutions

This document details the genuine technical challenges encountered during the implementation and integration of the TinyRAJIT compiler, along with the precise engineering solutions developed to resolve them.

---

## Challenge 1: Windows Toolchain Discovery and LLVM Integration

### Context & Problem
The development environment is a 64-bit Windows workstation. On clean shells, neither `cmake` nor `llvm-config` was exported in the default user `PATH`. An initial build attempt showed missing build tools, raising the risk of having to fall back to a mock or frontend-only build.

### Investigation
Inspection of the system discovered:
* MSYS2 was installed under `C:\msys64` with `mingw-w64-ucrt-x86_64-gcc` 16.2.0.
* MSYS2's pacman package manager was accessible without administrator privilege constraints.

### Solution
1. Installed native development packages via MSYS2:
   ```bash
   pacman -S --needed mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-llvm
   ```
   This provided **CMake 4.4.3**, **Ninja 1.13.2**, and modern **LLVM 22.1.8** with complete development headers, libraries (`LLVMConfig.cmake`), and tools.
2. Formatted CMake invocation to explicitly leverage Ninja:
   ```powershell
   $env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
   cmake -G Ninja -S . -B build
   cmake --build build
   ```

---

## Challenge 2: Architectural Coupling Between AST Interpreter and JIT Engine

### Context & Problem
Initially, `Interpreter.cpp` directly included `JITCompiler.h` and invoked `JITCompiler::hasCompiled` and `JITCompiler::invoke`. This created an undesirable bidirectional compile-time dependency: the core frontend and interpreter required LLVM execution engine headers and libraries, preventing lightweight testing of frontend modules on machines lacking LLVM.

### Engineering Solution: `CompiledFunctionHost` Interface
Applied the **Dependency Inversion Principle (DIP)**:
1. Created an abstract C++ interface in `include/Interpreter.h`:
   ```cpp
   class CompiledFunctionHost {
    public:
     virtual ~CompiledFunctionHost() = default;
     virtual bool hasCompiled(const std::string& name) const = 0;
     virtual std::int32_t invoke(const std::string& name,
                                 const std::vector<std::int32_t>& args) = 0;
   };
   ```
2. Refactored `Interpreter` to accept `CompiledFunctionHost*` rather than a concrete `JITCompiler*`.
3. Made `JITCompiler` in `include/JITCompiler.h` inherit from `CompiledFunctionHost`.
4. In frontend tests (`interpreter_test`, `profiler_test`), the interpreter is passed `nullptr`, allowing all six core test suites to compile and link into `librajit_core.a` with zero LLVM library dependencies.

---

## Challenge 3: Windows MinGW ORC JIT Symbol Resolution Failure (`__main`)

### Context & Problem
When compiling a program containing `fn main()` with `--jit`, LLVM IR generation succeeded, and the module was added to the ORC JIT session. However, upon calling `impl_->jit->lookup("main")`, the JIT failed with:
```text
JIT session error: Symbols not found: [ __main ]
ERROR: JIT Error: ORC lookup failed for 'main': Failed to materialize symbols: { (main, { add, main }) }
```

### Root Cause Analysis
Under Windows MinGW targets (`x86_64-w64-windows-gnu`), GCC and LLVM conventions dictate that any function named `main` must invoke `__main` at function entry to execute static global constructors and C runtime initialization.
Because the ORC JIT session operated in an isolated `JITDylib` without host symbol generators or runtime libraries linked into the JIT namespace, `__main` remained an unresolved external symbol, aborting symbol materialization.

### Engineering Solution
In `src/JITCompiler.cpp`:
1. Added dynamic host process symbol lookup so any standard C runtime functions can be resolved by the JIT:
   ```cpp
   auto& jd = impl_->jit->getMainJITDylib();
   auto gen = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
       impl_->jit->getDataLayout().getGlobalPrefix());
   if (gen) {
     jd.addGenerator(std::move(*gen));
   }
   ```
2. Defined a harmless runtime no-op symbol for `__main`:
   ```cpp
   extern "C" void tinyrajit_noop_runtime() {}
   ```
3. Explicitly registered both the mangled and un-mangled interned `__main` symbol as an absolute symbol in `MainJITDylib`:
   ```cpp
   llvm::orc::SymbolMap symbols;
   symbols[impl_->jit->mangleAndIntern("__main")] = {
       llvm::orc::ExecutorAddr::fromPtr(&tinyrajit_noop_runtime),
       llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable
   };
   symbols[impl_->jit->getExecutionSession().intern("__main")] = {
       llvm::orc::ExecutorAddr::fromPtr(&tinyrajit_noop_runtime),
       llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable
   };
   (void)jd.define(llvm::orc::absoluteSymbols(std::move(symbols)));
   ```
Following this change, `--jit examples/function.tiny` immediately materialized native machine code and produced the exact result (`Program Result: 30`).

---

## Challenge 4: Compiler Pipeline Staging & Diagnostic Isolation

### Context & Problem
Initially, `src/main.cpp` used a monolithic `frontend()` helper function that sequentially executed:
$$\text{Lexer} \to \text{Parser} \to \text{SemanticAnalyzer} \to \text{ResourceChecker}$$
This meant running `resourcejit --tokens examples/syntax_error.tiny` would print the tokens, but immediately abort with a syntax error because the parser was invoked unconditionally. This violated the principle that diagnostic flags should run only the stage requested.

### Engineering Solution
Refactored `src/main.cpp` into decoupled, single-responsibility functions:
* `runLex(source, dump)`: Runs tokenization; exits immediately if `--tokens` is active.
* `runParse(tokens, dump)`: Runs recursive-descent parsing; exits immediately if `--ast` is active.
* `runCheck(program, checker)`: Runs static type checking and typestate resource analysis; exits immediately if `--check` is active.
* Back-end execution (`--run`, `--profile`, `--print-ir`, `--jit`): Continues only when full validation succeeds.

This enables users to inspect tokens of programs with syntax errors, inspect ASTs of programs with semantic errors, and preserves strict fail-stop semantics before interpretation or code generation.

---

## Challenge 5: Semantic Invariant Enforcement in Unit Test Snippets

### Context & Problem
When creating the automated `tests/jit_test.cpp` test suite, isolated helper functions were written:
```tiny
fn multiply_add(x: Int, y: Int, z: Int) -> Int { return x * y + z; }
```
When passed to `testutil::analyze()`, the test crashed with an uncaught `SemanticError: Program must define fn main() -> Int`.

### Investigation & Solution
TinyRAJIT's semantic specification requires every valid program to possess a top-level `fn main() -> Int` entry point to prevent headless programs from reaching execution. Rather than disabling this safety invariant in the compiler, all unit test snippets in `tests/jit_test.cpp` were updated to declare a clean `fn main() -> Int` harness, testing both isolated function compilation and entry-point dispatch.
