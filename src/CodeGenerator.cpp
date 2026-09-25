#include "CodeGenerator.h"

#include "Error.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/raw_ostream.h"

#include <unordered_map>

namespace tinyrjit {
namespace {

using llvm::AllocaInst;
using llvm::BasicBlock;
using llvm::ConstantInt;
using llvm::Function;
using llvm::FunctionType;
using llvm::IRBuilder;
using llvm::LLVMContext;
using llvm::Module;
using llvm::Type;
using llvm::Value;

class Emitter {
 public:
  Emitter(LLVMContext& ctx, Module& module, Function& fn)
      : ctx_(ctx), module_(module), fn_(fn), builder_(&fn.getEntryBlock()) {
    (void)module_;
    i32_ = Type::getInt32Ty(ctx_);
    BasicBlock* entry = &fn.getEntryBlock();
    builder_.SetInsertPoint(entry);
    for (auto& arg : fn.args()) {
      AllocaInst* slot = entryAlloca(arg.getName().str());
      builder_.CreateStore(&arg, slot);
      locals_[arg.getName().str()] = slot;
    }
  }

  bool emitStmts(const std::vector<std::unique_ptr<Stmt>>& stmts) {
    for (const auto& stmt : stmts) {
      if (terminated()) {
        break;
      }
      emitStmt(*stmt);
    }
    return terminated();
  }

  void finish() {
    if (!terminated()) {
      builder_.CreateRet(ConstantInt::get(i32_, 0));
    }
  }

 private:
  LLVMContext& ctx_;
  Module& module_;
  Function& fn_;
  IRBuilder<> builder_;
  Type* i32_ = nullptr;
  std::unordered_map<std::string, AllocaInst*> locals_;

  bool terminated() const {
    return builder_.GetInsertBlock() != nullptr &&
           builder_.GetInsertBlock()->getTerminator() != nullptr;
  }

  AllocaInst* entryAlloca(const std::string& name) {
    IRBuilder<> tmp(&fn_.getEntryBlock(), fn_.getEntryBlock().begin());
    return tmp.CreateAlloca(i32_, nullptr, name);
  }

  Value* emitExpr(const Expr& expr) {
    if (const auto* n = dynamic_cast<const NumberExpr*>(&expr)) {
      return ConstantInt::get(i32_, n->value);
    }
    if (const auto* v = dynamic_cast<const VariableExpr*>(&expr)) {
      auto it = locals_.find(v->name);
      if (it == locals_.end()) {
        throw JITError(v->line, "Unknown variable '" + v->name + "' during IR gen.");
      }
      return builder_.CreateLoad(i32_, it->second, v->name);
    }
    if (const auto* b = dynamic_cast<const BinaryExpr*>(&expr)) {
      Value* l = emitExpr(*b->left);
      Value* r = emitExpr(*b->right);
      switch (b->op) {
        case TokenType::Plus:
          return builder_.CreateAdd(l, r, "addtmp");
        case TokenType::Minus:
          return builder_.CreateSub(l, r, "subtmp");
        case TokenType::Star:
          return builder_.CreateMul(l, r, "multmp");
        case TokenType::Slash:
          return builder_.CreateSDiv(l, r, "divtmp");
        case TokenType::EqualEqual:
          return builder_.CreateZExt(builder_.CreateICmpEQ(l, r, "eqtmp"), i32_);
        case TokenType::Less:
          return builder_.CreateZExt(builder_.CreateICmpSLT(l, r, "lttmp"), i32_);
        case TokenType::Greater:
          return builder_.CreateZExt(builder_.CreateICmpSGT(l, r, "gttmp"), i32_);
        case TokenType::LessEqual:
          return builder_.CreateZExt(builder_.CreateICmpSLE(l, r, "letmp"), i32_);
        case TokenType::GreaterEqual:
          return builder_.CreateZExt(builder_.CreateICmpSGE(l, r, "getmp"), i32_);
        default:
          throw JITError(b->line, "Unsupported operator in IR gen.");
      }
    }
    if (const auto* c = dynamic_cast<const CallExpr*>(&expr)) {
      Function* callee = module_.getFunction(c->callee);
      if (callee == nullptr) {
        throw JITError(c->line, "Cannot emit call to '" + c->callee + "'.");
      }
      std::vector<Value*> args;
      for (const auto& arg : c->args) {
        args.push_back(emitExpr(*arg));
      }
      return builder_.CreateCall(callee, args, "calltmp");
    }
    throw JITError(expr.line, "Unsupported expression in IR gen.");
  }

