#pragma once

#include "Token.h"

#include <memory>
#include <string>
#include <vector>

namespace tinyrjit {

// TinyRAJIT has two user-visible types. String exists only as an
// intermediate type for the argument of builtin open().
enum class TypeKind { Int, Handle, String, Void };

inline const char* typeName(TypeKind type) {
  switch (type) {
    case TypeKind::Int:
      return "Int";
    case TypeKind::Handle:
      return "Handle";
    case TypeKind::String:
      return "String";
    case TypeKind::Void:
      return "Void";
  }
  return "Unknown";
}

// ---- expressions (syntax tree for values) ----

struct Expr {
  virtual ~Expr() = default;
  int line = 0;
  TypeKind inferredType = TypeKind::Int;
};

struct NumberExpr : Expr {
  int value = 0;
};

struct StringExpr : Expr {
  std::string value;
};

struct VariableExpr : Expr {
  std::string name;
};

struct BinaryExpr : Expr {
  TokenType op = TokenType::Plus;
  std::unique_ptr<Expr> left;
  std::unique_ptr<Expr> right;
};

struct CallExpr : Expr {
  std::string callee;
  std::vector<std::unique_ptr<Expr>> args;
};

// ---- statements ----

struct Stmt {
  virtual ~Stmt() = default;
  int line = 0;
};

struct VariableDeclStmt : Stmt {
  std::string name;
  TypeKind type = TypeKind::Int;
  std::unique_ptr<Expr> init;
};

struct AssignmentStmt : Stmt {
  std::string name;
  std::unique_ptr<Expr> value;
};

struct ExpressionStmt : Stmt {
  std::unique_ptr<Expr> expr;
};

struct ReturnStmt : Stmt {
  std::unique_ptr<Expr> value;
};

struct IfStmt : Stmt {
  std::unique_ptr<Expr> condition;
  std::vector<std::unique_ptr<Stmt>> thenBranch;
  std::vector<std::unique_ptr<Stmt>> elseBranch;
};

struct WhileStmt : Stmt {
  std::unique_ptr<Expr> condition;
  std::vector<std::unique_ptr<Stmt>> body;
};

struct Param {
  std::string name;
  TypeKind type = TypeKind::Int;
  int line = 0;
};

struct FunctionDecl {
  std::string name;
  std::vector<Param> params;
  TypeKind returnType = TypeKind::Int;
  std::vector<std::unique_ptr<Stmt>> body;
  int line = 0;
};

struct Program {
  std::vector<std::unique_ptr<FunctionDecl>> functions;
};

std::string dumpAst(const Program& program);
std::string dumpExpr(const Expr& expr, const std::string& indent);
std::string dumpStmt(const Stmt& stmt, const std::string& indent);

}  // namespace tinyrjit
