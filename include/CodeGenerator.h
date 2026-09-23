#pragma once

#include "AST.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace rajit {

enum class OpCode : std::uint8_t {
  Push,
  Load,
  Store,
  Add,
  Sub,
  Mul,
  Div,
  Mod,
  Neg,
  Eq,
  Ne,
  Lt,
  Le,
  Gt,
  Ge,
  Jmp,
  JmpZ,
  Print,
  Halt
};

struct Chunk {
  std::vector<std::uint8_t> code;
  std::vector<std::string> slotNames;
};

class CodeGenerator {
 public:
  // Compiles a while-loop (condition + body) into bytecode that runs until
  // the condition is false. Integer locals are mapped to slots.
  bool compileWhile(const WhileStmt& loop, const std::vector<std::string>& locals,
                    Chunk& out, std::string& error);

  std::string disassemble(const Chunk& chunk) const;

 private:
  Chunk* chunk_ = nullptr;
  std::unordered_map<std::string, std::uint16_t> slots_;
  std::string* error_ = nullptr;
  bool ok_ = true;

  void emit(OpCode op);
  void emitU16(std::uint16_t value);
  void emitI64(std::int64_t value);
  void emitI32At(std::size_t offset, std::int32_t value);
  std::size_t emitJmpPlaceholder(OpCode op);

  bool compileStmt(const Stmt& stmt);
  bool compileExpr(const Expr& expr);
  bool fail(const std::string& message);
};

}  // namespace rajit
