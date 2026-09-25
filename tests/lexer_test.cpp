#include "Lexer.h"
#include "Error.h"
#include "test_support.h"

#include <iostream>
#include <string>
#include <vector>

using tinyrjit::Lexer;
using tinyrjit::Token;
using tinyrjit::TokenType;

static bool expectTypes(const std::vector<Token>& tokens, const std::vector<TokenType>& types) {
  if (tokens.size() != types.size()) {
    return false;
  }
  for (std::size_t i = 0; i < types.size(); ++i) {
    if (tokens[i].type != types[i]) {
      return false;
    }
  }
  return true;
}

int main() {
  int failed = 0;

  {
    Lexer lexer("let x: Int = 10;");
    auto tokens = lexer.tokenize();
    if (!expectTypes(tokens, {TokenType::Let, TokenType::Identifier, TokenType::Colon, TokenType::Int,
                              TokenType::Equal, TokenType::Number, TokenType::Semicolon,
                              TokenType::EndOfFile})) {
      failed += testutil::fail("keywords/identifiers/numbers");
    } else if (tokens[1].lexeme != "x" || tokens[5].lexeme != "10") {
      failed += testutil::fail("lexeme values");
    } else {
      testutil::pass("keywords identifiers numbers");
    }
  }

  {
    Lexer lexer("fn if else while return Handle + - * / == <= >= ->");
    auto tokens = lexer.tokenize();
    if (tokens[0].type != TokenType::Fn || tokens[5].type != TokenType::Handle ||
        tokens[11].type != TokenType::LessEqual || tokens[13].type != TokenType::Arrow) {
      failed += testutil::fail("operators and remaining keywords");
    } else {
      testutil::pass("operators and keywords");
    }
  }

  {
    try {
      Lexer lexer("let x = 10 @ 2;");
      (void)lexer.tokenize();
      failed += testutil::fail("expected lexical error for '@'");
    } catch (const tinyrjit::LexicalError& error) {
      std::string msg = error.what();
      if (msg.find("Unexpected character '@'") == std::string::npos || error.line() != 1) {
        failed += testutil::fail("lexical error text");
      } else {
        testutil::pass("invalid character");
      }
    }
  }

  {
    Lexer lexer("\"hello\"\nident");
    auto tokens = lexer.tokenize();
    if (tokens[0].type != TokenType::String || tokens[0].lexeme != "hello" || tokens[1].line != 2) {
      failed += testutil::fail("string and line numbers");
    } else {
      testutil::pass("strings and line numbers");
    }
  }

  if (failed == 0) {
    std::cout << "All lexer tests passed.\n";
  }
  return failed == 0 ? 0 : 1;
}
