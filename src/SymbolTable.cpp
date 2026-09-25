#include "SymbolTable.h"

#include "Error.h"

namespace tinyrjit {

void SymbolTable::enterScope(const std::string& name) {
  Scope scope;
  scope.name = name;
  scopes_.push_back(std::move(scope));
}

void SymbolTable::exitScope() {
  if (scopes_.empty()) {
    throw SemanticError(0, "internal error: exiting empty scope stack");
  }
  scopes_.pop_back();
}

const std::string& SymbolTable::currentScope() const {
  if (scopes_.empty()) {
    throw SemanticError(0, "internal error: no active scope");
  }
  return scopes_.back().name;
}

void SymbolTable::insert(const Symbol& symbol) {
  if (scopes_.empty()) {
    throw SemanticError(symbol.line, "internal error: insert with no scope");
  }
  auto& map = scopes_.back().symbols;
  if (map.count(symbol.name) != 0) {
    throw SemanticError(symbol.line, "Duplicate declaration of '" + symbol.name + "'.");
  }
  map[symbol.name] = symbol;
}

const Symbol* SymbolTable::lookup(const std::string& name) const {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    auto found = it->symbols.find(name);
    if (found != it->symbols.end()) {
      return &found->second;
    }
  }
  return nullptr;
}

bool SymbolTable::exists(const std::string& name) const { return lookup(name) != nullptr; }

bool SymbolTable::existsInCurrentScope(const std::string& name) const {
  if (scopes_.empty()) {
    return false;
  }
  return scopes_.back().symbols.count(name) != 0;
}

std::vector<Symbol> SymbolTable::allSymbols() const {
  std::vector<Symbol> result;
  for (const auto& scope : scopes_) {
    for (const auto& kv : scope.symbols) {
      result.push_back(kv.second);
    }
  }
  return result;
}

}  // namespace tinyrjit
