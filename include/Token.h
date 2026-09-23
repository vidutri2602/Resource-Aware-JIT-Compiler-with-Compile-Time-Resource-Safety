#pragma once

#include <cstdint>
#include <string>

namespace rajit {

enum class TokenType {
  // Single-character
  LeftParen,
  RightParen,
  LeftBrace,
  RightBrace,
  Comma,
  Semicolon,
  Plus,
  Minus,
  Star,
  Slash,
  Percent,
  Equal,
  Less,
  Greater,

  // Two-character
  EqualEqual,
  BangEqual,
  LessEqual,
  GreaterEqual,

  // Literals
  Identifier,
  Number,
  String,

  // Keywords
  Fn,
  Let,
  If,
  Else,
  While,
  Print,
  Return,
  Acquire,
  Release,
  File,
  Mem,
  Lock,
  True,
  False,

  EndOfFile,
  Unknown
};

struct Token {
  TokenType type = TokenType::Unknown;
  std::string lexeme;
  int line = 1;
  int column = 1;
};

inline const char* tokenTypeName(TokenType type) {
  switch (type) {
    case TokenType::LeftParen: return "LeftParen";
    case TokenType::RightParen: return "RightParen";
    case TokenType::LeftBrace: return "LeftBrace";
    case TokenType::RightBrace: return "RightBrace";
    case TokenType::Comma: return "Comma";
    case TokenType::Semicolon: return "Semicolon";
    case TokenType::Plus: return "Plus";
    case TokenType::Minus: return "Minus";
    case TokenType::Star: return "Star";
    case TokenType::Slash: return "Slash";
    case TokenType::Percent: return "Percent";
    case TokenType::Equal: return "Equal";
    case TokenType::Less: return "Less";
    case TokenType::Greater: return "Greater";
    case TokenType::EqualEqual: return "EqualEqual";
    case TokenType::BangEqual: return "BangEqual";
    case TokenType::LessEqual: return "LessEqual";
    case TokenType::GreaterEqual: return "GreaterEqual";
    case TokenType::Identifier: return "Identifier";
    case TokenType::Number: return "Number";
    case TokenType::String: return "String";
    case TokenType::Fn: return "Fn";
    case TokenType::Let: return "Let";
    case TokenType::If: return "If";
    case TokenType::Else: return "Else";
    case TokenType::While: return "While";
    case TokenType::Print: return "Print";
    case TokenType::Return: return "Return";
    case TokenType::Acquire: return "Acquire";
    case TokenType::Release: return "Release";
    case TokenType::File: return "File";
    case TokenType::Mem: return "Mem";
    case TokenType::Lock: return "Lock";
    case TokenType::True: return "True";
    case TokenType::False: return "False";
    case TokenType::EndOfFile: return "EOF";
    case TokenType::Unknown: return "Unknown";
  }
  return "Unknown";
}

}  // namespace rajit
