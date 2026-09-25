#include "Lexer.h"

#include "Error.h"

#include <cctype>
#include <unordered_map>

namespace tinyrjit {

const char* tokenTypeName(TokenType type) {
  switch (type) {
    case TokenType::Fn:
      return "FN";
    case TokenType::Let:
      return "LET";
    case TokenType::If:
      return "IF";
    case TokenType::Else:
      return "ELSE";
    case TokenType::While:
      return "WHILE";
    case TokenType::Return:
      return "RETURN";
    case TokenType::Int:
      return "INT";
    case TokenType::Handle:
      return "HANDLE";
    case TokenType::Identifier:
      return "IDENTIFIER";
    case TokenType::Number:
      return "NUMBER";
    case TokenType::String:
      return "STRING";
    case TokenType::Plus:
      return "PLUS";
    case TokenType::Minus:
      return "MINUS";
    case TokenType::Star:
      return "STAR";
    case TokenType::Slash:
      return "SLASH";
    case TokenType::Equal:
      return "ASSIGN";
    case TokenType::EqualEqual:
      return "EQUAL_EQUAL";
    case TokenType::Less:
      return "LESS";
    case TokenType::Greater:
      return "GREATER";
    case TokenType::LessEqual:
      return "LESS_EQUAL";
    case TokenType::GreaterEqual:
      return "GREATER_EQUAL";
    case TokenType::Arrow:
      return "ARROW";
    case TokenType::LeftParen:
      return "LEFT_PAREN";
    case TokenType::RightParen:
      return "RIGHT_PAREN";
    case TokenType::LeftBrace:
      return "LEFT_BRACE";
    case TokenType::RightBrace:
      return "RIGHT_BRACE";
    case TokenType::Colon:
      return "COLON";
    case TokenType::Comma:
      return "COMMA";
    case TokenType::Semicolon:
      return "SEMICOLON";
    case TokenType::EndOfFile:
      return "EOF";
    case TokenType::Unknown:
      return "UNKNOWN";
  }
  return "UNKNOWN";
}

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;
  for (;;) {
    Token token = nextToken();
    if (token.type == TokenType::Unknown) {
      throw LexicalError(token.line, "Unexpected character '" + token.lexeme + "'");
    }
    tokens.push_back(token);
    if (token.type == TokenType::EndOfFile) {
      break;
    }
  }
  return tokens;
}

bool Lexer::isAtEnd() const { return current_ >= source_.size(); }

char Lexer::peek() const { return isAtEnd() ? '\0' : source_[current_]; }

char Lexer::peekNext() const {
  if (current_ + 1 >= source_.size()) {
    return '\0';
  }
  return source_[current_ + 1];
}

char Lexer::advance() {
  char c = source_[current_++];
  if (c == '\n') {
    line_++;
  }
  return c;
}

bool Lexer::match(char expected) {
  if (isAtEnd() || source_[current_] != expected) {
    return false;
  }
  advance();
  return true;
}

void Lexer::skipWhitespaceAndComments() {
  for (;;) {
    char c = peek();
    if (c == ' ' || c == '\r' || c == '\t' || c == '\n') {
      advance();
      continue;
    }
    // Optional line comments keep example files readable.
    if (c == '#') {
      while (!isAtEnd() && peek() != '\n') {
        advance();
      }
      continue;
    }
    if (c == '/' && peekNext() == '/') {
      advance();
      advance();
      while (!isAtEnd() && peek() != '\n') {
        advance();
      }
      continue;
    }
    return;
  }
}

Token Lexer::makeToken(TokenType type) const {
  Token token;
  token.type = type;
  token.lexeme = source_.substr(start_, current_ - start_);
  token.line = line_;
  if (!token.lexeme.empty() && token.lexeme.back() == '\n') {
    token.line -= 1;
  }
  return token;
}

TokenType Lexer::keywordType(const std::string& text) const {
  static const std::unordered_map<std::string, TokenType> keywords = {
      {"fn", TokenType::Fn},
      {"let", TokenType::Let},
      {"if", TokenType::If},
      {"else", TokenType::Else},
      {"while", TokenType::While},
      {"return", TokenType::Return},
      {"Int", TokenType::Int},
      {"Handle", TokenType::Handle},
  };
  auto it = keywords.find(text);
  return it == keywords.end() ? TokenType::Identifier : it->second;
}

Token Lexer::identifier() {
  while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
    advance();
  }
  return makeToken(keywordType(source_.substr(start_, current_ - start_)));
}

Token Lexer::number() {
  while (std::isdigit(static_cast<unsigned char>(peek()))) {
    advance();
  }
  return makeToken(TokenType::Number);
}

Token Lexer::stringLiteral() {
  while (!isAtEnd() && peek() != '"' && peek() != '\n') {
    advance();
  }
  if (isAtEnd() || peek() != '"') {
    throw LexicalError(line_, "Unterminated string literal");
  }
  advance();
  Token token = makeToken(TokenType::String);
  if (token.lexeme.size() >= 2) {
    token.lexeme = token.lexeme.substr(1, token.lexeme.size() - 2);
  }
  return token;
}

Token Lexer::nextToken() {
  skipWhitespaceAndComments();
  start_ = current_;
  if (isAtEnd()) {
    Token eof;
    eof.type = TokenType::EndOfFile;
    eof.lexeme = "";
    eof.line = line_;
    return eof;
  }

  char c = advance();
  if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
    return identifier();
  }
  if (std::isdigit(static_cast<unsigned char>(c))) {
    return number();
  }

  switch (c) {
    case '(':
      return makeToken(TokenType::LeftParen);
    case ')':
      return makeToken(TokenType::RightParen);
    case '{':
      return makeToken(TokenType::LeftBrace);
    case '}':
      return makeToken(TokenType::RightBrace);
    case ':':
      return makeToken(TokenType::Colon);
    case ',':
      return makeToken(TokenType::Comma);
    case ';':
      return makeToken(TokenType::Semicolon);
    case '+':
      return makeToken(TokenType::Plus);
    case '-':
      return makeToken(match('>') ? TokenType::Arrow : TokenType::Minus);
    case '*':
      return makeToken(TokenType::Star);
    case '/':
      return makeToken(TokenType::Slash);
    case '"':
      return stringLiteral();
    case '=':
      return makeToken(match('=') ? TokenType::EqualEqual : TokenType::Equal);
    case '<':
      return makeToken(match('=') ? TokenType::LessEqual : TokenType::Less);
    case '>':
      return makeToken(match('=') ? TokenType::GreaterEqual : TokenType::Greater);
    default:
      return makeToken(TokenType::Unknown);
  }
}

}  // namespace tinyrjit
