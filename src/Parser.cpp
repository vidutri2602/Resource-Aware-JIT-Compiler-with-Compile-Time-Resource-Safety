#include "Parser.h"

#include "Error.h"

#include <initializer_list>

namespace tinyrjit {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

Program Parser::parse() {
  Program program;
  while (!isAtEnd()) {
    program.functions.push_back(parseFunction());
  }
  if (program.functions.empty()) {
    error(peek(), "expected at least one function");
  }
  return program;
}

bool Parser::isAtEnd() const { return peek().type == TokenType::EndOfFile; }

const Token& Parser::peek() const { return tokens_.at(current_); }

const Token& Parser::previous() const { return tokens_.at(current_ - 1); }

const Token& Parser::advance() {
  if (!isAtEnd()) {
    current_++;
  }
  return previous();
}

bool Parser::check(TokenType type) const {
  if (isAtEnd()) {
    return false;
  }
  return peek().type == type;
}

bool Parser::match(TokenType type) {
  if (!check(type)) {
    return false;
  }
  advance();
  return true;
}

bool Parser::matchAny(std::initializer_list<TokenType> types) {
  for (TokenType type : types) {
    if (match(type)) {
      return true;
    }
  }
  return false;
}

const Token& Parser::consume(TokenType type, const std::string& message) {
  if (check(type)) {
    return advance();
  }
  error(peek(), message);
}

void Parser::error(const Token& token, const std::string& message) {
  throw ParseError(token.line, message + " (got '" + token.lexeme + "')");
}

TypeKind Parser::parseType() {
  if (match(TokenType::Int)) {
    return TypeKind::Int;
  }
  if (match(TokenType::Handle)) {
    return TypeKind::Handle;
  }
  error(peek(), "expected type Int or Handle");
}

std::unique_ptr<FunctionDecl> Parser::parseFunction() {
  consume(TokenType::Fn, "expected 'fn'");
  Token name = consume(TokenType::Identifier, "expected function name");
  auto fn = std::make_unique<FunctionDecl>();
  fn->name = name.lexeme;
  fn->line = name.line;
  consume(TokenType::LeftParen, "expected '(' after function name");
  fn->params = parseParameters();
  consume(TokenType::RightParen, "expected ')' after parameters");
  consume(TokenType::Arrow, "expected '->' after parameter list");
  fn->returnType = parseType();
  fn->body = parseBlock();
  return fn;
}

std::vector<Param> Parser::parseParameters() {
  std::vector<Param> params;
  if (check(TokenType::RightParen)) {
    return params;
  }
  do {
    Param p;
    Token name = consume(TokenType::Identifier, "expected parameter name");
    p.name = name.lexeme;
    p.line = name.line;
    consume(TokenType::Colon, "expected ':' after parameter name");
    p.type = parseType();
    params.push_back(p);
  } while (match(TokenType::Comma));
  return params;
}

std::vector<std::unique_ptr<Stmt>> Parser::parseBlock() {
  consume(TokenType::LeftBrace, "expected '{'");
  std::vector<std::unique_ptr<Stmt>> stmts;
  while (!check(TokenType::RightBrace) && !isAtEnd()) {
    stmts.push_back(parseStatement());
  }
  consume(TokenType::RightBrace, "expected '}'");
  return stmts;
}

std::unique_ptr<Stmt> Parser::parseStatement() {
  if (check(TokenType::Let)) {
    return parseVariableDeclaration();
  }
  if (check(TokenType::If)) {
    return parseIf();
  }
  if (check(TokenType::While)) {
    return parseWhile();
  }
  if (check(TokenType::Return)) {
    return parseReturn();
  }
  if (check(TokenType::Identifier)) {
    return parseAssignmentOrCall();
  }
  error(peek(), "expected statement");
}

std::unique_ptr<Stmt> Parser::parseVariableDeclaration() {
  Token letTok = consume(TokenType::Let, "expected 'let'");
  Token name = consume(TokenType::Identifier, "expected variable name");
  consume(TokenType::Colon, "expected ':' after variable name");
  TypeKind type = parseType();
  consume(TokenType::Equal, "expected '=' in declaration");
  auto stmt = std::make_unique<VariableDeclStmt>();
  stmt->line = letTok.line;
  stmt->name = name.lexeme;
  stmt->type = type;
  stmt->init = parseExpression();
  consume(TokenType::Semicolon, "expected ';' after declaration");
  return stmt;
}

std::unique_ptr<Stmt> Parser::parseAssignmentOrCall() {
  Token name = consume(TokenType::Identifier, "expected identifier");
  if (match(TokenType::Equal)) {
    auto stmt = std::make_unique<AssignmentStmt>();
    stmt->line = name.line;
    stmt->name = name.lexeme;
    stmt->value = parseExpression();
    consume(TokenType::Semicolon, "expected ';' after assignment");
    return stmt;
  }
  if (match(TokenType::LeftParen)) {
    auto call = std::make_unique<CallExpr>();
    call->line = name.line;
    call->callee = name.lexeme;
    if (!check(TokenType::RightParen)) {
      do {
        call->args.push_back(parseExpression());
      } while (match(TokenType::Comma));
    }
    consume(TokenType::RightParen, "expected ')' after arguments");
    consume(TokenType::Semicolon, "expected ';' after call");
    auto stmt = std::make_unique<ExpressionStmt>();
    stmt->line = name.line;
    stmt->expr = std::move(call);
    return stmt;
  }
  error(name, "expected assignment or function call");
}

std::unique_ptr<Stmt> Parser::parseIf() {
  Token tok = consume(TokenType::If, "expected 'if'");
  consume(TokenType::LeftParen, "expected '(' after if");
  auto stmt = std::make_unique<IfStmt>();
  stmt->line = tok.line;
  stmt->condition = parseExpression();
  consume(TokenType::RightParen, "expected ')' after condition");
  stmt->thenBranch = parseBlock();
  if (match(TokenType::Else)) {
    stmt->elseBranch = parseBlock();
  }
  return stmt;
}

std::unique_ptr<Stmt> Parser::parseWhile() {
  Token tok = consume(TokenType::While, "expected 'while'");
  consume(TokenType::LeftParen, "expected '(' after while");
  auto stmt = std::make_unique<WhileStmt>();
  stmt->line = tok.line;
  stmt->condition = parseExpression();
  consume(TokenType::RightParen, "expected ')' after condition");
  stmt->body = parseBlock();
  return stmt;
}

std::unique_ptr<Stmt> Parser::parseReturn() {
  Token tok = consume(TokenType::Return, "expected 'return'");
  auto stmt = std::make_unique<ReturnStmt>();
  stmt->line = tok.line;
  stmt->value = parseExpression();
  consume(TokenType::Semicolon, "expected ';' after return");
  return stmt;
}

std::unique_ptr<Expr> Parser::parseExpression() { return parseComparison(); }

std::unique_ptr<Expr> Parser::parseComparison() {
  auto expr = parseAddition();
  while (matchAny({TokenType::EqualEqual, TokenType::Less, TokenType::Greater,
                   TokenType::LessEqual, TokenType::GreaterEqual})) {
    TokenType op = previous().type;
    int line = previous().line;
    auto right = parseAddition();
    auto bin = std::make_unique<BinaryExpr>();
    bin->line = line;
    bin->op = op;
    bin->left = std::move(expr);
    bin->right = std::move(right);
    expr = std::move(bin);
  }
  return expr;
}

std::unique_ptr<Expr> Parser::parseAddition() {
  auto expr = parseMultiplication();
  while (matchAny({TokenType::Plus, TokenType::Minus})) {
    TokenType op = previous().type;
    int line = previous().line;
    auto right = parseMultiplication();
    auto bin = std::make_unique<BinaryExpr>();
    bin->line = line;
    bin->op = op;
    bin->left = std::move(expr);
    bin->right = std::move(right);
    expr = std::move(bin);
  }
  return expr;
}

std::unique_ptr<Expr> Parser::parseMultiplication() {
  auto expr = parseUnary();
  while (matchAny({TokenType::Star, TokenType::Slash})) {
    TokenType op = previous().type;
    int line = previous().line;
    auto right = parseUnary();
    auto bin = std::make_unique<BinaryExpr>();
    bin->line = line;
    bin->op = op;
    bin->left = std::move(expr);
    bin->right = std::move(right);
    expr = std::move(bin);
  }
  return expr;
}

std::unique_ptr<Expr> Parser::parseUnary() {
  if (match(TokenType::Minus)) {
    int line = previous().line;
    auto zero = std::make_unique<NumberExpr>();
    zero->line = line;
    zero->value = 0;
    auto bin = std::make_unique<BinaryExpr>();
    bin->line = line;
    bin->op = TokenType::Minus;
    bin->left = std::move(zero);
    bin->right = parseUnary();
    return bin;
  }
  return parsePrimary();
}

std::unique_ptr<Expr> Parser::parsePrimary() {
  if (match(TokenType::Number)) {
    auto n = std::make_unique<NumberExpr>();
    n->line = previous().line;
    n->value = std::stoi(previous().lexeme);
    return n;
  }
  if (match(TokenType::String)) {
    auto s = std::make_unique<StringExpr>();
    s->line = previous().line;
    s->value = previous().lexeme;
    return s;
  }
  if (match(TokenType::Identifier)) {
    Token name = previous();
    if (match(TokenType::LeftParen)) {
      auto call = std::make_unique<CallExpr>();
      call->line = name.line;
      call->callee = name.lexeme;
      if (!check(TokenType::RightParen)) {
        do {
          call->args.push_back(parseExpression());
        } while (match(TokenType::Comma));
      }
      consume(TokenType::RightParen, "expected ')' after arguments");
      return call;
    }
    auto v = std::make_unique<VariableExpr>();
    v->line = name.line;
    v->name = name.lexeme;
    return v;
  }
  if (match(TokenType::LeftParen)) {
    auto expr = parseExpression();
    consume(TokenType::RightParen, "expected ')' after expression");
    return expr;
  }
  error(peek(), "expected expression");
}

}  // namespace tinyrjit
