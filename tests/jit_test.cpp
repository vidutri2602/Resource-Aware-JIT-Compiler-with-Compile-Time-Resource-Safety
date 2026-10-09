#include "CodeGenerator.h"
#include "Error.h"
#include "Interpreter.h"
#include "JITCompiler.h"
#include "JITPolicy.h"
#include "Profiler.h"
#include "ResourceChecker.h"
#include "test_support.h"

#include <iostream>
#include <string>
#include <unordered_set>

using namespace tinyrjit;

int main() {
  int failed = 0;

  // Test 1: LLVM IR Generation for numeric functions
  {
    std::string source =
        "fn add(a: Int, b: Int) -> Int {\n"
        "  return a + b;\n"
        "}\n"
        "fn main() -> Int {\n"
        "  return add(10, 20);\n"
        "}\n";
    ResourceChecker checker;
    Program program = testutil::analyze(source, &checker);
    auto eligible = JITPolicy::computeEligible(program, checker);

    if (eligible.count("add") == 0 || eligible.count("main") == 0) {
      failed += testutil::fail("eligibility calculation for numeric functions");
    } else {
      testutil::pass("numeric eligibility calculation");
    }

    try {
      CodeGenerator codegen;
      std::string ir = codegen.generate(program, eligible);
      if (ir.find("define i32 @add(i32 %a, i32 %b)") == std::string::npos ||
          ir.find("define i32 @main()") == std::string::npos) {
        failed += testutil::fail("LLVM IR output content");
      } else {
        testutil::pass("LLVM IR generation & verification");
      }
    } catch (const CompilerError& err) {
      failed += testutil::fail(std::string("IR generation threw: ") + err.what());
    }
  }

  // Test 2: Native ORC JIT execution of arithmetic functions
  {
    std::string source =
        "fn multiply_add(x: Int, y: Int, z: Int) -> Int {\n"
        "  return x * y + z;\n"
        "}\n"
        "fn main() -> Int {\n"
        "  return multiply_add(6, 7, 8);\n"
        "}\n";
    ResourceChecker checker;
    Program program = testutil::analyze(source, &checker);
    std::unordered_set<std::string> eligible = {"multiply_add", "main"};

    try {
      JITCompiler jit;
      jit.compile(program, eligible);

      if (!jit.hasCompiled("multiply_add")) {
        failed += testutil::fail("JIT compilation tracking");
      } else {
        testutil::pass("JIT compilation tracking");
      }

      std::int32_t res = jit.invoke("multiply_add", {6, 7, 8});
      if (res != 50) {
        failed += testutil::fail("JIT invoke result: expected 50, got " + std::to_string(res));
      } else {
        testutil::pass("JIT native execution (multiply_add 6*7+8 = 50)");
      }
    } catch (const CompilerError& err) {
      failed += testutil::fail(std::string("JIT compilation/invoke threw: ") + err.what());
    }
  }

  // Test 3: Native ORC JIT execution with control flow (loops & branches)
  {
    std::string source =
        "fn loop_sum(n: Int) -> Int {\n"
        "  let sum: Int = 0;\n"
        "  let i: Int = 1;\n"
        "  while (i <= n) {\n"
        "    if (i == 5) {\n"
        "      sum = sum + 50;\n"
        "    } else {\n"
        "      sum = sum + i;\n"
        "    }\n"
        "    i = i + 1;\n"
        "  }\n"
        "  return sum;\n"
        "}\n"
        "fn main() -> Int {\n"
        "  return loop_sum(10);\n"
        "}\n";
    ResourceChecker checker;
    Program program = testutil::analyze(source, &checker);
    std::unordered_set<std::string> eligible = {"loop_sum", "main"};

    try {
      JITCompiler jit;
      jit.compile(program, eligible);
      // sum for 1..10 where i=5 adds 50:
      // (1+2+3+4) + 50 + (6+7+8+9+10) = 10 + 50 + 40 = 100
      std::int32_t jitRes = jit.invoke("loop_sum", {10});
      if (jitRes != 100) {
        failed += testutil::fail("JIT loop/branch result: expected 100, got " + std::to_string(jitRes));
      } else {
        testutil::pass("JIT loop & branch native execution");
      }
    } catch (const CompilerError& err) {
      failed += testutil::fail(std::string("JIT control flow threw: ") + err.what());
    }
  }

  // Test 4: Parity comparison between Interpreter and JIT
  {
    std::string source =
        "fn fib(n: Int) -> Int {\n"
        "  if (n <= 1) {\n"
        "    return n;\n"
        "  } else {\n"
        "    return fib(n - 1) + fib(n - 2);\n"
        "  }\n"
        "}\n"
        "fn main() -> Int {\n"
        "  return fib(10);\n"
        "}\n";
    ResourceChecker checker;
    Program program = testutil::analyze(source, &checker);
    auto eligible = JITPolicy::computeEligible(program, checker);

    Profiler profiler(1000);
    Interpreter interp(profiler, nullptr);
    std::int32_t interpVal = interp.interpret(program);

    JITCompiler jit;
    jit.compile(program, eligible);
    std::int32_t jitVal = jit.invoke("main", {});

    if (interpVal != 55 || jitVal != 55 || interpVal != jitVal) {
      failed += testutil::fail("Interpreter and JIT parity: interp=" + std::to_string(interpVal) +
                               ", jit=" + std::to_string(jitVal));
    } else {
      testutil::pass("Interpreter and JIT parity check (fib(10) == 55)");
    }
  }

  // Test 5: Handle resources excluded from JIT eligibility
  {
    std::string source =
        "fn work() -> Int {\n"
        "  let h: Handle = open(\"data.txt\");\n"
        "  use(h);\n"
        "  close(h);\n"
        "  return 42;\n"
        "}\n"
        "fn main() -> Int {\n"
        "  return work();\n"
        "}\n";
    ResourceChecker checker;
    Program program = testutil::analyze(source, &checker);
    auto eligible = JITPolicy::computeEligible(program, checker);

    if (eligible.count("work") != 0) {
      failed += testutil::fail("Handle function must not be JIT-eligible");
    } else {
      testutil::pass("Handle resource function excluded from JIT");
    }
  }

  // Test 6: Invocation of uncompiled function throws JITError
  {
    JITCompiler jit;
    try {
      jit.invoke("nonexistent", {});
      failed += testutil::fail("invoke on uncompiled function should throw");
    } catch (const JITError&) {
      testutil::pass("uncompiled function invocation rejection");
    }
  }

  if (failed == 0) {
    std::cout << "All JIT tests passed.\n";
  }
  return failed == 0 ? 0 : 1;
}
