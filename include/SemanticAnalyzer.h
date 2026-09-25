#pragma once

#include "AST.h"
#include "SymbolTable.h"

namespace tinyrjit {

// Name resolution and type checking (semantic analysis).
class SemanticAnalyzer {
 public:
  void analyze(Program& program);
  const SymbolTable& symbols() const { return table_; }

 private:
  SymbolTable table_;
  TypeKind currentReturn_ = TypeKind::Int;
  std::string currentFunction_;

  void collectFunctions(const Program& program);
  void analyzeFunction(FunctionDecl& fn);
  void analyzeStmt(Stmt& stmt);
  TypeKind analyzeExpr(Expr& expr);
};

}  // namespace tinyrjit
