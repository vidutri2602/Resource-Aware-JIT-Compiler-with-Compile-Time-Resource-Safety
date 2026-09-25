#include "ResourceChecker.h"

#include "Error.h"

#include <set>

namespace tinyrjit {

bool ResourceChecker::functionUsesHandles(const std::string& name) const {
  auto it = usesHandles_.find(name);
  return it != usesHandles_.end() && it->second;
}

std::string ResourceChecker::handleName(const Expr& expr) {
  if (const auto* v = dynamic_cast<const VariableExpr*>(&expr)) {
    return v->name;
  }
  return "";
}

void ResourceChecker::check(const Program& program) {
  trace_.clear();
  usesHandles_.clear();
  for (const auto& fn : program.functions) {
    checkFunction(*fn);
  }
}

void ResourceChecker::applyCall(const CallExpr& call,
                                std::unordered_map<std::string, ResourceState>& state) {
  if (call.callee != "use" && call.callee != "close") {
    return;
  }
  if (call.args.size() != 1) {
    return;
  }
  std::string name = handleName(*call.args[0]);
  if (name.empty()) {
    throw ResourceError(call.line, call.callee + "() requires a Handle variable.");
  }
  auto it = state.find(name);
  if (it == state.end() || it->second == ResourceState::UNOPENED) {
    throw ResourceError(call.line, "Handle '" + name + "' has not been opened.");
  }
  if (call.callee == "use") {
    if (it->second == ResourceState::CLOSED) {
      throw ResourceError(call.line, "Cannot use closed Handle '" + name + "'.");
    }
    trace_.push_back({name, ResourceState::OPEN});
    return;
  }
  // close
  if (it->second == ResourceState::CLOSED) {
    throw ResourceError(call.line, "Handle '" + name + "' is already closed.");
  }
  it->second = ResourceState::CLOSED;
  trace_.push_back({name, ResourceState::CLOSED});
}

void ResourceChecker::checkStmts(const std::vector<std::unique_ptr<Stmt>>& stmts,
                                 std::unordered_map<std::string, ResourceState>& state,
                                 bool& alwaysReturns) {
  alwaysReturns = false;
  for (const auto& stmt : stmts) {
    if (alwaysReturns) {
      break;
    }
    if (auto* d = dynamic_cast<VariableDeclStmt*>(stmt.get())) {
      if (d->type == TypeKind::Handle) {
        auto* call = dynamic_cast<CallExpr*>(d->init.get());
        if (call && call->callee == "open") {
          state[d->name] = ResourceState::UNOPENED;
          trace_.push_back({d->name, ResourceState::UNOPENED});
          state[d->name] = ResourceState::OPEN;
          trace_.push_back({d->name, ResourceState::OPEN});
        } else {
          state[d->name] = ResourceState::UNOPENED;
          trace_.push_back({d->name, ResourceState::UNOPENED});
        }
      }
      continue;
    }
    if (auto* a = dynamic_cast<AssignmentStmt*>(stmt.get())) {
      auto* call = dynamic_cast<CallExpr*>(a->value.get());
      if (call && call->callee == "open") {
        state[a->name] = ResourceState::UNOPENED;
        trace_.push_back({a->name, ResourceState::UNOPENED});
        state[a->name] = ResourceState::OPEN;
        trace_.push_back({a->name, ResourceState::OPEN});
      } else if (auto* exprStmtCall = dynamic_cast<CallExpr*>(a->value.get())) {
        applyCall(*exprStmtCall, state);
      }
      continue;
    }
    if (auto* e = dynamic_cast<ExpressionStmt*>(stmt.get())) {
      if (auto* call = dynamic_cast<CallExpr*>(e->expr.get())) {
        applyCall(*call, state);
      }
      continue;
    }
    if (auto* r = dynamic_cast<ReturnStmt*>(stmt.get())) {
      for (const auto& kv : state) {
        if (kv.second == ResourceState::OPEN) {
          throw ResourceError(r->line, "Handle '" + kv.first + "' remains open at function exit.");
        }
      }
      alwaysReturns = true;
      continue;
    }
    if (auto* i = dynamic_cast<IfStmt*>(stmt.get())) {
      auto thenState = state;
      auto elseState = state;
      bool thenRet = false;
      bool elseRet = false;
      checkStmts(i->thenBranch, thenState, thenRet);
      checkStmts(i->elseBranch, elseState, elseRet);
      if (thenRet && elseRet) {
        alwaysReturns = true;
        state = thenState;
        continue;
      }
      if (thenRet) {
        state = elseState;
        continue;
      }
      if (elseRet) {
        state = thenState;
        continue;
      }
      std::set<std::string> names;
      for (const auto& kv : thenState) {
        names.insert(kv.first);
      }
      for (const auto& kv : elseState) {
        names.insert(kv.first);
      }
      for (const auto& name : names) {
        ResourceState ts = thenState.count(name) ? thenState[name] : ResourceState::UNOPENED;
        ResourceState es = elseState.count(name) ? elseState[name] : ResourceState::UNOPENED;
        if (ts != es) {
          throw ResourceError(i->line,
                              "Handle '" + name +
                                  "' has different states on if/else branches "
                                  "(conservative analysis).");
        }
        state[name] = ts;
      }
      continue;
    }
    if (auto* w = dynamic_cast<WhileStmt*>(stmt.get())) {
      auto bodyState = state;
      bool bodyRet = false;
      checkStmts(w->body, bodyState, bodyRet);
      for (const auto& kv : state) {
        ResourceState after = bodyState.count(kv.first) ? bodyState[kv.first] : kv.second;
        if (after != kv.second) {
          throw ResourceError(w->line,
                              "Loop may change Handle '" + kv.first +
                                  "' (conservative while-loop analysis).");
        }
      }
      for (const auto& kv : bodyState) {
        if (!state.count(kv.first)) {
          throw ResourceError(w->line,
                              "Loop may change Handle '" + kv.first +
                                  "' (conservative while-loop analysis).");
        }
      }
      continue;
    }
  }
}

void ResourceChecker::checkFunction(const FunctionDecl& fn) {
  std::unordered_map<std::string, ResourceState> state;
  bool uses = false;
  for (const auto& p : fn.params) {
    if (p.type == TypeKind::Handle) {
      uses = true;
      state[p.name] = ResourceState::OPEN;
      trace_.push_back({p.name, ResourceState::OPEN});
    }
  }

  auto markUses = [&](auto&& self, const std::vector<std::unique_ptr<Stmt>>& stmts) -> void {
    for (const auto& stmt : stmts) {
      if (auto* d = dynamic_cast<VariableDeclStmt*>(stmt.get())) {
        if (d->type == TypeKind::Handle) {
          uses = true;
        }
      }
      if (auto* e = dynamic_cast<ExpressionStmt*>(stmt.get())) {
        if (auto* call = dynamic_cast<CallExpr*>(e->expr.get())) {
          if (call->callee == "open" || call->callee == "use" || call->callee == "close") {
            uses = true;
          }
        }
      }
      if (auto* d = dynamic_cast<VariableDeclStmt*>(stmt.get())) {
        if (auto* call = dynamic_cast<CallExpr*>(d->init.get())) {
          if (call->callee == "open") {
            uses = true;
          }
        }
      }
      if (auto* i = dynamic_cast<IfStmt*>(stmt.get())) {
        self(self, i->thenBranch);
        self(self, i->elseBranch);
      }
      if (auto* w = dynamic_cast<WhileStmt*>(stmt.get())) {
        self(self, w->body);
      }
    }
  };
  markUses(markUses, fn.body);
  usesHandles_[fn.name] = uses;

  bool alwaysReturns = false;
  checkStmts(fn.body, state, alwaysReturns);
  if (!alwaysReturns) {
    for (const auto& kv : state) {
      if (kv.second == ResourceState::OPEN) {
        throw ResourceError(fn.line, "Handle '" + kv.first + "' remains open at function exit.");
      }
    }
  }
}

}  // namespace tinyrjit
