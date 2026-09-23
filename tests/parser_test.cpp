#include "Lexer.h"
#include "Parser.h"

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

int main() {
  auto program = parse(R"(
    fn add(a, b) { return a + b; }
    fn main() {
      let x = 1;
      if (x < 2) { print add(x, 3); } else { print 0; }
      while (x < 5) { x = x + 1; }
    }
  )");
  expect(program.functions.size() == 2, "two functions");
  expect(program.functions[0]->name == "add", "first fn is add");
  expect(program.functions[1]->name == "main", "second fn is main");
  expect(program.functions[1]->body->statements.size() == 3, "three statements in main");
  expect(program.functions[1]->body->statements[1]->kind == rajit::StmtKind::If, "if stmt");
  expect(program.functions[1]->body->statements[2]->kind == rajit::StmtKind::While, "while stmt");

  const auto& loop = static_cast<rajit::WhileStmt&>(*program.functions[1]->body->statements[2]);
  expect(loop.loopId == 0, "first loop id is 0");

  bool threw = false;
  try {
    parse("fn main() { let x = ; }");
  } catch (const rajit::ParseError&) {
    threw = true;
  }
  expect(threw, "invalid program throws");

  if (failures) {
    std::cerr << failures << " parser tests failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "parser_test ok\n";
  return EXIT_SUCCESS;
}
