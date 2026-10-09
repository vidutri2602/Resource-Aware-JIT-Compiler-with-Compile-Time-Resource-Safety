#pragma once

#include "AST.h"
#include "Profiler.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tinyrjit {

// Interpreter may call into JIT-compiled numeric functions without linking LLVM
// into frontend tests. The driver passes a JITCompiler; tests pass nullptr.
class CompiledFunctionHost {
 public:
  virtual ~CompiledFunctionHost() = default;
  virtual bool hasCompiled(const std::string& name) const = 0;
  virtual std::int32_t invoke(const std::string& name,
                              const std::vector<std::int32_t>& args) = 0;
};

struct Value {
  TypeKind type = TypeKind::Int;
  std::int32_t i = 0;
  int handleId = -1;
  std::string text;
};

class Interpreter {
 public:
  Interpreter(Profiler& profiler, CompiledFunctionHost* jit);

  std::int32_t interpret(const Program& program);

 private:
  using Env = std::unordered_map<std::string, Value>;

  Profiler& profiler_;
  CompiledFunctionHost* jit_;
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
