#include "AST.h"

#include <sstream>

namespace tinyrjit {
namespace {

std::string opText(TokenType op) {
  switch (op) {
    case TokenType::Plus:
      return "+";
    case TokenType::Minus:
      return "-";
    case TokenType::Star:
      return "*";
    case TokenType::Slash:
      return "/";
    case TokenType::EqualEqual:
      return "==";
    case TokenType::Less:
      return "<";
    case TokenType::Greater:
      return ">";
    case TokenType::LessEqual:
      return "<=";
    case TokenType::GreaterEqual:
      return ">=";
    default:
      return "?";
  }
}

}  // namespace

std::string dumpExpr(const Expr& expr, const std::string& indent) {
  std::ostringstream out;
  if (const auto* n = dynamic_cast<const NumberExpr*>(&expr)) {
    out << indent << n->value << "\n";
  } else if (const auto* s = dynamic_cast<const StringExpr*>(&expr)) {
    out << indent << "\"" << s->value << "\"\n";
  } else if (const auto* v = dynamic_cast<const VariableExpr*>(&expr)) {
    out << indent << v->name << "\n";
  } else if (const auto* b = dynamic_cast<const BinaryExpr*>(&expr)) {
    out << indent << "BinaryExpression: " << opText(b->op) << "\n";
    out << dumpExpr(*b->left, indent + "    ");
    out << dumpExpr(*b->right, indent + "    ");
  } else if (const auto* c = dynamic_cast<const CallExpr*>(&expr)) {
    out << indent << "CallExpression: " << c->callee << "\n";
    for (const auto& arg : c->args) {
      out << dumpExpr(*arg, indent + "    ");
    }
  }
  return out.str();
}

std::string dumpStmt(const Stmt& stmt, const std::string& indent) {
  std::ostringstream out;
  if (const auto* d = dynamic_cast<const VariableDeclStmt*>(&stmt)) {
    out << indent << "VariableDeclaration\n";
    out << indent << "├── Name: " << d->name << "\n";
    out << indent << "├── Type: " << typeName(d->type) << "\n";
    out << indent << "└── ";
    std::string initDump = dumpExpr(*d->init, indent + "    ");
    // Put the first line of the initializer on the same line as └──.
    if (!initDump.empty() && initDump.compare(0, (indent + "    ").size(), indent + "    ") == 0) {
      out << initDump.substr((indent + "    ").size());
    } else {
      out << "\n" << initDump;
    }
  } else if (const auto* a = dynamic_cast<const AssignmentStmt*>(&stmt)) {
    out << indent << "Assignment: " << a->name << "\n";
    out << dumpExpr(*a->value, indent + "    ");
  } else if (const auto* e = dynamic_cast<const ExpressionStmt*>(&stmt)) {
    out << indent << "ExpressionStatement\n";
    out << dumpExpr(*e->expr, indent + "    ");
  } else if (const auto* r = dynamic_cast<const ReturnStmt*>(&stmt)) {
    out << indent << "Return\n";
    out << dumpExpr(*r->value, indent + "    ");
  } else if (const auto* i = dynamic_cast<const IfStmt*>(&stmt)) {
    out << indent << "If\n";
    out << indent << "├── Condition\n";
    out << dumpExpr(*i->condition, indent + "│   ");
    out << indent << "├── Then\n";
    for (const auto& s : i->thenBranch) {
      out << dumpStmt(*s, indent + "│   ");
    }
    out << indent << "└── Else\n";
    for (const auto& s : i->elseBranch) {
      out << dumpStmt(*s, indent + "    ");
    }
  } else if (const auto* w = dynamic_cast<const WhileStmt*>(&stmt)) {
    out << indent << "While\n";
    out << indent << "├── Condition\n";
    out << dumpExpr(*w->condition, indent + "│   ");
    out << indent << "└── Body\n";
    for (const auto& s : w->body) {
      out << dumpStmt(*s, indent + "    ");
    }
  }
  return out.str();
}

std::string dumpAst(const Program& program) {
  std::ostringstream out;
  for (const auto& fn : program.functions) {
    out << "Function: " << fn->name << " -> " << typeName(fn->returnType) << "\n";
    for (const auto& p : fn->params) {
      out << "  Param: " << p.name << ": " << typeName(p.type) << "\n";
    }
    out << "  Body:\n";
    for (const auto& stmt : fn->body) {
      out << dumpStmt(*stmt, "    ");
    }
    out << "\n";
  }
  return out.str();
}

}  // namespace tinyrjit
