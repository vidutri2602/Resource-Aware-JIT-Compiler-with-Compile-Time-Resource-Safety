#pragma once

#include "Token.h"

#include <string>
#include <vector>

namespace tinyrjit {

// Converts source text into a token stream (lexical analysis).
class Lexer {
 public:
  explicit Lexer(std::string source);

  std::vector<Token> tokenize();

 private:
  std::string source_;
  std::size_t start_ = 0;
  std::size_t current_ = 0;
  int line_ = 1;

  bool isAtEnd() const;
  char peek() const;
  char peekNext() const;
  char advance();
  bool match(char expected);
  void skipWhitespaceAndComments();
  Token makeToken(TokenType type) const;
  Token identifier();
  Token number();
  Token stringLiteral();
  TokenType keywordType(const std::string& text) const;
  Token nextToken();
};

}  // namespace tinyrjit
