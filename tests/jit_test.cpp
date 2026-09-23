#include "Interpreter.h"
#include "Lexer.h"
#include "Parser.h"
#include "ResourceChecker.h"

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

static int failures = 0;

static void expect(bool cond, const std::string& message) {
  if (!cond) {
    std::cerr << "FAIL: " << message << "\n";
    failures++;
  }
}

static std::string run(const std::string& source, bool enableJit, std::int64_t threshold) {
  rajit::Lexer lexer(source);
  rajit::Parser parser(lexer.tokenize());
  auto program = parser.parse();
  rajit::ResourceChecker checker;
  checker.check(program);

  rajit::Profiler profiler(threshold);
  rajit::JITCompiler jit(profiler);
  jit.setEnabled(enableJit);
  std::ostringstream out;
  rajit::Interpreter interpreter(out, profiler, jit);
  interpreter.interpret(program);
  return out.str();
}

int main() {
  const std::string program = R"(
    fn main() {
      let i = 0;
      let sum = 0;
      while (i < 40) {
        sum = sum + i;
        i = i + 1;
      }
      print sum;
    }
  )";

  std::string interpreted = run(program, false, 1000);
  std::string jitted = run(program, true, 8);
  expect(interpreted == jitted, "JIT and interpreter agree");
  expect(interpreted.find("780") != std::string::npos, "sum 0..39 is 780");

  rajit::Lexer lexer(program);
  rajit::Parser parser(lexer.tokenize());
  auto ast = parser.parse();
  rajit::ResourceChecker checker;
  checker.check(ast);
  rajit::Profiler profiler(5);
  rajit::JITCompiler jit(profiler);
  jit.setEnabled(true);
  std::ostringstream out;
  rajit::Interpreter interpreter(out, profiler, jit);
  interpreter.interpret(ast);
  expect(jit.has(0), "hot loop was compiled");
  expect(out.str().find("780") != std::string::npos, "compiled run printed sum");

  if (failures) {
    std::cerr << failures << " jit tests failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "jit_test ok\n";
  return EXIT_SUCCESS;
}
