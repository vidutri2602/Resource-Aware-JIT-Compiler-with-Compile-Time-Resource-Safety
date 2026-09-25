#include "Interpreter.h"
#include "Profiler.h"
#include "test_support.h"

#include <iostream>

static std::int32_t run(const std::string& source) {
  tinyrjit::ResourceChecker checker;
  tinyrjit::Program program = testutil::analyze(source, &checker);
  tinyrjit::Profiler profiler(1000);
  tinyrjit::Interpreter interpreter(profiler, nullptr);
  return interpreter.interpret(program);
}

int main() {
  int failed = 0;

  if (run("fn main() -> Int { return 10 + 20 * 2; }\n") != 50) {
    failed += testutil::fail("arithmetic");
  } else {
    testutil::pass("arithmetic");
  }

  if (run("fn main() -> Int { let x: Int = 10; let y: Int = 20; return x + y; }\n") != 30) {
    failed += testutil::fail("variables");
  } else {
    testutil::pass("variables");
  }

  if (run("fn add(a: Int, b: Int) -> Int { return a + b; }\n"
          "fn main() -> Int { return add(10, 20); }\n") != 30) {
    failed += testutil::fail("functions");
  } else {
    testutil::pass("functions");
  }

  if (run("fn main() -> Int { let x: Int = 10; if (x > 5) { return 1; } else { return 0; } }\n") !=
      1) {
    failed += testutil::fail("if");
  } else {
    testutil::pass("if");
  }

  if (run("fn calculate(n: Int) -> Int {\n"
          "  let sum: Int = 0;\n"
          "  let i: Int = 0;\n"
          "  while (i < n) { sum = sum + i; i = i + 1; }\n"
          "  return sum;\n"
          "}\n"
          "fn main() -> Int { return calculate(10); }\n") != 45) {
    failed += testutil::fail("loops");
  } else {
    testutil::pass("loops");
  }

  if (failed == 0) {
    std::cout << "All interpreter tests passed.\n";
  }
  return failed == 0 ? 0 : 1;
}
