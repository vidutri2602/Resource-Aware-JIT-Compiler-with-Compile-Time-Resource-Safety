# Resource-Aware JIT Compiler with Compile-Time Resource Safety

A small compiler for **Tiny**, a teaching language with:

1. A recursive-descent frontend (lexer, parser, AST)
2. Compile-time **resource safety** (acquire/release must balance on every path)
3. A tree-walking **interpreter**
4. A **profiler** that detects hot loops
5. A **method JIT** that compiles hot loops to a compact bytecode VM

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

On Windows with Visual Studio:

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The driver is `rajit` (or `Release/rajit.exe`).

## Run

```bash
# Hello world + call
./build/rajit examples/hello.tiny

# Resource checker accepts balanced acquire/release
./build/rajit examples/resource_valid.tiny

# Resource checker rejects a leaked file handle
./build/rajit examples/resource_invalid.tiny

# Profile a hot loop, then compile the remainder
./build/rajit --dump-jit --threshold 32 examples/hot_loop.tiny
```

Useful flags:

| Flag | Meaning |
| --- | --- |
| `--tokens` | dump token stream |
| `--ast` | dump AST |
| `--check` | stop after resource/type checking |
| `--no-jit` | interpret only |
| `--dump-jit` | disassemble compiled hot loops |
| `--threshold N` | loop hits before JIT (default 32) |

## Tiny language

```text
program     → fnDecl+
fnDecl      → "fn" IDENT "(" params? ")" block
block       → "{" stmt* "}"
stmt        → let | assign | if | while | print | return | release | expr ";"
let         → "let" IDENT "=" expr ";"
            | "let" IDENT "=" "acquire" ("file"|"mem"|"lock") expr? ";"
assign      → IDENT "=" expr ";"
if          → "if" "(" expr ")" block ("else" block)?
while       → "while" "(" expr ")" block
print       → "print" expr ";"
release     → "release" IDENT ";"
```

Comments start with `#` or `//`.

### Resource rules (checked before execution)

- `acquire` puts a resource in the **held** state
- `release` requires a currently held resource (no double-release)
- A function cannot end (or `return`) while any resource is held
- `if` / `else` must leave the same held set on both branches
- A loop body must not change **net** holdings (acquire+release in the same iteration is allowed)

## Pipeline

```text
source .tiny
    → Lexer
    → Parser  (AST)
    → ResourceChecker + SymbolTable
    → Interpreter
           ↘ Profiler (per while-loop hit count)
           ↘ JITCompiler + CodeGenerator  (hot loops → bytecode)
```

The JIT only compiles numeric loops (no `acquire` / `release` / calls). If compilation fails, the interpreter keeps running the loop.

## Layout

```text
ResourceAwareJIT/
├── CMakeLists.txt
├── README.md
├── examples/
├── include/
├── src/
└── tests/
```
