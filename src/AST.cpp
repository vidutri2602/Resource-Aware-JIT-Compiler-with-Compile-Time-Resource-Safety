#include "AST.h"

#include <sstream>

namespace rajit {

std::string resourceKindName(ResourceKind kind) {
  switch (kind) {
    case ResourceKind::File:
      return "file";
    case ResourceKind::Mem:
      return "mem";
    case ResourceKind::Lock:
      return "lock";
  }
  return "resource";
}

std::string binaryOpName(BinaryOp op) {
  switch (op) {
    case BinaryOp::Add:
      return "+";
    case BinaryOp::Sub:
      return "-";
    case BinaryOp::Mul:
      return "*";
    case BinaryOp::Div:
      return "/";
    case BinaryOp::Mod:
      return "%";
    case BinaryOp::Eq:
      return "==";
    case BinaryOp::Ne:
      return "!=";
    case BinaryOp::Lt:
      return "<";
    case BinaryOp::Le:
      return "<=";
    case BinaryOp::Gt:
      return ">";
    case BinaryOp::Ge:
      return ">=";
  }
  return "?";
}

std::string unaryOpName(UnaryOp op) {
  return op == UnaryOp::Negate ? "-" : "!";
}

namespace {

void indent(std::ostream& out, int depth) { out << std::string(depth * 2, ' '); }

void dumpExpr(const Expr& expr, std::ostream& out, int depth);
void dumpStmt(const Stmt& stmt, std::ostream& out, int depth);

void dumpExpr(const Expr& expr, std::ostream& out, int depth) {
  indent(out, depth);
  switch (expr.kind) {
    case ExprKind::Literal: {
      const auto& lit = static_cast<const LiteralExpr&>(expr);
      out << "Literal ";
      if (lit.literalKind == LiteralKind::Integer) {
        out << lit.intValue;
      } else if (lit.literalKind == LiteralKind::Boolean) {
        out << (lit.boolValue ? "true" : "false");
      } else {
        out << '"' << lit.stringValue << '"';
      }
      out << "\n";
      break;
    }
    case ExprKind::Variable: {
      const auto& var = static_cast<const VariableExpr&>(expr);
      out << "Var " << var.name << "\n";
      break;
    }
    case ExprKind::Unary: {
      const auto& un = static_cast<const UnaryExpr&>(expr);
      out << "Unary " << unaryOpName(un.op) << "\n";
      dumpExpr(*un.operand, out, depth + 1);
      break;
    }
    case ExprKind::Binary: {
      const auto& bin = static_cast<const BinaryExpr&>(expr);
      out << "Binary " << binaryOpName(bin.op) << "\n";
      dumpExpr(*bin.left, out, depth + 1);
      dumpExpr(*bin.right, out, depth + 1);
      break;
    }
    case ExprKind::Call: {
      const auto& call = static_cast<const CallExpr&>(expr);
      out << "Call " << call.callee << "\n";
      for (const auto& arg : call.args) {
        dumpExpr(*arg, out, depth + 1);
      }
      break;
    }
  }
}

void dumpStmt(const Stmt& stmt, std::ostream& out, int depth) {
  indent(out, depth);
  switch (stmt.kind) {
    case StmtKind::Let: {
      const auto& s = static_cast<const LetStmt&>(stmt);
      out << "Let " << s.name << "\n";
      dumpExpr(*s.initializer, out, depth + 1);
      break;
    }
    case StmtKind::Assign: {
      const auto& s = static_cast<const AssignStmt&>(stmt);
      out << "Assign " << s.name << "\n";
      dumpExpr(*s.value, out, depth + 1);
      break;
    }
    case StmtKind::Expr: {
      const auto& s = static_cast<const ExprStmt&>(stmt);
      out << "ExprStmt\n";
      dumpExpr(*s.expr, out, depth + 1);
      break;
    }
    case StmtKind::Print: {
      const auto& s = static_cast<const PrintStmt&>(stmt);
      out << "Print\n";
      dumpExpr(*s.expr, out, depth + 1);
      break;
    }
    case StmtKind::Return: {
      const auto& s = static_cast<const ReturnStmt&>(stmt);
      out << "Return\n";
      if (s.value) {
        dumpExpr(*s.value, out, depth + 1);
      }
      break;
    }
    case StmtKind::If: {
      const auto& s = static_cast<const IfStmt&>(stmt);
      out << "If\n";
      dumpExpr(*s.condition, out, depth + 1);
      dumpStmt(*s.thenBranch, out, depth + 1);
      if (s.elseBranch) {
        dumpStmt(*s.elseBranch, out, depth + 1);
      }
      break;
    }
    case StmtKind::While: {
      const auto& s = static_cast<const WhileStmt&>(stmt);
      out << "While id=" << s.loopId << "\n";
      dumpExpr(*s.condition, out, depth + 1);
      dumpStmt(*s.body, out, depth + 1);
      break;
    }
    case StmtKind::Block: {
      const auto& s = static_cast<const BlockStmt&>(stmt);
      out << "Block\n";
      for (const auto& inner : s.statements) {
        dumpStmt(*inner, out, depth + 1);
      }
      break;
    }
    case StmtKind::Acquire: {
      const auto& s = static_cast<const AcquireStmt&>(stmt);
      out << "Acquire " << resourceKindName(s.resourceKind) << " " << s.name << "\n";
      if (s.argument) {
        dumpExpr(*s.argument, out, depth + 1);
      }
      break;
    }
    case StmtKind::Release: {
      const auto& s = static_cast<const ReleaseStmt&>(stmt);
      out << "Release " << s.name << "\n";
      break;
    }
    case StmtKind::Function: {
      const auto& s = static_cast<const FunctionStmt&>(stmt);
      out << "Fn " << s.name << "(";
      for (std::size_t i = 0; i < s.params.size(); ++i) {
        if (i) {
          out << ", ";
        }
        out << s.params[i];
      }
      out << ")\n";
      dumpStmt(*s.body, out, depth + 1);
      break;
    }
  }
}

}  // namespace

std::string dumpAst(const Program& program) {
  std::ostringstream out;
  out << "Program\n";
  for (const auto& fn : program.functions) {
    dumpStmt(*fn, out, 1);
  }
  return out.str();
}

}  // namespace rajit
