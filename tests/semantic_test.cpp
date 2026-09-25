#include "Error.h"
#include "test_support.h"

#include <iostream>

static bool expectSemantic(const std::string& source, const std::string& needle) {
  try {
    (void)testutil::analyze(source);
    std::cerr << "FAIL: expected semantic error containing: " << needle << "\n";
    return false;
  } catch (const tinyrjit::SemanticError& error) {
    std::string msg = error.what();
    if (msg.find(needle) == std::string::npos) {
      std::cerr << "FAIL: semantic message mismatch\n" << msg << "\n";
      return false;
    }
    return true;
  }
}

int main() {
  int failed = 0;

  if (!expectSemantic("fn main() -> Int { y = 10; return 0; }\n", "has not been declared")) {
    failed++;
  } else {
    testutil::pass("undeclared variable");
  }

  if (!expectSemantic(
          "fn main() -> Int { let x: Int = 1; let x: Int = 2; return x; }\n", "Duplicate")) {
    failed++;
  } else {
    testutil::pass("duplicate variable");
  }

  if (!expectSemantic("fn main() -> Int { let x: Int = open(\"f\"); return 0; }\n",
                      "Cannot assign Handle to Int")) {
    failed++;
  } else {
    testutil::pass("type mismatch");
  }

  if (!expectSemantic("fn main() -> Int { return missing(1); }\n", "Unknown function")) {
    failed++;
  } else {
    testutil::pass("invalid function call");
  }

  {
    try {
      (void)testutil::analyze(
          "fn add(a: Int, b: Int) -> Int { return a + b; }\n"
          "fn main() -> Int { return add(1); }\n");
      failed += testutil::fail("expected argument count error");
    } catch (const tinyrjit::SemanticError& error) {
      if (std::string(error.what()).find("argument count") == std::string::npos) {
        failed += testutil::fail("argument count message");
      } else {
        testutil::pass("function argument count");
      }
    }
  }

  if (failed == 0) {
    std::cout << "All semantic tests passed.\n";
  }
  return failed == 0 ? 0 : 1;
}
