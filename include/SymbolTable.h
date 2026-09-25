#pragma once

#include "AST.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace tinyrjit {

enum class SymbolKind { Variable, Function };

struct Symbol {
  std::string name;
  TypeKind type = TypeKind::Int;
  SymbolKind kind = SymbolKind::Variable;
  std::string scope;
  std::vector<TypeKind> paramTypes;
  int line = 0;
};

// Nested lexical scopes used by semantic analysis.
class SymbolTable {
 public:
  void enterScope(const std::string& name);
  void exitScope();
  const std::string& currentScope() const;

  void insert(const Symbol& symbol);
  const Symbol* lookup(const std::string& name) const;
  bool exists(const std::string& name) const;
  bool existsInCurrentScope(const std::string& name) const;

  std::vector<Symbol> allSymbols() const;

 private:
  struct Scope {
    std::string name;
    std::unordered_map<std::string, Symbol> symbols;
  };

  std::vector<Scope> scopes_;
};

}  // namespace tinyrjit
