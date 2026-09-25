#include "Error.h"
#include "ResourceChecker.h"
#include "test_support.h"

#include <iostream>

static const char* kValid =
    "fn main() -> Int {\n"
    "  let h: Handle = open(\"data.txt\");\n"
    "  use(h);\n"
    "  close(h);\n"
    "  return 0;\n"
    "}\n";

static bool expectResource(const std::string& source, const std::string& needle) {
  try {
    tinyrjit::ResourceChecker checker;
    (void)testutil::analyze(source, &checker);
    std::cerr << "FAIL: expected resource error containing: " << needle << "\n";
    return false;
  } catch (const tinyrjit::ResourceError& error) {
    std::string msg = error.what();
    if (msg.find(needle) == std::string::npos) {
      std::cerr << "FAIL: resource message mismatch\n" << msg << "\n";
      return false;
    }
    return true;
  }
}

int main() {
  int failed = 0;

  {
    tinyrjit::ResourceChecker checker;
    try {
      (void)testutil::analyze(kValid, &checker);
      if (checker.trace().size() < 4) {
        failed += testutil::fail("valid handle trace too short");
      } else {
        testutil::pass("valid Handle");
      }
    } catch (const std::exception& error) {
      failed += testutil::fail(std::string("valid Handle threw: ") + error.what());
    }
  }

  if (!expectResource(
          "fn main() -> Int { let h: Handle = open(\"data.txt\"); close(h); close(h); return 0; }\n",
          "already closed")) {
    failed++;
  } else {
    testutil::pass("double close");
  }

  if (!expectResource(
          "fn main() -> Int { let h: Handle = open(\"data.txt\"); close(h); use(h); return 0; }\n",
          "Cannot use closed Handle")) {
    failed++;
  } else {
    testutil::pass("use after close");
  }

  if (!expectResource(
          "fn main() -> Int {\n"
          "  let src: Handle = open(\"data.txt\");\n"
          "  close(src);\n"
          "  let h: Handle = src;\n"
          "  use(h);\n"
          "  return 0;\n"
          "}\n",
          "has not been opened")) {
    failed++;
  } else {
    testutil::pass("use before open");
  }

  if (!expectResource("fn main() -> Int { let h: Handle = open(\"data.txt\"); return 0; }\n",
                      "remains open")) {
    failed++;
  } else {
    testutil::pass("unclosed Handle");
  }

  if (failed == 0) {
    std::cout << "All resource tests passed.\n";
  }
  return failed == 0 ? 0 : 1;
}
