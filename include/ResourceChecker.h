#pragma once

#include "AST.h"
#include "SymbolTable.h"

#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace rajit {

class CheckError : public std::runtime_error {
 public:
  int line;
  CheckError(int line, const std::string& message)
      : std::runtime_error(message), line(line) {}
};

enum class ResourceState { Unheld, Held };

class ResourceChecker {
 public:
  void check(const Program& program);

 private:
  using ResourceEnv = std::unordered_map<std::string, ResourceState>;

  SymbolTable symbols_;
  std::unordered_map<std::string, FunctionStmt*> functions_;

  void checkFunction(const FunctionStmt& fn);
  void checkStmt(const Stmt& stmt, ResourceEnv& env);
  TypeTag checkExpr(const Expr& expr);
  ResourceEnv merge(const ResourceEnv& a, const ResourceEnv& b, int line) const;
  static ResourceEnv snapshot(const ResourceEnv& env);
};

}  // namespace rajit
