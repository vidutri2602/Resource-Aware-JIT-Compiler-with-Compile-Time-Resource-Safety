#include "JITPolicy.h"

#include <sstream>
#include <vector>

namespace tinyrjit {
namespace {

bool exprNumeric(const Expr& expr);
bool stmtsNumeric(const std::vector<std::unique_ptr<Stmt>>& stmts);

bool exprNumeric(const Expr& expr) {
  if (dynamic_cast<const NumberExpr*>(&expr) != nullptr) {
    return true;
  }
  if (dynamic_cast<const StringExpr*>(&expr) != nullptr) {
    return false;
  }
  if (dynamic_cast<const VariableExpr*>(&expr) != nullptr) {
    return true;
  }
  if (const auto* b = dynamic_cast<const BinaryExpr*>(&expr)) {
    return exprNumeric(*b->left) && exprNumeric(*b->right);
  }
  if (const auto* c = dynamic_cast<const CallExpr*>(&expr)) {
    if (c->callee == "open" || c->callee == "use" || c->callee == "close") {
      return false;
    }
    for (const auto& arg : c->args) {
      if (!exprNumeric(*arg)) {
        return false;
      }
    }
    return true;
  }
  return false;
}

bool stmtsNumeric(const std::vector<std::unique_ptr<Stmt>>& stmts) {
  for (const auto& stmt : stmts) {
    if (const auto* d = dynamic_cast<const VariableDeclStmt*>(stmt.get())) {
      if (d->type != TypeKind::Int || !exprNumeric(*d->init)) {
        return false;
      }
    } else if (const auto* a = dynamic_cast<const AssignmentStmt*>(stmt.get())) {
      if (!exprNumeric(*a->value)) {
        return false;
      }
    } else if (const auto* e = dynamic_cast<const ExpressionStmt*>(stmt.get())) {
      if (!exprNumeric(*e->expr)) {
        return false;
      }
    } else if (const auto* r = dynamic_cast<const ReturnStmt*>(stmt.get())) {
      if (!exprNumeric(*r->value)) {
        return false;
      }
    } else if (const auto* i = dynamic_cast<const IfStmt*>(stmt.get())) {
      if (!exprNumeric(*i->condition) || !stmtsNumeric(i->thenBranch) ||
          !stmtsNumeric(i->elseBranch)) {
        return false;
      }
    } else if (const auto* w = dynamic_cast<const WhileStmt*>(stmt.get())) {
      if (!exprNumeric(*w->condition) || !stmtsNumeric(w->body)) {
        return false;
      }
    } else {
      return false;
    }
  }
  return true;
}

void collectCallees(const Expr& expr, std::vector<std::string>& out) {
  if (const auto* b = dynamic_cast<const BinaryExpr*>(&expr)) {
    collectCallees(*b->left, out);
    collectCallees(*b->right, out);
    return;
  }
  if (const auto* c = dynamic_cast<const CallExpr*>(&expr)) {
    out.push_back(c->callee);
    for (const auto& arg : c->args) {
      collectCallees(*arg, out);
    }
  }
}

void collectCallees(const std::vector<std::unique_ptr<Stmt>>& stmts, std::vector<std::string>& out) {
  for (const auto& stmt : stmts) {
    if (const auto* d = dynamic_cast<const VariableDeclStmt*>(stmt.get())) {
      collectCallees(*d->init, out);
    } else if (const auto* a = dynamic_cast<const AssignmentStmt*>(stmt.get())) {
      collectCallees(*a->value, out);
    } else if (const auto* e = dynamic_cast<const ExpressionStmt*>(stmt.get())) {
      collectCallees(*e->expr, out);
    } else if (const auto* r = dynamic_cast<const ReturnStmt*>(stmt.get())) {
      collectCallees(*r->value, out);
    } else if (const auto* i = dynamic_cast<const IfStmt*>(stmt.get())) {
      collectCallees(*i->condition, out);
      collectCallees(i->thenBranch, out);
      collectCallees(i->elseBranch, out);
    } else if (const auto* w = dynamic_cast<const WhileStmt*>(stmt.get())) {
      collectCallees(*w->condition, out);
      collectCallees(w->body, out);
    }
  }
}

}  // namespace

bool functionIsNumeric(const FunctionDecl& fn) {
  if (fn.returnType != TypeKind::Int) {
    return false;
  }
  for (const auto& p : fn.params) {
    if (p.type != TypeKind::Int) {
      return false;
    }
  }
  return stmtsNumeric(fn.body);
}

JITPolicy::JITPolicy(const Profiler& profiler, const ResourceChecker& checker)
    : profiler_(profiler), checker_(checker) {}

void JITPolicy::markNumeric(const std::string& name, bool eligible) {
  numericEligible_[name] = eligible;
}

bool JITPolicy::isNumericEligible(const FunctionDecl& fn) const {
  auto it = numericEligible_.find(fn.name);
  if (it != numericEligible_.end()) {
    return it->second;
  }
  return functionIsNumeric(fn) && !checker_.functionUsesHandles(fn.name);
}

bool JITPolicy::resourceEligible(const std::string& name) const {
  auto it = numericEligible_.find(name);
  if (it != numericEligible_.end()) {
    return it->second;
  }
  return !checker_.functionUsesHandles(name);
}

std::unordered_set<std::string> JITPolicy::computeEligible(const Program& program,
                                                           const ResourceChecker& checker) {
  std::unordered_set<std::string> bad;
  for (const auto& fn : program.functions) {
    if (checker.functionUsesHandles(fn->name) || !functionIsNumeric(*fn)) {
      bad.insert(fn->name);
    }
  }
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto& fn : program.functions) {
      if (bad.count(fn->name) != 0) {
        continue;
      }
      std::vector<std::string> callees;
      collectCallees(fn->body, callees);
      for (const auto& callee : callees) {
        if (callee == "open" || callee == "use" || callee == "close" || bad.count(callee) != 0) {
          bad.insert(fn->name);
          changed = true;
          break;
        }
      }
    }
  }
  std::unordered_set<std::string> good;
  for (const auto& fn : program.functions) {
    if (bad.count(fn->name) == 0) {
      good.insert(fn->name);
    }
  }
  return good;
}

Eligibility JITPolicy::decide(const std::string& name, const FunctionDecl* fn) const {
  Eligibility e;
  e.function = name;
  e.hot = profiler_.isHot(name);
  e.resourceEligible = fn != nullptr ? isNumericEligible(*fn) : resourceEligible(name);
  e.jit = e.hot && e.resourceEligible;
  return e;
}

std::string JITPolicy::dump(const Program& program) const {
  std::ostringstream out;
  out << "[JIT Eligibility]\n";
  for (const auto& fn : program.functions) {
    Eligibility e = decide(fn->name, fn.get());
    out << "\nFunction: " << e.function << "\n";
    out << "Hot: " << (e.hot ? "YES" : "NO") << "\n";
    out << "Resource Eligible: " << (e.resourceEligible ? "YES" : "NO") << "\n";
    out << "Decision: " << (e.jit ? "JIT COMPILE" : "INTERPRETER") << "\n";
  }
  return out.str();
}

}  // namespace tinyrjit