  void emitStmt(const Stmt& stmt) {
    if (const auto* d = dynamic_cast<const VariableDeclStmt*>(&stmt)) {
      AllocaInst* slot = entryAlloca(d->name);
      locals_[d->name] = slot;
      builder_.CreateStore(emitExpr(*d->init), slot);
      return;
    }
    if (const auto* a = dynamic_cast<const AssignmentStmt*>(&stmt)) {
      auto it = locals_.find(a->name);
      if (it == locals_.end()) {
        throw JITError(a->line, "Unknown variable '" + a->name + "' during IR gen.");
      }
      builder_.CreateStore(emitExpr(*a->value), it->second);
      return;
    }
    if (const auto* e = dynamic_cast<const ExpressionStmt*>(&stmt)) {
      emitExpr(*e->expr);
      return;
    }
    if (const auto* r = dynamic_cast<const ReturnStmt*>(&stmt)) {
      builder_.CreateRet(emitExpr(*r->value));
      return;
    }
    if (const auto* i = dynamic_cast<const IfStmt*>(&stmt)) {
      Value* condVal = emitExpr(*i->condition);
      Value* cond = builder_.CreateICmpNE(condVal, ConstantInt::get(i32_, 0), "ifcond");
      BasicBlock* thenBB = BasicBlock::Create(ctx_, "then", &fn_);
      BasicBlock* elseBB = BasicBlock::Create(ctx_, "else", &fn_);
      BasicBlock* mergeBB = BasicBlock::Create(ctx_, "ifcont", &fn_);
      builder_.CreateCondBr(cond, thenBB, elseBB);

      builder_.SetInsertPoint(thenBB);
      bool thenTerm = emitStmts(i->thenBranch);
      if (!thenTerm) {
        builder_.CreateBr(mergeBB);
      }

      builder_.SetInsertPoint(elseBB);
      bool elseTerm = emitStmts(i->elseBranch);
      if (!elseTerm) {
        builder_.CreateBr(mergeBB);
      }

      if (thenTerm && elseTerm) {
        mergeBB->eraseFromParent();
        return;
      }
      builder_.SetInsertPoint(mergeBB);
      return;
    }
    if (const auto* w = dynamic_cast<const WhileStmt*>(&stmt)) {
      BasicBlock* condBB = BasicBlock::Create(ctx_, "loopcond", &fn_);
      BasicBlock* bodyBB = BasicBlock::Create(ctx_, "loopbody", &fn_);
      BasicBlock* afterBB = BasicBlock::Create(ctx_, "loopend", &fn_);
      builder_.CreateBr(condBB);

      builder_.SetInsertPoint(condBB);
      Value* condVal = emitExpr(*w->condition);
      Value* cond = builder_.CreateICmpNE(condVal, ConstantInt::get(i32_, 0), "loopc");
      builder_.CreateCondBr(cond, bodyBB, afterBB);

      builder_.SetInsertPoint(bodyBB);
      bool bodyTerm = emitStmts(w->body);
      if (!bodyTerm) {
        builder_.CreateBr(condBB);
      }

      builder_.SetInsertPoint(afterBB);
      return;
    }
  }
};

}  // namespace

struct CodeGenerator::Impl {
  std::unique_ptr<LLVMContext> context = std::make_unique<LLVMContext>();
  std::unique_ptr<Module> module;
};

CodeGenerator::CodeGenerator() : impl_(std::make_unique<Impl>()) {}

CodeGenerator::~CodeGenerator() = default;

std::string CodeGenerator::generate(const Program& program,
                                    const std::unordered_set<std::string>& eligible) {
  impl_->context = std::make_unique<LLVMContext>();
  impl_->module = std::make_unique<Module>("tinyrjit", *impl_->context);
  LLVMContext& ctx = *impl_->context;
  Type* i32 = Type::getInt32Ty(ctx);

  for (const auto& fn : program.functions) {
    if (!eligible.count(fn->name)) {
      continue;
    }
    std::vector<Type*> params(fn->params.size(), i32);
    FunctionType* ft = FunctionType::get(i32, params, false);
    Function* llvmFn = Function::Create(ft, Function::ExternalLinkage, fn->name, impl_->module.get());
    std::size_t idx = 0;
    for (auto& arg : llvmFn->args()) {
      arg.setName(fn->params[idx++].name);
    }
  }

  for (const auto& fn : program.functions) {
    if (!eligible.count(fn->name)) {
      continue;
    }
    Function* llvmFn = impl_->module->getFunction(fn->name);
    BasicBlock::Create(ctx, "entry", llvmFn);
    Emitter emitter(ctx, *impl_->module, *llvmFn);
    emitter.emitStmts(fn->body);
    emitter.finish();
    std::string err;
    llvm::raw_string_ostream es(err);
    if (llvm::verifyFunction(*llvmFn, &es)) {
      throw JITError(fn->line, "LLVM verifier failed for '" + fn->name + "': " + es.str());
    }
  }

  std::string ir;
  llvm::raw_string_ostream os(ir);
  impl_->module->print(os, nullptr);
  os.flush();
  return ir;
}

void CodeGenerator::install(llvm::orc::LLJIT& jit) {
  if (!impl_->module || !impl_->context) {
    throw JITError(0, "No LLVM module to install.");
  }
  llvm::Error err = jit.addIRModule(
      llvm::orc::ThreadSafeModule(std::move(impl_->module), std::move(impl_->context)));
  if (err) {
    std::string msg;
    llvm::handleAllErrors(std::move(err), [&](const llvm::ErrorInfoBase& info) {
      msg = info.message();
    });
    throw JITError(0, "Failed to add module to ORC JIT: " + msg);
  }
}

}  // namespace tinyrjit
