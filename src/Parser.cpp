#include "Parser.h"

#include <utility>

namespace rajit {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

bool Parser::isAtEnd() const { return peek().type == TokenType::EndOfFile; }

const Token& Parser::peek() const { return tokens_[current_]; }

const Token& Parser::previous() const { return tokens_[current_ - 1]; }

const Token& Parser::advance() {
  if (!isAtEnd()) {
    current_++;
  }
  return previous();
}

bool Parser::check(TokenType type) const { return !isAtEnd() && peek().type == type; }

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
  throw error(peek(), message);
}

ParseError Parser::error(const Token& token, const std::string& message) const {
  return ParseError(token.line, "parse error at line " + std::to_string(token.line) + ": " +
                                    message);
}

Program Parser::parse() {
  Program program;
  while (!isAtEnd()) {
    program.functions.push_back(function());
  }
  return program;
}

std::unique_ptr<FunctionStmt> Parser::function() {
  consume(TokenType::Fn, "expected 'fn'");
  Token name = consume(TokenType::Identifier, "expected function name");
  consume(TokenType::LeftParen, "expected '(' after function name");
  auto fn = std::make_unique<FunctionStmt>();
  fn->name = name.lexeme;
  fn->line = name.line;
  if (!check(TokenType::RightParen)) {
    do {
      Token param = consume(TokenType::Identifier, "expected parameter name");
      fn->params.push_back(param.lexeme);
    } while (match(TokenType::Comma));
  }
  consume(TokenType::RightParen, "expected ')' after parameters");
  fn->body = block();
  return fn;
}

std::unique_ptr<BlockStmt> Parser::block() {
  consume(TokenType::LeftBrace, "expected '{'");
  auto blockStmt = std::make_unique<BlockStmt>();
  blockStmt->line = previous().line;
  while (!check(TokenType::RightBrace) && !isAtEnd()) {
    blockStmt->statements.push_back(statement());
  }
  consume(TokenType::RightBrace, "expected '}' after block");
  return blockStmt;
}

std::unique_ptr<Stmt> Parser::statement() {
  if (match(TokenType::Let)) {
    return letOrAcquire();
  }
  if (match(TokenType::If)) {
    auto stmt = std::make_unique<IfStmt>();
    stmt->line = previous().line;
    consume(TokenType::LeftParen, "expected '(' after if");
    stmt->condition = expression();
    consume(TokenType::RightParen, "expected ')' after condition");
    stmt->thenBranch = block();
    if (match(TokenType::Else)) {
      stmt->elseBranch = block();
    }
    return stmt;
  }
  if (match(TokenType::While)) {
    auto stmt = std::make_unique<WhileStmt>();
    stmt->line = previous().line;
    stmt->loopId = nextLoopId_++;
    consume(TokenType::LeftParen, "expected '(' after while");
    stmt->condition = expression();
    consume(TokenType::RightParen, "expected ')' after condition");
    stmt->body = block();
    return stmt;
  }
  if (match(TokenType::Print)) {
    auto stmt = std::make_unique<PrintStmt>();
    stmt->line = previous().line;
    stmt->expr = expression();
    consume(TokenType::Semicolon, "expected ';' after print");
    return stmt;
  }
  if (match(TokenType::Return)) {
    auto stmt = std::make_unique<ReturnStmt>();
    stmt->line = previous().line;
    if (!check(TokenType::Semicolon)) {
      stmt->value = expression();
    }
    consume(TokenType::Semicolon, "expected ';' after return");
    return stmt;
  }
  if (match(TokenType::Release)) {
    auto stmt = std::make_unique<ReleaseStmt>();
    stmt->line = previous().line;
    Token name = consume(TokenType::Identifier, "expected resource name");
    stmt->name = name.lexeme;
    consume(TokenType::Semicolon, "expected ';' after release");
    return stmt;
  }
  if (check(TokenType::LeftBrace)) {
    return block();
  }

  if (check(TokenType::Identifier) && current_ + 1 < tokens_.size() &&
      tokens_[current_ + 1].type == TokenType::Equal) {
    Token name = advance();
    advance();  // =
    auto stmt = std::make_unique<AssignStmt>();
    stmt->line = name.line;
    stmt->name = name.lexeme;
    stmt->value = expression();
    consume(TokenType::Semicolon, "expected ';' after assignment");
    return stmt;
  }

  auto stmt = std::make_unique<ExprStmt>();
  stmt->line = peek().line;
  stmt->expr = expression();
  consume(TokenType::Semicolon, "expected ';' after expression");
  return stmt;
}

