#include "CodeGenerator.h"
#include "Error.h"
#include "JITCompiler.h"

namespace tinyrjit {
namespace {

[[noreturn]] void llvmMissing(const char* feature) {
  throw JITError(0, std::string(feature) +
                        " requires LLVM. This binary was configured without LLVM. "
                        "Install LLVM, reconfigure CMake, and rebuild.");
}

}  // namespace

struct CodeGenerator::Impl {};

CodeGenerator::CodeGenerator() : impl_(std::make_unique<Impl>()) {}
CodeGenerator::~CodeGenerator() = default;

std::string CodeGenerator::generate(const Program&, const std::unordered_set<std::string>&) {
  llvmMissing("--print-ir / IR generation");
}

void CodeGenerator::install(llvm::orc::LLJIT&) { llvmMissing("ORC JIT install"); }

struct JITCompiler::Impl {};

JITCompiler::JITCompiler() : impl_(std::make_unique<Impl>()) {}
JITCompiler::~JITCompiler() = default;

void JITCompiler::compile(const Program&, const std::unordered_set<std::string>&) {
  llvmMissing("--jit / --benchmark");
}

bool JITCompiler::hasCompiled(const std::string&) const { return false; }

std::int32_t JITCompiler::invoke(const std::string&, const std::vector<std::int32_t>&) {
  llvmMissing("JIT invoke");
}

}  // namespace tinyrjit
