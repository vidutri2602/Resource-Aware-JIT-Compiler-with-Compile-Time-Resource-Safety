#include "ResourceChecker.h"

#include <algorithm>
#include <vector>

namespace rajit {

namespace {

CheckError err(int line, const std::string& message) {
  return CheckError(line, "resource error at line " + std::to_string(line) + ": " + message);
}

}  // namespace

void ResourceChecker::check(const Program& program) {
  symbols_.pushScope();
  functions_.clear();
  for (const auto& fn : program.functions) {
    Symbol sym;
    sym.name = fn->name;
    sym.type = TypeTag::Function;
    sym.line = fn->line;
    if (!symbols_.declare(sym)) {
      throw err(fn->line, "duplicate function '" + fn->name + "'");
    }
    functions_[fn->name] = fn.get();
  }
  if (!functions_.count("main")) {
    throw err(1, "program must define fn main()");
  }
  for (const auto& fn : program.functions) {
    checkFunction(*fn);
  }
}

void ResourceChecker::checkFunction(const FunctionStmt& fn) {
  symbols_.pushScope();
  ResourceEnv env;
  for (const auto& param : fn.params) {
    Symbol sym;
    sym.name = param;
    sym.type = TypeTag::Int;
    sym.line = fn.line;
    if (!symbols_.declare(sym)) {
      throw err(fn.line, "duplicate parameter '" + param + "'");
    }
  }
  checkStmt(*fn.body, env);
  for (const auto& [name, state] : env) {
    if (state == ResourceState::Held) {
      throw err(fn.line, "resource '" + name + "' is still held at end of '" + fn.name + "'");
    }
  }
  symbols_.popScope();
}

void ResourceChecker::checkStmt(const Stmt& stmt, ResourceEnv& env) {
  switch (stmt.kind) {
    case StmtKind::Let: {
      const auto& s = static_cast<const LetStmt&>(stmt);
      TypeTag type = checkExpr(*s.initializer);
      Symbol sym;
      sym.name = s.name;
      sym.type = type;
      sym.line = s.line;
      if (!symbols_.declare(sym)) {
        throw err(s.line, "redeclaration of '" + s.name + "'");
      }
      break;
    }
    case StmtKind::Assign: {
      const auto& s = static_cast<const AssignStmt&>(stmt);
      Symbol* existing = symbols_.resolve(s.name);
      if (!existing) {
        throw err(s.line, "undeclared variable '" + s.name + "'");
      }
      if (existing->isResource) {
        throw err(s.line, "cannot assign to resource '" + s.name + "'");
      }
      TypeTag type = checkExpr(*s.value);
      if (existing->type != TypeTag::Unknown && type != TypeTag::Unknown &&
          existing->type != type &&
          !(existing->type == TypeTag::Int && type == TypeTag::Bool) &&
          !(existing->type == TypeTag::Bool && type == TypeTag::Int)) {
        throw err(s.line, "type mismatch in assignment to '" + s.name + "'");
      }
      break;
    }
    case StmtKind::Expr: {
      const auto& s = static_cast<const ExprStmt&>(stmt);
      checkExpr(*s.expr);
      break;
    }
    case StmtKind::Print: {
      const auto& s = static_cast<const PrintStmt&>(stmt);
      checkExpr(*s.expr);
      break;
    }
    case StmtKind::Return: {
      const auto& s = static_cast<const ReturnStmt&>(stmt);
      if (s.value) {
        checkExpr(*s.value);
      }
      for (const auto& [name, state] : env) {
        if (state == ResourceState::Held) {
          throw err(s.line, "cannot return while resource '" + name + "' is held");
        }
      }
      break;
    }
    case StmtKind::If: {
      const auto& s = static_cast<const IfStmt&>(stmt);
      checkExpr(*s.condition);
      ResourceEnv thenEnv = snapshot(env);
      symbols_.pushScope();
      checkStmt(*s.thenBranch, thenEnv);
      symbols_.popScope();
      ResourceEnv elseEnv = snapshot(env);
      if (s.elseBranch) {
        symbols_.pushScope();
        checkStmt(*s.elseBranch, elseEnv);
        symbols_.popScope();
      }
      env = merge(thenEnv, elseEnv, s.line);
      break;
    }
    case StmtKind::While: {
      const auto& s = static_cast<const WhileStmt&>(stmt);
      checkExpr(*s.condition);
      ResourceEnv before = snapshot(env);
      ResourceEnv bodyEnv = snapshot(env);
      symbols_.pushScope();
      checkStmt(*s.body, bodyEnv);
      symbols_.popScope();

      auto heldNames = [](const ResourceEnv& e) {
        std::vector<std::string> names;
        for (const auto& [name, state] : e) {
          if (state == ResourceState::Held) {
            names.push_back(name);
          }
        }
        std::sort(names.begin(), names.end());
        return names;
      };
      if (heldNames(before) != heldNames(bodyEnv)) {
        throw err(s.line, "loop must not change net resource holdings");
      }
      break;
    }
    case StmtKind::Block: {
      const auto& s = static_cast<const BlockStmt&>(stmt);
      symbols_.pushScope();
      for (const auto& inner : s.statements) {
        checkStmt(*inner, env);
      }
      symbols_.popScope();
      break;
    }
    case StmtKind::Acquire: {
      const auto& s = static_cast<const AcquireStmt&>(stmt);
      if (s.argument) {
        checkExpr(*s.argument);
      }
      if (s.resourceKind == ResourceKind::File && !s.argument) {
        throw err(s.line, "acquire file requires a path");
      }
      if (s.resourceKind == ResourceKind::Mem && !s.argument) {
        throw err(s.line, "acquire mem requires a size");
      }
      Symbol* existing = symbols_.resolve(s.name);
      if (existing && existing->isResource) {
        auto it = env.find(s.name);
        if (it != env.end() && it->second == ResourceState::Held) {
          throw err(s.line, "acquire would leak already-held resource '" + s.name + "'");
        }
      }
      Symbol sym;
      sym.name = s.name;
      sym.type = TypeTag::Resource;
      sym.isResource = true;
      sym.resourceKind = s.resourceKind;
      sym.line = s.line;
      if (!existing) {
        if (!symbols_.declare(sym)) {
          throw err(s.line, "redeclaration of '" + s.name + "'");
        }
      } else if (!existing->isResource) {
        throw err(s.line, "'" + s.name + "' is not a resource");
      }
      env[s.name] = ResourceState::Held;
      break;
    }
    case StmtKind::Release: {
      const auto& s = static_cast<const ReleaseStmt&>(stmt);
      Symbol* existing = symbols_.resolve(s.name);
      if (!existing || !existing->isResource) {
        throw err(s.line, "release of unknown resource '" + s.name + "'");
      }
      auto it = env.find(s.name);
      if (it == env.end() || it->second != ResourceState::Held) {
        throw err(s.line, "double release or release of unheld resource '" + s.name + "'");
      }
      it->second = ResourceState::Unheld;
      break;
    }
    case StmtKind::Function:
      break;
  }
}

TypeTag ResourceChecker::checkExpr(const Expr& expr) {
  switch (expr.kind) {
    case ExprKind::Literal: {
      const auto& lit = static_cast<const LiteralExpr&>(expr);
      if (lit.literalKind == LiteralKind::Integer) {
        return TypeTag::Int;
      }
      if (lit.literalKind == LiteralKind::Boolean) {
        return TypeTag::Bool;
      }
      return TypeTag::String;
    }
    case ExprKind::Variable: {
      const auto& var = static_cast<const VariableExpr&>(expr);
      const Symbol* sym = symbols_.resolve(var.name);
      if (!sym) {
        throw err(var.line, "undeclared variable '" + var.name + "'");
      }
      if (sym->isResource) {
        throw err(var.line, "resource '" + var.name + "' cannot be used as a value");
      }
      return sym->type;
    }
    case ExprKind::Unary: {
      const auto& un = static_cast<const UnaryExpr&>(expr);
      return checkExpr(*un.operand);
    }
    case ExprKind::Binary: {
      const auto& bin = static_cast<const BinaryExpr&>(expr);
      checkExpr(*bin.left);
      checkExpr(*bin.right);
      if (bin.op == BinaryOp::Eq || bin.op == BinaryOp::Ne || bin.op == BinaryOp::Lt ||
          bin.op == BinaryOp::Le || bin.op == BinaryOp::Gt || bin.op == BinaryOp::Ge) {
        return TypeTag::Bool;
      }
      return TypeTag::Int;
    }
    case ExprKind::Call: {
      const auto& call = static_cast<const CallExpr&>(expr);
      auto it = functions_.find(call.callee);
      if (it == functions_.end()) {
        throw err(call.line, "unknown function '" + call.callee + "'");
      }
      if (it->second->params.size() != call.args.size()) {
        throw err(call.line, "argument count mismatch for '" + call.callee + "'");
      }
      for (const auto& arg : call.args) {
        if (checkExpr(*arg) == TypeTag::Resource) {
          throw err(call.line, "resources cannot be passed as arguments");
        }
      }
      return TypeTag::Int;
    }
  }
  return TypeTag::Unknown;
}

ResourceChecker::ResourceEnv ResourceChecker::merge(const ResourceEnv& a, const ResourceEnv& b,
                                                    int line) const {
  ResourceEnv out = a;
  for (const auto& [name, state] : b) {
    auto it = out.find(name);
    if (it == out.end()) {
      if (state == ResourceState::Held) {
        throw err(line, "resource '" + name + "' is held on only one branch");
      }
      out[name] = state;
    } else if (it->second != state) {
      throw err(line, "resource '" + name + "' has inconsistent state across branches");
    }
  }
  for (const auto& [name, state] : a) {
    if (!b.count(name) && state == ResourceState::Held) {
      throw err(line, "resource '" + name + "' is held on only one branch");
    }
  }
  return out;
}

ResourceChecker::ResourceEnv ResourceChecker::snapshot(const ResourceEnv& env) { return env; }

}  // namespace rajit
