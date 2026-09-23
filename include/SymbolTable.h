#pragma once

#include "AST.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace rajit {

enum class TypeTag { Int, Bool, String, Resource, Function, Unknown };

struct Symbol {
  std::string name;
  TypeTag type = TypeTag::Unknown;
  ResourceKind resourceKind = ResourceKind::File;
  bool isResource = false;
  int line = 1;
};

class SymbolTable {
 public:
  void pushScope();
  void popScope();
  bool declare(const Symbol& symbol);
  Symbol* resolve(const std::string& name);
  const Symbol* resolve(const std::string& name) const;
  int depth() const { return static_cast<int>(scopes_.size()); }

 private:
  std::vector<std::unordered_map<std::string, Symbol>> scopes_;
};

}  // namespace rajit
