#pragma once

#include "AST.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace tinyrjit {

enum class ResourceState { UNOPENED, OPEN, CLOSED };

inline const char* resourceStateName(ResourceState state) {
  switch (state) {
    case ResourceState::UNOPENED:
      return "UNOPENED";
    case ResourceState::OPEN:
      return "OPEN";
    case ResourceState::CLOSED:
      return "CLOSED";
  }
  return "?";
}

struct ResourceTrace {
  std::string handle;
  ResourceState state;
};

// Compile-time Handle protocol: UNOPENED -> open -> OPEN -> close -> CLOSED.
class ResourceChecker {
 public:
  void check(const Program& program);

  const std::vector<ResourceTrace>& trace() const { return trace_; }
  bool functionUsesHandles(const std::string& name) const;

 private:
  std::vector<ResourceTrace> trace_;
  std::unordered_map<std::string, bool> usesHandles_;

  void checkFunction(const FunctionDecl& fn);
  void checkStmts(const std::vector<std::unique_ptr<Stmt>>& stmts,
                  std::unordered_map<std::string, ResourceState>& state,
                  bool& alwaysReturns);
  void applyCall(const CallExpr& call, std::unordered_map<std::string, ResourceState>& state);
  static std::string handleName(const Expr& expr);
};

}  // namespace tinyrjit
