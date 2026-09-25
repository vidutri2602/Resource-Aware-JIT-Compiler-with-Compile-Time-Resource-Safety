#pragma once

#include <stdexcept>
#include <string>

namespace tinyrjit {

class CompilerError : public std::runtime_error {
 public:
  CompilerError(const std::string& kind, int line, const std::string& message)
      : std::runtime_error(format(kind, line, message)), line_(line) {}

  int line() const { return line_; }

  static std::string format(const std::string& kind, int line, const std::string& message) {
    if (line > 0) {
      return "ERROR [Line " + std::to_string(line) + "]:\n" + kind + ":\n" + message;
    }
    return std::string("ERROR:\n") + kind + ":\n" + message;
  }

 private:
  int line_;
};

class LexicalError : public CompilerError {
 public:
  LexicalError(int line, const std::string& message)
      : CompilerError("Lexical Error", line, message) {}
};

class ParseError : public CompilerError {
 public:
  ParseError(int line, const std::string& message)
      : CompilerError("Syntax Error", line, message) {}
};

class SemanticError : public CompilerError {
 public:
  SemanticError(int line, const std::string& message)
      : CompilerError("Semantic Error", line, message) {}
};

class ResourceError : public CompilerError {
 public:
  ResourceError(int line, const std::string& message)
      : CompilerError("Resource Error", line, message) {}
};

class RuntimeError : public CompilerError {
 public:
  RuntimeError(int line, const std::string& message)
      : CompilerError("Runtime Error", line, message) {}
};

class JITError : public CompilerError {
 public:
  JITError(int line, const std::string& message)
      : CompilerError("JIT Error", line, message) {}
};

}  // namespace tinyrjit
