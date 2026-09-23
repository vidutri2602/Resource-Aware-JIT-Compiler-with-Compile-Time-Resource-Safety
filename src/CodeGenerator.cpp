#include "CodeGenerator.h"

#include <cstring>
#include <sstream>

namespace rajit {

namespace {

void writeI64(std::vector<std::uint8_t>& code, std::int64_t value) {
  std::uint8_t bytes[8];
  std::memcpy(bytes, &value, sizeof(value));
  code.insert(code.end(), bytes, bytes + 8);
}

void writeI32(std::vector<std::uint8_t>& code, std::size_t offset, std::int32_t value) {
  std::uint8_t bytes[4];
  std::memcpy(bytes, &value, sizeof(value));
  for (int i = 0; i < 4; ++i) {
    code[offset + i] = bytes[i];
  }
}

std::int64_t readI64(const std::vector<std::uint8_t>& code, std::size_t offset) {
  std::int64_t value = 0;
  std::memcpy(&value, code.data() + offset, sizeof(value));
  return value;
}

std::int32_t readI32(const std::vector<std::uint8_t>& code, std::size_t offset) {
  std::int32_t value = 0;
  std::memcpy(&value, code.data() + offset, sizeof(value));
  return value;
}

std::uint16_t readU16(const std::vector<std::uint8_t>& code, std::size_t offset) {
  std::uint16_t value = 0;
  std::memcpy(&value, code.data() + offset, sizeof(value));
  return value;
}

}  // namespace

void CodeGenerator::emit(OpCode op) { chunk_->code.push_back(static_cast<std::uint8_t>(op)); }

void CodeGenerator::emitU16(std::uint16_t value) {
  std::uint8_t bytes[2];
  std::memcpy(bytes, &value, sizeof(value));
  chunk_->code.push_back(bytes[0]);
  chunk_->code.push_back(bytes[1]);
}

void CodeGenerator::emitI64(std::int64_t value) { writeI64(chunk_->code, value); }

void CodeGenerator::emitI32At(std::size_t offset, std::int32_t value) {
  writeI32(chunk_->code, offset, value);
}

std::size_t CodeGenerator::emitJmpPlaceholder(OpCode op) {
  emit(op);
  std::size_t offset = chunk_->code.size();
  chunk_->code.insert(chunk_->code.end(), 4, 0);
  return offset;
}

bool CodeGenerator::fail(const std::string& message) {
  ok_ = false;
  if (error_) {
    *error_ = message;
  }
  return false;
}

bool CodeGenerator::compileExpr(const Expr& expr) {
  if (!ok_) {
    return false;
  }
  switch (expr.kind) {
    case ExprKind::Literal: {
      const auto& lit = static_cast<const LiteralExpr&>(expr);
      if (lit.literalKind == LiteralKind::String) {
        return fail("JIT cannot compile string expressions");
      }
      emit(OpCode::Push);
      if (lit.literalKind == LiteralKind::Boolean) {
        emitI64(lit.boolValue ? 1 : 0);
      } else {
        emitI64(lit.intValue);
      }
      return true;
    }
    case ExprKind::Variable: {
      const auto& var = static_cast<const VariableExpr&>(expr);
      auto it = slots_.find(var.name);
      if (it == slots_.end()) {
        return fail("JIT local '" + var.name + "' is not an integer slot");
      }
      emit(OpCode::Load);
      emitU16(it->second);
      return true;
    }
    case ExprKind::Unary: {
      const auto& un = static_cast<const UnaryExpr&>(expr);
      if (!compileExpr(*un.operand)) {
        return false;
      }
      emit(OpCode::Neg);
      return true;
    }
    case ExprKind::Binary: {
      const auto& bin = static_cast<const BinaryExpr&>(expr);
      if (!compileExpr(*bin.left) || !compileExpr(*bin.right)) {
        return false;
      }
      switch (bin.op) {
        case BinaryOp::Add:
          emit(OpCode::Add);
          break;
        case BinaryOp::Sub:
          emit(OpCode::Sub);
          break;
        case BinaryOp::Mul:
          emit(OpCode::Mul);
          break;
        case BinaryOp::Div:
          emit(OpCode::Div);
          break;
        case BinaryOp::Mod:
          emit(OpCode::Mod);
          break;
        case BinaryOp::Eq:
          emit(OpCode::Eq);
          break;
        case BinaryOp::Ne:
          emit(OpCode::Ne);
          break;
        case BinaryOp::Lt:
          emit(OpCode::Lt);
          break;
        case BinaryOp::Le:
          emit(OpCode::Le);
          break;
        case BinaryOp::Gt:
          emit(OpCode::Gt);
          break;
        case BinaryOp::Ge:
          emit(OpCode::Ge);
          break;
      }
      return true;
    }
    case ExprKind::Call:
      return fail("JIT cannot compile function calls");
  }
  return fail("unsupported expression");
}

bool CodeGenerator::compileStmt(const Stmt& stmt) {
  if (!ok_) {
    return false;
  }
  switch (stmt.kind) {
    case StmtKind::Let: {
      const auto& s = static_cast<const LetStmt&>(stmt);
      if (slots_.find(s.name) == slots_.end()) {
        return fail("let '" + s.name + "' is not in JIT slot map");
      }
      if (!compileExpr(*s.initializer)) {
        return false;
      }
      emit(OpCode::Store);
      emitU16(slots_[s.name]);
      return true;
    }
    case StmtKind::Assign: {
      const auto& s = static_cast<const AssignStmt&>(stmt);
      auto it = slots_.find(s.name);
      if (it == slots_.end()) {
        return fail("assignment to non-integer '" + s.name + "'");
      }
      if (!compileExpr(*s.value)) {
        return false;
      }
      emit(OpCode::Store);
      emitU16(it->second);
      return true;
    }
    case StmtKind::Expr:
      return fail("expression statements are not JIT-compiled");
    case StmtKind::Print: {
      const auto& s = static_cast<const PrintStmt&>(stmt);
      if (!compileExpr(*s.expr)) {
        return false;
      }
      emit(OpCode::Print);
      return true;
    }
    case StmtKind::If: {
      const auto& s = static_cast<const IfStmt&>(stmt);
      if (!compileExpr(*s.condition)) {
        return false;
      }
      std::size_t elseJump = emitJmpPlaceholder(OpCode::JmpZ);
      if (!compileStmt(*s.thenBranch)) {
        return false;
      }
      if (s.elseBranch) {
        std::size_t endJump = emitJmpPlaceholder(OpCode::Jmp);
        emitI32At(elseJump, static_cast<std::int32_t>(chunk_->code.size()));
        if (!compileStmt(*s.elseBranch)) {
          return false;
        }
        emitI32At(endJump, static_cast<std::int32_t>(chunk_->code.size()));
      } else {
        emitI32At(elseJump, static_cast<std::int32_t>(chunk_->code.size()));
      }
      return true;
    }
    case StmtKind::While: {
      const auto& s = static_cast<const WhileStmt&>(stmt);
      std::size_t loopStart = chunk_->code.size();
      if (!compileExpr(*s.condition)) {
        return false;
      }
      std::size_t exitJump = emitJmpPlaceholder(OpCode::JmpZ);
      if (!compileStmt(*s.body)) {
        return false;
      }
      std::size_t back = emitJmpPlaceholder(OpCode::Jmp);
      emitI32At(back, static_cast<std::int32_t>(loopStart));
      emitI32At(exitJump, static_cast<std::int32_t>(chunk_->code.size()));
      return true;
    }
    case StmtKind::Block: {
      const auto& s = static_cast<const BlockStmt&>(stmt);
      for (const auto& inner : s.statements) {
        if (!compileStmt(*inner)) {
          return false;
        }
      }
      return true;
    }
    case StmtKind::Return:
    case StmtKind::Acquire:
    case StmtKind::Release:
    case StmtKind::Function:
      return fail("statement is not eligible for JIT compilation");
  }
  return fail("unsupported statement");
}

bool CodeGenerator::compileWhile(const WhileStmt& loop, const std::vector<std::string>& locals,
                                 Chunk& out, std::string& error) {
  out = Chunk{};
  chunk_ = &out;
  error_ = &error;
  ok_ = true;
  slots_.clear();
  for (std::uint16_t i = 0; i < locals.size(); ++i) {
    slots_[locals[i]] = i;
    out.slotNames.push_back(locals[i]);
  }

  std::size_t loopStart = chunk_->code.size();
  if (!compileExpr(*loop.condition)) {
    return false;
  }
  std::size_t exitJump = emitJmpPlaceholder(OpCode::JmpZ);
  if (!compileStmt(*loop.body)) {
    return false;
  }
  std::size_t back = emitJmpPlaceholder(OpCode::Jmp);
  emitI32At(back, static_cast<std::int32_t>(loopStart));
  emitI32At(exitJump, static_cast<std::int32_t>(chunk_->code.size()));
  emit(OpCode::Halt);
  return ok_;
}

std::string CodeGenerator::disassemble(const Chunk& chunk) const {
  std::ostringstream out;
  std::size_t ip = 0;
  while (ip < chunk.code.size()) {
    auto op = static_cast<OpCode>(chunk.code[ip]);
    out << ip << ": ";
    switch (op) {
      case OpCode::Push:
        out << "PUSH " << readI64(chunk.code, ip + 1) << "\n";
        ip += 9;
        break;
      case OpCode::Load:
        out << "LOAD " << readU16(chunk.code, ip + 1) << "\n";
        ip += 3;
        break;
      case OpCode::Store:
        out << "STORE " << readU16(chunk.code, ip + 1) << "\n";
        ip += 3;
        break;
      case OpCode::Add:
        out << "ADD\n";
        ip += 1;
        break;
      case OpCode::Sub:
        out << "SUB\n";
        ip += 1;
        break;
      case OpCode::Mul:
        out << "MUL\n";
        ip += 1;
        break;
      case OpCode::Div:
        out << "DIV\n";
        ip += 1;
        break;
      case OpCode::Mod:
        out << "MOD\n";
        ip += 1;
        break;
      case OpCode::Neg:
        out << "NEG\n";
        ip += 1;
        break;
      case OpCode::Eq:
        out << "EQ\n";
        ip += 1;
        break;
      case OpCode::Ne:
        out << "NE\n";
        ip += 1;
        break;
      case OpCode::Lt:
        out << "LT\n";
        ip += 1;
        break;
      case OpCode::Le:
        out << "LE\n";
        ip += 1;
        break;
      case OpCode::Gt:
        out << "GT\n";
        ip += 1;
        break;
      case OpCode::Ge:
        out << "GE\n";
        ip += 1;
        break;
      case OpCode::Jmp:
        out << "JMP " << readI32(chunk.code, ip + 1) << "\n";
        ip += 5;
        break;
      case OpCode::JmpZ:
        out << "JMPZ " << readI32(chunk.code, ip + 1) << "\n";
        ip += 5;
        break;
      case OpCode::Print:
        out << "PRINT\n";
        ip += 1;
        break;
      case OpCode::Halt:
        out << "HALT\n";
        ip += 1;
        break;
      default:
        out << "???\n";
        ip += 1;
        break;
    }
  }
  return out.str();
}

}  // namespace rajit
