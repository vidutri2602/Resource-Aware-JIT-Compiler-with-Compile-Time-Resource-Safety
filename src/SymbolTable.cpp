#include "SymbolTable.h"

namespace rajit {

void SymbolTable::pushScope() { scopes_.emplace_back(); }

void SymbolTable::popScope() {
  if (!scopes_.empty()) {
    scopes_.pop_back();
  }
}

bool SymbolTable::declare(const Symbol& symbol) {
  if (scopes_.empty()) {
    pushScope();
  }
  auto& current = scopes_.back();
  if (current.count(symbol.name)) {
    return false;
  }
  current[symbol.name] = symbol;
  return true;
}

Symbol* SymbolTable::resolve(const std::string& name) {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      return &found->second;
    }
  }
  return nullptr;
}

const Symbol* SymbolTable::resolve(const std::string& name) const {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      return &found->second;
    }
  }
  return nullptr;
}

}  // namespace rajit
