#pragma once

#include "AST.h"
#include "Lexer.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace rajit {

class ParseError : public std::runtime_error {
 public:
  int line;
  ParseError(int line, const std::string& message)
      : std::runtime_error(message), line(line) {}
};

class Parser {
 public:
  explicit Parser(std::vector<Token> tokens);
  Program parse();

 private:
  std::vector<Token> tokens_;
  std::size_t current_ = 0;
  int nextLoopId_ = 0;

  bool isAtEnd() const;
  const Token& peek() const;
  const Token& previous() const;
  const Token& advance();
  bool check(TokenType type) const;
  bool match(TokenType type);
  bool matchAny(std::initializer_list<TokenType> types);
  const Token& consume(TokenType type, const std::string& message);
  ParseError error(const Token& token, const std::string& message) const;

  std::unique_ptr<FunctionStmt> function();
  std::unique_ptr<BlockStmt> block();
  std::unique_ptr<Stmt> statement();
  std::unique_ptr<Stmt> letOrAcquire();
  std::unique_ptr<Expr> expression();
  std::unique_ptr<Expr> equality();
  std::unique_ptr<Expr> comparison();
  std::unique_ptr<Expr> term();
  std::unique_ptr<Expr> factor();
  std::unique_ptr<Expr> unary();
  std::unique_ptr<Expr> primary();
};

}  // namespace rajit
