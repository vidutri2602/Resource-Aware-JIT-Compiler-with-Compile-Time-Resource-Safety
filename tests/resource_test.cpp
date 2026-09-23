#include "Lexer.h"
#include "Parser.h"
#include "ResourceChecker.h"

#include <cstdlib>
#include <iostream>
#include <string>

static int failures = 0;

static void expect(bool cond, const std::string& message) {
  if (!cond) {
    std::cerr << "FAIL: " << message << "\n";
    failures++;
  }
}

static rajit::Program parse(const std::string& source) {
  rajit::Lexer lexer(source);
  rajit::Parser parser(lexer.tokenize());
  return parser.parse();
}

static bool checkOk(const std::string& source) {
  auto program = parse(source);
  rajit::ResourceChecker checker;
  checker.check(program);
  return true;
}

static bool checkFails(const std::string& source) {
  try {
    auto program = parse(source);
    rajit::ResourceChecker checker;
    checker.check(program);
    return false;
  } catch (const rajit::CheckError&) {
    return true;
  }
}

int main() {
  expect(checkOk(R"(
    fn main() {
      let f = acquire file "a.txt";
      release f;
    }
  )"),
         "balanced acquire/release");

  expect(checkFails(R"(
    fn main() {
      let f = acquire file "a.txt";
    }
  )"),
         "leak is rejected");

  expect(checkFails(R"(
    fn main() {
      let f = acquire file "a.txt";
      release f;
      release f;
    }
  )"),
         "double release is rejected");

  expect(checkFails(R"(
    fn main() {
      let x = 1;
      if (x) { let f = acquire file "a.txt"; } else { print 0; }
    }
  )"),
         "held on one branch only");

  expect(checkOk(R"(
    fn main() {
      let x = 1;
      let f = acquire file "a.txt";
      if (x) { release f; } else { release f; }
    }
  )"),
         "release on both branches");

  expect(checkFails(R"(
    fn main() {
      let i = 0;
      while (i < 3) {
        let f = acquire file "a.txt";
        i = i + 1;
      }
    }
  )"),
         "acquire in loop without matching release");

  expect(checkOk(R"(
    fn main() {
      let i = 0;
      while (i < 3) {
        let f = acquire file "a.txt";
        release f;
        i = i + 1;
      }
    }
  )"),
         "acquire+release in same iteration");

  if (failures) {
    std::cerr << failures << " resource tests failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "resource_test ok\n";
  return EXIT_SUCCESS;
}