std::unique_ptr<Stmt> Parser::letOrAcquire() {
  int line = previous().line;
  Token name = consume(TokenType::Identifier, "expected variable name");
  consume(TokenType::Equal, "expected '=' after variable name");
  if (match(TokenType::Acquire)) {
    auto stmt = std::make_unique<AcquireStmt>();
    stmt->line = line;
    stmt->name = name.lexeme;
    if (match(TokenType::File)) {
      stmt->resourceKind = ResourceKind::File;
    } else if (match(TokenType::Mem)) {
      stmt->resourceKind = ResourceKind::Mem;
    } else if (match(TokenType::Lock)) {
      stmt->resourceKind = ResourceKind::Lock;
    } else {
      throw error(peek(), "expected resource kind 'file', 'mem', or 'lock'");
    }
    if (!check(TokenType::Semicolon)) {
      stmt->argument = expression();
    }
    consume(TokenType::Semicolon, "expected ';' after acquire");
    return stmt;
  }
  auto stmt = std::make_unique<LetStmt>();
  stmt->line = line;
  stmt->name = name.lexeme;
  stmt->initializer = expression();
  consume(TokenType::Semicolon, "expected ';' after let");
  return stmt;
}

std::unique_ptr<Expr> Parser::expression() { return equality(); }

std::unique_ptr<Expr> Parser::equality() {
  auto expr = comparison();
  while (matchAny({TokenType::EqualEqual, TokenType::BangEqual})) {
    Token op = previous();
    auto bin = std::make_unique<BinaryExpr>();
    bin->line = op.line;
    bin->op = op.type == TokenType::EqualEqual ? BinaryOp::Eq : BinaryOp::Ne;
    bin->left = std::move(expr);
    bin->right = comparison();
    expr = std::move(bin);
  }
  return expr;
}

std::unique_ptr<Expr> Parser::comparison() {
  auto expr = term();
  while (matchAny({TokenType::Less, TokenType::LessEqual, TokenType::Greater,
                   TokenType::GreaterEqual})) {
    Token op = previous();
    auto bin = std::make_unique<BinaryExpr>();
    bin->line = op.line;
    if (op.type == TokenType::Less) {
      bin->op = BinaryOp::Lt;
    } else if (op.type == TokenType::LessEqual) {
      bin->op = BinaryOp::Le;
    } else if (op.type == TokenType::Greater) {
      bin->op = BinaryOp::Gt;
    } else {
      bin->op = BinaryOp::Ge;
    }
    bin->left = std::move(expr);
    bin->right = term();
    expr = std::move(bin);
  }
  return expr;
}

std::unique_ptr<Expr> Parser::term() {
  auto expr = factor();
  while (matchAny({TokenType::Plus, TokenType::Minus})) {
    Token op = previous();
    auto bin = std::make_unique<BinaryExpr>();
    bin->line = op.line;
    bin->op = op.type == TokenType::Plus ? BinaryOp::Add : BinaryOp::Sub;
    bin->left = std::move(expr);
    bin->right = factor();
    expr = std::move(bin);
  }
  return expr;
}

std::unique_ptr<Expr> Parser::factor() {
  auto expr = unary();
  while (matchAny({TokenType::Star, TokenType::Slash, TokenType::Percent})) {
    Token op = previous();
    auto bin = std::make_unique<BinaryExpr>();
    bin->line = op.line;
    if (op.type == TokenType::Star) {
      bin->op = BinaryOp::Mul;
    } else if (op.type == TokenType::Slash) {
      bin->op = BinaryOp::Div;
    } else {
      bin->op = BinaryOp::Mod;
    }
    bin->left = std::move(expr);
    bin->right = unary();
    expr = std::move(bin);
  }
  return expr;
}

std::unique_ptr<Expr> Parser::unary() {
  if (match(TokenType::Minus)) {
    auto un = std::make_unique<UnaryExpr>();
    un->line = previous().line;
    un->op = UnaryOp::Negate;
    un->operand = unary();
    return un;
  }
  return primary();
}

std::unique_ptr<Expr> Parser::primary() {
  if (match(TokenType::Number)) {
    auto lit = std::make_unique<LiteralExpr>();
    lit->line = previous().line;
    lit->literalKind = LiteralKind::Integer;
    lit->intValue = std::stoll(previous().lexeme);
    return lit;
  }
  if (match(TokenType::True) || match(TokenType::False)) {
    auto lit = std::make_unique<LiteralExpr>();
    lit->line = previous().line;
    lit->literalKind = LiteralKind::Boolean;
    lit->boolValue = previous().type == TokenType::True || previous().lexeme == "true";
    return lit;
  }
  if (match(TokenType::String)) {
    auto lit = std::make_unique<LiteralExpr>();
    lit->line = previous().line;
    lit->literalKind = LiteralKind::String;
    lit->stringValue = previous().lexeme;
    return lit;
  }
  if (match(TokenType::Identifier)) {
    Token name = previous();
    if (match(TokenType::LeftParen)) {
      auto call = std::make_unique<CallExpr>();
      call->line = name.line;
      call->callee = name.lexeme;
      if (!check(TokenType::RightParen)) {
        do {
          call->args.push_back(expression());
        } while (match(TokenType::Comma));
      }
      consume(TokenType::RightParen, "expected ')' after arguments");
      return call;
    }
    auto var = std::make_unique<VariableExpr>();
    var->line = name.line;
    var->name = name.lexeme;
    return var;
  }
  if (match(TokenType::LeftParen)) {
    auto expr = expression();
    consume(TokenType::RightParen, "expected ')' after expression");
    return expr;
  }
  throw error(peek(), "expected expression");
}

}  // namespace rajit
