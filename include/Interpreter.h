#pragma once

#include "AST.h"
#include "Profiler.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tinyrjit {

class JITCompiler;

struct Value {
  TypeKind type = TypeKind::Int;
  std::int32_t i = 0;
  int handleId = -1;
  std::string text;
};

class Interpreter {
 public:
  Interpreter(Profiler& profiler, JITCompiler* jit);

  std::int32_t interpret(const Program& program);

 private:
  using Env = std::unordered_map<std::string, Value>;

  Profiler& profiler_;
  JITCompiler* jit_;
  const Program* program_ = nullptr;
  std::string currentFunction_;
  std::unordered_map<std::string, const FunctionDecl*> functions_;
  int nextHandle_ = 1;
  struct HandleObject {
    std::string path;
    bool open = false;
    bool used = false;
  };
  std::unordered_map<int, HandleObject> handles_;

  Value call(const std::string& name, const std::vector<Value>& args, int line);
  Value eval(const Expr& expr, Env& env);
  void execStmt(const Stmt& stmt, Env& env, bool& returned, Value& ret);
};

}  // namespace tinyrjit
