#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rajit {

enum class ExprKind {
  Literal,
  Variable,
  Unary,
  Binary,
  Call
};

enum class StmtKind {
  Let,
  Assign,
  Expr,
  Print,
  Return,
  If,
  While,
  Block,
  Acquire,
  Release,
  Function
};

enum class LiteralKind { Integer, String, Boolean };

enum class ResourceKind { File, Mem, Lock };

enum class UnaryOp { Negate, Not };
enum class BinaryOp { Add, Sub, Mul, Div, Mod, Eq, Ne, Lt, Le, Gt, Ge };

struct Expr {
  ExprKind kind;
  int line = 1;
  virtual ~Expr() = default;

 protected:
  explicit Expr(ExprKind k) : kind(k) {}
};

struct LiteralExpr : Expr {
  LiteralKind literalKind = LiteralKind::Integer;
  std::int64_t intValue = 0;
  std::string stringValue;
  bool boolValue = false;

  LiteralExpr() : Expr(ExprKind::Literal) {}
};

struct VariableExpr : Expr {
  std::string name;
  VariableExpr() : Expr(ExprKind::Variable) {}
};

struct UnaryExpr : Expr {
  UnaryOp op = UnaryOp::Negate;
  std::unique_ptr<Expr> operand;
  UnaryExpr() : Expr(ExprKind::Unary) {}
};

struct BinaryExpr : Expr {
  BinaryOp op = BinaryOp::Add;
  std::unique_ptr<Expr> left;
  std::unique_ptr<Expr> right;
  BinaryExpr() : Expr(ExprKind::Binary) {}
};

struct CallExpr : Expr {
  std::string callee;
  std::vector<std::unique_ptr<Expr>> args;
  CallExpr() : Expr(ExprKind::Call) {}
};

struct Stmt {
  StmtKind kind;
  int line = 1;
  virtual ~Stmt() = default;

 protected:
  explicit Stmt(StmtKind k) : kind(k) {}
};

struct LetStmt : Stmt {
  std::string name;
  std::unique_ptr<Expr> initializer;
  LetStmt() : Stmt(StmtKind::Let) {}
};

struct AssignStmt : Stmt {
  std::string name;
  std::unique_ptr<Expr> value;
  AssignStmt() : Stmt(StmtKind::Assign) {}
};

struct ExprStmt : Stmt {
  std::unique_ptr<Expr> expr;
  ExprStmt() : Stmt(StmtKind::Expr) {}
};

struct PrintStmt : Stmt {
  std::unique_ptr<Expr> expr;
  PrintStmt() : Stmt(StmtKind::Print) {}
};

struct ReturnStmt : Stmt {
  std::unique_ptr<Expr> value;  // optional
  ReturnStmt() : Stmt(StmtKind::Return) {}
};

struct BlockStmt : Stmt {
  std::vector<std::unique_ptr<Stmt>> statements;
  BlockStmt() : Stmt(StmtKind::Block) {}
};

struct IfStmt : Stmt {
  std::unique_ptr<Expr> condition;
  std::unique_ptr<Stmt> thenBranch;
  std::unique_ptr<Stmt> elseBranch;  // optional
  IfStmt() : Stmt(StmtKind::If) {}
};

struct WhileStmt : Stmt {
  std::unique_ptr<Expr> condition;
  std::unique_ptr<Stmt> body;
  int loopId = -1;
  WhileStmt() : Stmt(StmtKind::While) {}
};

struct AcquireStmt : Stmt {
  std::string name;
  ResourceKind resourceKind = ResourceKind::File;
  std::unique_ptr<Expr> argument;  // file name or mem size; optional for lock
  AcquireStmt() : Stmt(StmtKind::Acquire) {}
};

struct ReleaseStmt : Stmt {
  std::string name;
  ReleaseStmt() : Stmt(StmtKind::Release) {}
};

struct FunctionStmt : Stmt {
  std::string name;
  std::vector<std::string> params;
  std::unique_ptr<BlockStmt> body;
  FunctionStmt() : Stmt(StmtKind::Function) {}
};

struct Program {
  std::vector<std::unique_ptr<FunctionStmt>> functions;
};

std::string dumpAst(const Program& program);
std::string resourceKindName(ResourceKind kind);
std::string binaryOpName(BinaryOp op);
std::string unaryOpName(UnaryOp op);

}  // namespace rajit
