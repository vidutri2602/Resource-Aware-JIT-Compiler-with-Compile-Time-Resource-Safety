#include "Lexer.h"

#include <cctype>
#include <unordered_map>

namespace rajit {

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

std::vector<Token> Lexer::tokenize() {
  std::vector<Token> tokens;
  for (;;) {
    Token token = nextToken();
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
    column_ = 1;
  } else {
    column_++;
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
    if (c == '#') {
      while (!isAtEnd() && peek() != '\n') {
        advance();
      }
      continue;
    }
    if (c == '/' && peekNext() == '/') {
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
  token.column = startColumn_;
  if (!token.lexeme.empty() && token.lexeme.back() == '\n') {
    // line_ already advanced; keep column as recorded at start
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
      {"print", TokenType::Print},
      {"return", TokenType::Return},
      {"acquire", TokenType::Acquire},
      {"release", TokenType::Release},
      {"file", TokenType::File},
      {"mem", TokenType::Mem},
      {"lock", TokenType::Lock},
      {"true", TokenType::True},
      {"false", TokenType::False},
  };
  auto it = keywords.find(text);
  return it == keywords.end() ? TokenType::Identifier : it->second;
}

Token Lexer::identifier() {
  while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
    advance();
  }
  std::string text = source_.substr(start_, current_ - start_);
  return makeToken(keywordType(text));
}

Token Lexer::number() {
  while (std::isdigit(static_cast<unsigned char>(peek()))) {
    advance();
  }
  return makeToken(TokenType::Number);
}

Token Lexer::stringLiteral() {
  while (!isAtEnd() && peek() != '"') {
    advance();
  }
  if (isAtEnd()) {
    return makeToken(TokenType::Unknown);
  }
  advance();  // closing quote
  Token token = makeToken(TokenType::String);
  if (token.lexeme.size() >= 2) {
    token.lexeme = token.lexeme.substr(1, token.lexeme.size() - 2);
  }
  return token;
}

Token Lexer::nextToken() {
  skipWhitespaceAndComments();
  start_ = current_;
  startColumn_ = column_;
  if (isAtEnd()) {
    Token eof;
    eof.type = TokenType::EndOfFile;
    eof.line = line_;
    eof.column = column_;
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
    case ',':
      return makeToken(TokenType::Comma);
    case ';':
      return makeToken(TokenType::Semicolon);
    case '+':
      return makeToken(TokenType::Plus);
    case '-':
      return makeToken(TokenType::Minus);
    case '*':
      return makeToken(TokenType::Star);
    case '/':
      return makeToken(TokenType::Slash);
    case '%':
      return makeToken(TokenType::Percent);
    case '"':
      return stringLiteral();
    case '=':
      return makeToken(match('=') ? TokenType::EqualEqual : TokenType::Equal);
    case '!':
      return makeToken(match('=') ? TokenType::BangEqual : TokenType::Unknown);
    case '<':
      return makeToken(match('=') ? TokenType::LessEqual : TokenType::Less);
    case '>':
      return makeToken(match('=') ? TokenType::GreaterEqual : TokenType::Greater);
    default:
      return makeToken(TokenType::Unknown);
  }
}

}  // namespace rajit
