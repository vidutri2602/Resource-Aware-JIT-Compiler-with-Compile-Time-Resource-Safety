#pragma once

#include "Error.h"
#include "Lexer.h"
#include "Parser.h"
#include "ResourceChecker.h"
#include "SemanticAnalyzer.h"

#include <iostream>
#include <string>

namespace testutil {

inline tinyrjit::Program parse(const std::string& source) {
  tinyrjit::Lexer lexer(source);
  tinyrjit::Parser parser(lexer.tokenize());
  return parser.parse();
}

inline tinyrjit::Program analyze(const std::string& source, tinyrjit::ResourceChecker* checker = nullptr) {
  tinyrjit::Program program = parse(source);
  tinyrjit::SemanticAnalyzer semantics;
  semantics.analyze(program);
  if (checker != nullptr) {
    checker->check(program);
  }
  return program;
}

inline int fail(const std::string& message) {
  std::cerr << "FAIL: " << message << "\n";
  return 1;
}

inline void pass(const std::string& name) { std::cout << "PASS: " << name << "\n"; }

}  // namespace testutil
