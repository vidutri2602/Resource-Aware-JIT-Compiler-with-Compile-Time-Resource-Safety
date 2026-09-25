#include "SemanticAnalyzer.h"

#include "Error.h"

#include <string>

namespace tinyrjit {
namespace {

bool isBuiltin(const std::string& name) {
  return name == "open" || name == "use" || name == "close";
}

}  // namespace

void SemanticAnalyzer::analyze(Program& program) {
  table_ = SymbolTable{};
  table_.enterScope("global");
  collectFunctions(program);

  bool hasMain = false;
  for (auto& fn : program.functions) {
    if (fn->name == "main") {
      hasMain = true;
      if (!fn->params.empty()) {
        throw SemanticError(fn->line, "main must take no parameters.");
      }
      if (fn->returnType != TypeKind::Int) {
        throw SemanticError(fn->line, "main must return Int.");
      }
    }
    analyzeFunction(*fn);
  }
  if (!hasMain) {
    throw SemanticError(0, "Program must define fn main() -> Int.");
  }
  table_.exitScope();
}

void SemanticAnalyzer::collectFunctions(const Program& program) {
  for (const auto& fn : program.functions) {
    Symbol s;
    s.name = fn->name;
    s.type = fn->returnType;
    s.kind = SymbolKind::Function;
    s.scope = "global";
    s.line = fn->line;
    for (const auto& p : fn->params) {
      s.paramTypes.push_back(p.type);
    }
    table_.insert(s);
  }
}

void SemanticAnalyzer::analyzeFunction(FunctionDecl& fn) {
  currentReturn_ = fn.returnType;
  currentFunction_ = fn.name;
  table_.enterScope(fn.name);
  for (const auto& p : fn.params) {
    Symbol s;
    s.name = p.name;
    s.type = p.type;
    s.kind = SymbolKind::Variable;
    s.scope = fn.name;
    s.line = p.line;
    table_.insert(s);
  }
  for (auto& stmt : fn.body) {
    analyzeStmt(*stmt);
  }
  table_.exitScope();
}

void SemanticAnalyzer::analyzeStmt(Stmt& stmt) {
  if (auto* d = dynamic_cast<VariableDeclStmt*>(&stmt)) {
    TypeKind initType = analyzeExpr(*d->init);
    if (initType != d->type) {
      throw SemanticError(d->line, std::string("Cannot assign ") + typeName(initType) +
                                       " to " + typeName(d->type) + ".");
    }
    Symbol s;
    s.name = d->name;
    s.type = d->type;
    s.kind = SymbolKind::Variable;
    s.scope = currentFunction_;
    s.line = d->line;
    table_.insert(s);
    return;
  }
  if (auto* a = dynamic_cast<AssignmentStmt*>(&stmt)) {
    const Symbol* found = table_.lookup(a->name);
    if (!found || found->kind != SymbolKind::Variable) {
      throw SemanticError(a->line, "Variable '" + a->name + "' has not been declared.");
    }
    TypeKind valueType = analyzeExpr(*a->value);
    if (valueType != found->type) {
      throw SemanticError(a->line, std::string("Cannot assign ") + typeName(valueType) +
                                       " to " + typeName(found->type) + ".");
    }
    return;
  }
  if (auto* e = dynamic_cast<ExpressionStmt*>(&stmt)) {
    analyzeExpr(*e->expr);
    return;
  }
  if (auto* r = dynamic_cast<ReturnStmt*>(&stmt)) {
    TypeKind t = analyzeExpr(*r->value);
    if (t != currentReturn_) {
      throw SemanticError(r->line, std::string("Return type ") + typeName(t) +
                                       " is not compatible with " +
                                       typeName(currentReturn_) + ".");
    }
    return;
  }
  if (auto* i = dynamic_cast<IfStmt*>(&stmt)) {
    TypeKind cond = analyzeExpr(*i->condition);
    if (cond != TypeKind::Int) {
      throw SemanticError(i->line, "If condition must be Int (0 is false, non-zero is true).");
    }
    table_.enterScope(currentFunction_ + ".if");
    for (auto& s : i->thenBranch) {
      analyzeStmt(*s);
    }
    table_.exitScope();
    table_.enterScope(currentFunction_ + ".else");
    for (auto& s : i->elseBranch) {
      analyzeStmt(*s);
    }
    table_.exitScope();
    return;
  }
  if (auto* w = dynamic_cast<WhileStmt*>(&stmt)) {
    TypeKind cond = analyzeExpr(*w->condition);
    if (cond != TypeKind::Int) {
      throw SemanticError(w->line, "While condition must be Int.");
    }
    table_.enterScope(currentFunction_ + ".while");
    for (auto& s : w->body) {
      analyzeStmt(*s);
    }
    table_.exitScope();
    return;
  }
}

TypeKind SemanticAnalyzer::analyzeExpr(Expr& expr) {
  if (auto* n = dynamic_cast<NumberExpr*>(&expr)) {
    expr.inferredType = TypeKind::Int;
    (void)n;
    return TypeKind::Int;
  }
  if (auto* s = dynamic_cast<StringExpr*>(&expr)) {
    expr.inferredType = TypeKind::String;
    (void)s;
    return TypeKind::String;
  }
  if (auto* v = dynamic_cast<VariableExpr*>(&expr)) {
    const Symbol* found = table_.lookup(v->name);
    if (!found || found->kind != SymbolKind::Variable) {
      throw SemanticError(v->line, "Variable '" + v->name + "' has not been declared.");
    }
    expr.inferredType = found->type;
    return found->type;
  }
  if (auto* b = dynamic_cast<BinaryExpr*>(&expr)) {
    TypeKind l = analyzeExpr(*b->left);
    TypeKind r = analyzeExpr(*b->right);
    if (l != TypeKind::Int || r != TypeKind::Int) {
      throw SemanticError(b->line, "Arithmetic and comparison require Int operands.");
    }
    expr.inferredType = TypeKind::Int;
    return TypeKind::Int;
  }
  if (auto* c = dynamic_cast<CallExpr*>(&expr)) {
    if (isBuiltin(c->callee)) {
      if (c->callee == "open") {
        if (c->args.size() != 1) {
          throw SemanticError(c->line, "open() takes one string argument.");
        }
        TypeKind arg = analyzeExpr(*c->args[0]);
        if (arg != TypeKind::String) {
          throw SemanticError(c->line, "open() requires a string path.");
        }
        expr.inferredType = TypeKind::Handle;
        return TypeKind::Handle;
      }
      if (c->args.size() != 1) {
        throw SemanticError(c->line, c->callee + "() takes one Handle argument.");
      }
      TypeKind arg = analyzeExpr(*c->args[0]);
      if (arg != TypeKind::Handle) {
        throw SemanticError(c->line, c->callee + "() requires a Handle value.");
      }
      expr.inferredType = TypeKind::Int;
      return TypeKind::Int;
    }

    const Symbol* found = table_.lookup(c->callee);
    if (!found || found->kind != SymbolKind::Function) {
      throw SemanticError(c->line, "Unknown function '" + c->callee + "'.");
    }
    if (c->args.size() != found->paramTypes.size()) {
      throw SemanticError(c->line, "Function '" + c->callee + "' argument count mismatch.");
    }
    for (std::size_t i = 0; i < c->args.size(); ++i) {
      TypeKind arg = analyzeExpr(*c->args[i]);
      if (arg != found->paramTypes[i]) {
        throw SemanticError(c->line, "Argument type mismatch in call to '" + c->callee + "'.");
      }
    }
    expr.inferredType = found->type;
    return found->type;
  }
  throw SemanticError(expr.line, "internal error: unknown expression");
}

}  // namespace tinyrjit
