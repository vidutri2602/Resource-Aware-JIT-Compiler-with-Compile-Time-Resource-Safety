#include "Lexer.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using rajit::Lexer;
using rajit::TokenType;

static int failures = 0;

static void expect(bool cond, const std::string& message) {
  if (!cond) {
    std::cerr << "FAIL: " << message << "\n";
    failures++;
  }
}

int main() {
  Lexer lexer("fn main() { let x = 12; print x; }");
  auto tokens = lexer.tokenize();
  expect(!tokens.empty(), "produced tokens");
  expect(tokens.front().type == TokenType::Fn, "starts with fn");
  expect(tokens.back().type == TokenType::EndOfFile, "ends with EOF");

  std::vector<TokenType> expected = {
      TokenType::Fn,         TokenType::Identifier, TokenType::LeftParen,
      TokenType::RightParen, TokenType::LeftBrace,  TokenType::Let,
      TokenType::Identifier, TokenType::Equal,      TokenType::Number,
      TokenType::Semicolon,  TokenType::Print,      TokenType::Identifier,
      TokenType::Semicolon,  TokenType::RightBrace, TokenType::EndOfFile};
  expect(tokens.size() == expected.size(), "token count");
  for (std::size_t i = 0; i < expected.size() && i < tokens.size(); ++i) {
    expect(tokens[i].type == expected[i], "token " + std::to_string(i));
  }

  Lexer ops("a == b != c <= d >= e < f > g // comment\n ident");
  auto opTokens = ops.tokenize();
  expect(opTokens[1].type == TokenType::EqualEqual, "==");
  expect(opTokens[3].type == TokenType::BangEqual, "!=");
  expect(opTokens[5].type == TokenType::LessEqual, "<=");
  expect(opTokens[7].type == TokenType::GreaterEqual, ">=");

  if (failures) {
    std::cerr << failures << " lexer tests failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "lexer_test ok\n";
  return EXIT_SUCCESS;
}
