#pragma once

#include "AST.h"
#include "Token.h"

#include <initializer_list>
#include <string>
#include <vector>

namespace tinyrjit {

// Recursive-descent parser: tokens -> AST (syntax analysis).
class Parser {
 public:
  explicit Parser(std::vector<Token> tokens);

  Program parse();

 private:
  std::vector<Token> tokens_;
  std::size_t current_ = 0;

  bool isAtEnd() const;
  const Token& peek() const;
  const Token& previous() const;
  const Token& advance();
  bool check(TokenType type) const;
  bool match(TokenType type);
  bool matchAny(std::initializer_list<TokenType> types);
  const Token& consume(TokenType type, const std::string& message);
  [[noreturn]] void error(const Token& token, const std::string& message);

  std::unique_ptr<FunctionDecl> parseFunction();
  std::vector<Param> parseParameters();
  TypeKind parseType();
  std::vector<std::unique_ptr<Stmt>> parseBlock();
  std::unique_ptr<Stmt> parseStatement();
  std::unique_ptr<Stmt> parseVariableDeclaration();
  std::unique_ptr<Stmt> parseAssignmentOrCall();
  std::unique_ptr<Stmt> parseIf();
  std::unique_ptr<Stmt> parseWhile();
  std::unique_ptr<Stmt> parseReturn();

  std::unique_ptr<Expr> parseExpression();
  std::unique_ptr<Expr> parseComparison();
  std::unique_ptr<Expr> parseAddition();
  std::unique_ptr<Expr> parseMultiplication();
  std::unique_ptr<Expr> parseUnary();
  std::unique_ptr<Expr> parsePrimary();
};

}  // namespace tinyrjit
