#include "AST.h"
#include "Error.h"
#include "test_support.h"

#include <iostream>

int main() {
  int failed = 0;

  {
    auto program = testutil::parse(
        "fn add(a: Int, b: Int) -> Int {\n"
        "  return a + b;\n"
        "}\n"
        "fn main() -> Int {\n"
        "  return add(10, 20);\n"
        "}\n");
    if (program.functions.size() != 2 || program.functions[0]->name != "add") {
      failed += testutil::fail("valid function parse");
    } else {
      testutil::pass("valid function");
    }
  }

  {
    try {
      (void)testutil::parse("fn main() -> Int { return 1 }");
      failed += testutil::fail("expected syntax error");
    } catch (const tinyrjit::ParseError&) {
      testutil::pass("invalid syntax");
    }
  }

  {
    auto program = testutil::parse(
        "fn main() -> Int {\n"
        "  return 1 + 2 * 3;\n"
        "}\n");
    auto* ret = dynamic_cast<tinyrjit::ReturnStmt*>(program.functions[0]->body[0].get());
    auto* bin = ret ? dynamic_cast<tinyrjit::BinaryExpr*>(ret->value.get()) : nullptr;
    auto* right = bin ? dynamic_cast<tinyrjit::BinaryExpr*>(bin->right.get()) : nullptr;
    if (!bin || bin->op != tinyrjit::TokenType::Plus || !right ||
        right->op != tinyrjit::TokenType::Star) {
      failed += testutil::fail("expression precedence a + (b * c)");
    } else {
      testutil::pass("expression precedence");
    }
  }

  {
    auto program = testutil::parse(
        "fn main() -> Int {\n"
        "  if (1 > 0) { return 1; } else { return 0; }\n"
        "}\n");
    if (dynamic_cast<tinyrjit::IfStmt*>(program.functions[0]->body[0].get()) == nullptr) {
      failed += testutil::fail("if/else");
    } else {
      testutil::pass("if/else");
    }
  }

  {
    auto program = testutil::parse(
        "fn main() -> Int {\n"
        "  while (0 < 1) { return 1; }\n"
        "  return 0;\n"
        "}\n");
    if (dynamic_cast<tinyrjit::WhileStmt*>(program.functions[0]->body[0].get()) == nullptr) {
      failed += testutil::fail("while");
    } else {
      testutil::pass("while");
    }
  }

  if (failed == 0) {
    std::cout << "All parser tests passed.\n";
  }
  return failed == 0 ? 0 : 1;
}
