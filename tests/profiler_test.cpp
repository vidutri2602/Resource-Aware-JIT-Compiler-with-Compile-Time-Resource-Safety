#include "Interpreter.h"
#include "Profiler.h"
#include "test_support.h"

#include <iostream>

int main() {
  int failed = 0;

  tinyrjit::Profiler profiler(1000);
  profiler.recordFunctionCall("main");
  for (int i = 0; i < 1000; ++i) {
    profiler.recordFunctionCall("calculate");
  }

  if (profiler.getCallCount("main") != 1 || profiler.getCallCount("calculate") != 1000) {
    failed += testutil::fail("call counting");
  } else {
    testutil::pass("call counting");
  }

  if (profiler.isHot("main") || !profiler.isHot("calculate")) {
    failed += testutil::fail("hot function detection");
  } else {
    testutil::pass("hot function detection");
  }

  {
    tinyrjit::ResourceChecker checker;
    auto program = testutil::analyze(
        "fn calculate(n: Int) -> Int {\n"
        "  let sum: Int = 0;\n"
        "  let i: Int = 0;\n"
        "  while (i < n) { sum = sum + i; i = i + 1; }\n"
        "  return sum;\n"
        "}\n"
        "fn main() -> Int { return calculate(1000); }\n",
        &checker);
    tinyrjit::Profiler runtime(1000);
    tinyrjit::Interpreter interpreter(runtime, nullptr);
    (void)interpreter.interpret(program);
    if (!runtime.isHot("calculate")) {
      failed += testutil::fail("runtime hot loop heat");
    } else {
      testutil::pass("runtime hot loop heat");
    }
  }

  if (failed == 0) {
    std::cout << "All profiler tests passed.\n";
  }
  return failed == 0 ? 0 : 1;
}
