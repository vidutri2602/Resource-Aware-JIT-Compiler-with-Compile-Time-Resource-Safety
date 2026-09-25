#include "Interpreter.h"

#include "Error.h"
#include "JITCompiler.h"

namespace tinyrjit {

Interpreter::Interpreter(Profiler& profiler, JITCompiler* jit)
    : profiler_(profiler), jit_(jit) {}

std::int32_t Interpreter::interpret(const Program& program) {
  program_ = &program;
  functions_.clear();
  for (const auto& fn : program.functions) {
    functions_[fn->name] = fn.get();
  }
  Value result = call("main", {}, 1);
  return result.i;
}

Value Interpreter::call(const std::string& name, const std::vector<Value>& args, int line) {
  if (name == "open") {
    Value h;
    h.type = TypeKind::Handle;
    h.handleId = nextHandle_++;
    HandleObject obj;
    obj.path = args.empty() ? "" : args[0].text;
    obj.open = true;
    handles_[h.handleId] = obj;
    return h;
  }
  if (name == "use") {
    if (args.size() != 1 || args[0].type != TypeKind::Handle) {
      throw RuntimeError(line, "use() requires a Handle.");
    }
    auto it = handles_.find(args[0].handleId);
    if (it == handles_.end() || !it->second.open) {
      throw RuntimeError(line, "Cannot use Handle that is not open.");
    }
    it->second.used = true;
    Value v;
    v.i = 0;
    return v;
  }
  if (name == "close") {
    if (args.size() != 1 || args[0].type != TypeKind::Handle) {
      throw RuntimeError(line, "close() requires a Handle.");
    }
    auto it = handles_.find(args[0].handleId);
    if (it == handles_.end() || !it->second.open) {
      throw RuntimeError(line, "Cannot close Handle that is not open.");
    }
    it->second.open = false;
    Value v;
    v.i = 0;
    return v;
  }

  auto fnIt = functions_.find(name);
  if (fnIt == functions_.end()) {
    throw RuntimeError(line, "Unknown function '" + name + "'.");
  }
  const FunctionDecl* fn = fnIt->second;
  if (args.size() != fn->params.size()) {
    throw RuntimeError(line, "Wrong number of arguments to '" + name + "'.");
  }

  profiler_.recordFunctionCall(name);
  std::string previous = currentFunction_;
  currentFunction_ = name;

  if (jit_ && jit_->hasCompiled(name)) {
    std::vector<std::int32_t> ints;
    ints.reserve(args.size());
    for (const auto& a : args) {
      ints.push_back(a.i);
    }
    Value v;
    v.i = jit_->invoke(name, ints);
    currentFunction_ = previous;
    return v;
  }

  Env env;
  for (std::size_t i = 0; i < fn->params.size(); ++i) {
    env[fn->params[i].name] = args[i];
  }
  bool returned = false;
  Value ret;
  for (const auto& stmt : fn->body) {
    execStmt(*stmt, env, returned, ret);
    if (returned) {
      currentFunction_ = previous;
      return ret;
    }
  }
  currentFunction_ = previous;
  throw RuntimeError(fn->line, "Function '" + name + "' ended without return.");
}

Value Interpreter::eval(const Expr& expr, Env& env) {
  if (const auto* n = dynamic_cast<const NumberExpr*>(&expr)) {
    Value v;
    v.i = n->value;
    return v;
  }
  if (const auto* s = dynamic_cast<const StringExpr*>(&expr)) {
    Value v;
    v.type = TypeKind::String;
    v.text = s->value;
    return v;
  }
  if (const auto* var = dynamic_cast<const VariableExpr*>(&expr)) {
    auto it = env.find(var->name);
    if (it == env.end()) {
      throw RuntimeError(var->line, "Variable '" + var->name + "' has not been declared.");
    }
    return it->second;
  }
  if (const auto* b = dynamic_cast<const BinaryExpr*>(&expr)) {
    Value l = eval(*b->left, env);
    Value r = eval(*b->right, env);
    Value v;
    switch (b->op) {
      case TokenType::Plus:
        v.i = l.i + r.i;
        break;
      case TokenType::Minus:
        v.i = l.i - r.i;
        break;
      case TokenType::Star:
        v.i = l.i * r.i;
        break;
      case TokenType::Slash:
        if (r.i == 0) {
          throw RuntimeError(b->line, "Division by zero.");
        }
        v.i = l.i / r.i;
        break;
      case TokenType::EqualEqual:
        v.i = l.i == r.i;
        break;
      case TokenType::Less:
        v.i = l.i < r.i;
        break;
      case TokenType::Greater:
        v.i = l.i > r.i;
        break;
      case TokenType::LessEqual:
        v.i = l.i <= r.i;
        break;
      case TokenType::GreaterEqual:
        v.i = l.i >= r.i;
        break;
      default:
        throw RuntimeError(b->line, "Unknown operator.");
    }
    return v;
  }
  if (const auto* c = dynamic_cast<const CallExpr*>(&expr)) {
    std::vector<Value> args;
    for (const auto& arg : c->args) {
      args.push_back(eval(*arg, env));
    }
    return call(c->callee, args, c->line);
  }
  throw RuntimeError(expr.line, "internal error: unknown expression");
}

void Interpreter::execStmt(const Stmt& stmt, Env& env, bool& returned, Value& ret) {
  if (const auto* d = dynamic_cast<const VariableDeclStmt*>(&stmt)) {
    env[d->name] = eval(*d->init, env);
    env[d->name].type = d->type;
    return;
  }
  if (const auto* a = dynamic_cast<const AssignmentStmt*>(&stmt)) {
    auto it = env.find(a->name);
    if (it == env.end()) {
      throw RuntimeError(a->line, "Variable '" + a->name + "' has not been declared.");
    }
    TypeKind t = it->second.type;
    it->second = eval(*a->value, env);
    it->second.type = t;
    return;
  }
  if (const auto* e = dynamic_cast<const ExpressionStmt*>(&stmt)) {
    eval(*e->expr, env);
    return;
  }
  if (const auto* r = dynamic_cast<const ReturnStmt*>(&stmt)) {
    ret = eval(*r->value, env);
    returned = true;
    return;
  }
  if (const auto* i = dynamic_cast<const IfStmt*>(&stmt)) {
    Value cond = eval(*i->condition, env);
    const auto& branch = cond.i != 0 ? i->thenBranch : i->elseBranch;
    for (const auto& s : branch) {
      execStmt(*s, env, returned, ret);
      if (returned) {
        return;
      }
    }
    return;
  }
  if (const auto* w = dynamic_cast<const WhileStmt*>(&stmt)) {
    while (eval(*w->condition, env).i != 0) {
      if (!currentFunction_.empty()) {
        profiler_.recordFunctionCall(currentFunction_);
      }
      for (const auto& s : w->body) {
        execStmt(*s, env, returned, ret);
        if (returned) {
          return;
        }
      }
    }
    return;
  }
}

}  // namespace tinyrjit
