#pragma once

#include <string>

namespace tinyrjit {

// Token kinds recognized by the TinyRAJIT lexer (lexical analysis).
enum class TokenType {
  // Keywords
  Fn,
  Let,
  If,
  Else,
  While,
  Return,
  Int,
  Handle,

  // Literals and names
  Identifier,
  Number,
  String,

  // Operators
  Plus,
  Minus,
  Star,
  Slash,
  Equal,        // =
  EqualEqual,   // ==
  Less,
  Greater,
  LessEqual,
  GreaterEqual,
  Arrow,        // ->

  // Punctuation
  LeftParen,
  RightParen,
  LeftBrace,
  RightBrace,
  Colon,
  Comma,
  Semicolon,

  EndOfFile,
  Unknown
};

struct Token {
  TokenType type = TokenType::Unknown;
  std::string lexeme;
  int line = 1;
};

const char* tokenTypeName(TokenType type);

}  // namespace tinyrjit
