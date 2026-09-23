#pragma once

#include "AST.h"
#include "JITCompiler.h"
#include "Profiler.h"

#include <cstdint>
#include <iosfwd>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace rajit {

class RuntimeError : public std::runtime_error {
 public:
  int line;
  RuntimeError(int line, const std::string& message)
      : std::runtime_error(message), line(line) {}
};

enum class ValueKind { Int, Bool, String, Resource, Null };

struct ResourceHandle {
  ResourceKind kind = ResourceKind::File;
  std::int64_t id = 0;
  std::string label;
  bool held = false;
};

struct Value {
  ValueKind kind = ValueKind::Null;
  std::int64_t i = 0;
  bool b = false;
  std::string s;
  ResourceHandle resource;

  static Value makeInt(std::int64_t v);
  static Value makeBool(bool v);
  static Value makeString(std::string v);
  static Value makeResource(ResourceHandle h);
  static Value makeNull();

  bool isTruthy() const;
  std::string toString() const;
};

class Interpreter {
 public:
  Interpreter(std::ostream& output, Profiler& profiler, JITCompiler& jit);

  Value interpret(const Program& program);
  const Profiler& profiler() const { return profiler_; }

 private:
  struct Environment {
    std::unordered_map<std::string, Value> values;
    Environment* parent = nullptr;

    bool assign(const std::string& name, Value value);
    void define(const std::string& name, Value value);
    Value* get(const std::string& name);
  };

  struct ReturnJump {
    Value value;
  };

  std::ostream& output_;
  Profiler& profiler_;
  JITCompiler& jit_;
  std::unordered_map<std::string, const FunctionStmt*> functions_;
  std::int64_t nextResourceId_ = 1;

  Value executeFunction(const FunctionStmt& fn, const std::vector<Value>& args);
  void execute(const Stmt& stmt, Environment& env);
  Value evaluate(const Expr& expr, Environment& env);

  std::vector<std::string> collectIntLocals(Environment& env) const;
  void syncEnvFromInts(Environment& env,
                       const std::unordered_map<std::string, std::int64_t>& ints) const;
};

}  // namespace rajit
