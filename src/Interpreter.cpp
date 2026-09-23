#include "Interpreter.h"

#include <iostream>
#include <utility>

namespace rajit {

Value Value::makeInt(std::int64_t v) {
  Value value;
  value.kind = ValueKind::Int;
  value.i = v;
  return value;
}

Value Value::makeBool(bool v) {
  Value value;
  value.kind = ValueKind::Bool;
  value.b = v;
  value.i = v ? 1 : 0;
  return value;
}

Value Value::makeString(std::string v) {
  Value value;
  value.kind = ValueKind::String;
  value.s = std::move(v);
  return value;
}

Value Value::makeResource(ResourceHandle h) {
  Value value;
  value.kind = ValueKind::Resource;
  value.resource = std::move(h);
  return value;
}

Value Value::makeNull() { return Value{}; }

bool Value::isTruthy() const {
  switch (kind) {
    case ValueKind::Int:
      return i != 0;
    case ValueKind::Bool:
      return b;
    case ValueKind::String:
      return !s.empty();
    case ValueKind::Resource:
      return resource.held;
    case ValueKind::Null:
      return false;
  }
  return false;
}

std::string Value::toString() const {
  switch (kind) {
    case ValueKind::Int:
      return std::to_string(i);
    case ValueKind::Bool:
      return b ? "true" : "false";
    case ValueKind::String:
      return s;
    case ValueKind::Resource:
      return resourceKindName(resource.kind) + "#" + std::to_string(resource.id);
    case ValueKind::Null:
      return "null";
  }
  return "null";
}

bool Interpreter::Environment::assign(const std::string& name, Value value) {
  auto it = values.find(name);
  if (it != values.end()) {
    it->second = std::move(value);
    return true;
  }
  if (parent) {
    return parent->assign(name, std::move(value));
  }
  return false;
}

void Interpreter::Environment::define(const std::string& name, Value value) {
  values[name] = std::move(value);
}

Value* Interpreter::Environment::get(const std::string& name) {
  auto it = values.find(name);
  if (it != values.end()) {
    return &it->second;
  }
  if (parent) {
    return parent->get(name);
  }
  return nullptr;
}

Interpreter::Interpreter(std::ostream& output, Profiler& profiler, JITCompiler& jit)
    : output_(output), profiler_(profiler), jit_(jit) {}

namespace {

RuntimeError rt(int line, const std::string& message) {
  return RuntimeError(line, "runtime error at line " + std::to_string(line) + ": " + message);
}

Value binary(const BinaryExpr& expr, const Value& left, const Value& right) {
  auto asInt = [](const Value& v) -> std::int64_t {
    if (v.kind == ValueKind::Int) {
      return v.i;
    }
    if (v.kind == ValueKind::Bool) {
      return v.b ? 1 : 0;
    }
    throw rt(0, "operands must be numeric");
  };

  switch (expr.op) {
    case BinaryOp::Add:
      return Value::makeInt(asInt(left) + asInt(right));
    case BinaryOp::Sub:
      return Value::makeInt(asInt(left) - asInt(right));
    case BinaryOp::Mul:
      return Value::makeInt(asInt(left) * asInt(right));
    case BinaryOp::Div: {
      auto b = asInt(right);
      if (b == 0) {
        throw rt(expr.line, "division by zero");
      }
      return Value::makeInt(asInt(left) / b);
    }
    case BinaryOp::Mod: {
      auto b = asInt(right);
      if (b == 0) {
        throw rt(expr.line, "division by zero");
      }
      return Value::makeInt(asInt(left) % b);
    }
    case BinaryOp::Eq:
      return Value::makeBool(asInt(left) == asInt(right));
    case BinaryOp::Ne:
      return Value::makeBool(asInt(left) != asInt(right));
    case BinaryOp::Lt:
      return Value::makeBool(asInt(left) < asInt(right));
    case BinaryOp::Le:
      return Value::makeBool(asInt(left) <= asInt(right));
    case BinaryOp::Gt:
      return Value::makeBool(asInt(left) > asInt(right));
    case BinaryOp::Ge:
      return Value::makeBool(asInt(left) >= asInt(right));
  }
  return Value::makeNull();
}

}  // namespace

Value Interpreter::interpret(const Program& program) {
  functions_.clear();
  for (const auto& fn : program.functions) {
    functions_[fn->name] = fn.get();
  }
  auto it = functions_.find("main");
  if (it == functions_.end()) {
    throw rt(1, "missing main");
  }
  return executeFunction(*it->second, {});
}

Value Interpreter::executeFunction(const FunctionStmt& fn, const std::vector<Value>& args) {
  Environment env;
  if (args.size() != fn.params.size()) {
    throw rt(fn.line, "argument count mismatch");
  }
  for (std::size_t i = 0; i < fn.params.size(); ++i) {
    env.define(fn.params[i], args[i]);
  }
  try {
    execute(*fn.body, env);
  } catch (const ReturnJump& ret) {
    return ret.value;
  }
  return Value::makeInt(0);
}

void Interpreter::execute(const Stmt& stmt, Environment& env) {
  switch (stmt.kind) {
    case StmtKind::Let: {
      const auto& s = static_cast<const LetStmt&>(stmt);
      env.define(s.name, evaluate(*s.initializer, env));
      break;
    }
    case StmtKind::Assign: {
      const auto& s = static_cast<const AssignStmt&>(stmt);
      if (!env.assign(s.name, evaluate(*s.value, env))) {
        throw rt(s.line, "undeclared variable '" + s.name + "'");
      }
      break;
    }
    case StmtKind::Expr: {
      const auto& s = static_cast<const ExprStmt&>(stmt);
      evaluate(*s.expr, env);
      break;
    }
    case StmtKind::Print: {
      const auto& s = static_cast<const PrintStmt&>(stmt);
      output_ << evaluate(*s.expr, env).toString() << "\n";
      break;
    }
    case StmtKind::Return: {
      const auto& s = static_cast<const ReturnStmt&>(stmt);
      Value value = s.value ? evaluate(*s.value, env) : Value::makeNull();
      throw ReturnJump{std::move(value)};
    }
    case StmtKind::If: {
      const auto& s = static_cast<const IfStmt&>(stmt);
      if (evaluate(*s.condition, env).isTruthy()) {
        execute(*s.thenBranch, env);
      } else if (s.elseBranch) {
        execute(*s.elseBranch, env);
      }
      break;
    }
    case StmtKind::While: {
      const auto& s = static_cast<const WhileStmt&>(stmt);
      while (evaluate(*s.condition, env).isTruthy()) {
        profiler_.hit(s.loopId);
        if (jit_.enabled() && jit_.has(s.loopId)) {
          auto ints = std::unordered_map<std::string, std::int64_t>{};
          for (const auto& name : collectIntLocals(env)) {
            if (Value* v = env.get(name)) {
              ints[name] = v->i;
            }
          }
          jit_.run(s.loopId, ints, output_);
          syncEnvFromInts(env, ints);
          break;
        }
        execute(*s.body, env);
        if (jit_.enabled() && profiler_.isHot(s.loopId) && !jit_.has(s.loopId)) {
          jit_.maybeCompile(s, collectIntLocals(env));
        }
      }
      break;
    }
    case StmtKind::Block: {
      const auto& s = static_cast<const BlockStmt&>(stmt);
      Environment inner;
      inner.parent = &env;
      for (const auto& innerStmt : s.statements) {
        execute(*innerStmt, inner);
      }
      break;
    }
    case StmtKind::Acquire: {
      const auto& s = static_cast<const AcquireStmt&>(stmt);
      ResourceHandle handle;
      handle.kind = s.resourceKind;
      handle.id = nextResourceId_++;
      handle.held = true;
      if (s.argument) {
        handle.label = evaluate(*s.argument, env).toString();
      }
      output_ << "[acquire " << resourceKindName(handle.kind);
      if (!handle.label.empty()) {
        output_ << " " << handle.label;
      }
      output_ << " -> " << s.name << "]\n";
      env.define(s.name, Value::makeResource(std::move(handle)));
      break;
    }
    case StmtKind::Release: {
      const auto& s = static_cast<const ReleaseStmt&>(stmt);
      Value* value = env.get(s.name);
      if (!value || value->kind != ValueKind::Resource || !value->resource.held) {
        throw rt(s.line, "invalid release of '" + s.name + "'");
      }
      value->resource.held = false;
      output_ << "[release " << s.name << "]\n";
      break;
    }
    case StmtKind::Function:
      break;
  }
}

Value Interpreter::evaluate(const Expr& expr, Environment& env) {
  switch (expr.kind) {
    case ExprKind::Literal: {
      const auto& lit = static_cast<const LiteralExpr&>(expr);
      if (lit.literalKind == LiteralKind::Integer) {
        return Value::makeInt(lit.intValue);
      }
      if (lit.literalKind == LiteralKind::Boolean) {
        return Value::makeBool(lit.boolValue);
      }
      return Value::makeString(lit.stringValue);
    }
    case ExprKind::Variable: {
      const auto& var = static_cast<const VariableExpr&>(expr);
      Value* value = env.get(var.name);
      if (!value) {
        throw rt(var.line, "undeclared variable '" + var.name + "'");
      }
      return *value;
    }
    case ExprKind::Unary: {
      const auto& un = static_cast<const UnaryExpr&>(expr);
      Value operand = evaluate(*un.operand, env);
      return Value::makeInt(-operand.i);
    }
    case ExprKind::Binary: {
      const auto& bin = static_cast<const BinaryExpr&>(expr);
      Value left = evaluate(*bin.left, env);
      Value right = evaluate(*bin.right, env);
      try {
        return binary(bin, left, right);
      } catch (const RuntimeError& error) {
        if (error.line == 0) {
          throw rt(bin.line, error.what());
        }
        throw;
      }
    }
    case ExprKind::Call: {
      const auto& call = static_cast<const CallExpr&>(expr);
      auto it = functions_.find(call.callee);
      if (it == functions_.end()) {
        throw rt(call.line, "unknown function '" + call.callee + "'");
      }
      std::vector<Value> args;
      args.reserve(call.args.size());
      for (const auto& arg : call.args) {
        args.push_back(evaluate(*arg, env));
      }
      return executeFunction(*it->second, args);
    }
  }
  return Value::makeNull();
}

std::vector<std::string> Interpreter::collectIntLocals(Environment& env) const {
  std::unordered_map<std::string, bool> seen;
  auto collect = [&](auto&& self, Environment* current) -> void {
    if (!current) {
      return;
    }
    self(self, current->parent);
    for (auto& [name, value] : current->values) {
      seen[name] = value.kind == ValueKind::Int || value.kind == ValueKind::Bool;
    }
  };
  collect(collect, &env);
  std::vector<std::string> names;
  for (const auto& [name, isInt] : seen) {
    if (isInt) {
      names.push_back(name);
    }
  }
  return names;
}

void Interpreter::syncEnvFromInts(
    Environment& env, const std::unordered_map<std::string, std::int64_t>& ints) const {
  for (const auto& [name, value] : ints) {
    if (Value* slot = env.get(name)) {
      if (slot->kind == ValueKind::Bool) {
        *slot = Value::makeBool(value != 0);
      } else {
        *slot = Value::makeInt(value);
      }
    }
  }
}

}  // namespace rajit
