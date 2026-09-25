#pragma once

#include "AST.h"

#include <memory>
#include <string>
#include <unordered_set>

namespace llvm {
namespace orc {
class LLJIT;
}
}  // namespace llvm

namespace tinyrjit {

class CodeGenerator {
 public:
  CodeGenerator();
  ~CodeGenerator();

  CodeGenerator(const CodeGenerator&) = delete;
  CodeGenerator& operator=(const CodeGenerator&) = delete;

  // Emit LLVM IR for numeric, resource-free functions. Returns IR text.
  std::string generate(const Program& program, const std::unordered_set<std::string>& eligible);

  void install(llvm::orc::LLJIT& jit);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace tinyrjit
