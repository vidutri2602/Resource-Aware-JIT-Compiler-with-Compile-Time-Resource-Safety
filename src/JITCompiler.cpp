#include "JITCompiler.h"

#include "CodeGenerator.h"
#include "Error.h"

#include "llvm/Config/llvm-config.h"
#include "llvm/ExecutionEngine/Orc/AbsoluteSymbols.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/TargetSelect.h"

#include <cstdint>

extern "C" void tinyrajit_noop_runtime() {}

namespace tinyrjit {

struct JITCompiler::Impl {
  std::unique_ptr<llvm::orc::LLJIT> jit;
};

JITCompiler::JITCompiler() : impl_(std::make_unique<Impl>()) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  auto expected = llvm::orc::LLJITBuilder().create();
  if (!expected) {
    std::string msg;
    llvm::handleAllErrors(expected.takeError(), [&](const llvm::ErrorInfoBase& info) {
      msg = info.message();
    });
    throw JITError(0, "Failed to create LLVM ORC LLJIT: " + msg);
  }
  impl_->jit = std::move(*expected);

  auto& jd = impl_->jit->getMainJITDylib();
  auto gen = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
      impl_->jit->getDataLayout().getGlobalPrefix());
  if (gen) {
    jd.addGenerator(std::move(*gen));
  }

  // Under Windows/MinGW, LLVM codegen for 'main' generates a call to '__main' runtime startup.
  llvm::orc::SymbolMap symbols;
  symbols[impl_->jit->mangleAndIntern("__main")] = {
      llvm::orc::ExecutorAddr::fromPtr(&tinyrajit_noop_runtime),
      llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable
  };
  symbols[impl_->jit->getExecutionSession().intern("__main")] = {
      llvm::orc::ExecutorAddr::fromPtr(&tinyrajit_noop_runtime),
      llvm::JITSymbolFlags::Exported | llvm::JITSymbolFlags::Callable
  };
  (void)jd.define(llvm::orc::absoluteSymbols(std::move(symbols)));
}

JITCompiler::~JITCompiler() = default;

void JITCompiler::compile(const Program& program, const std::unordered_set<std::string>& eligible) {
  compiled_.clear();
  CodeGenerator gen;
  ir_ = gen.generate(program, eligible);
  if (eligible.empty()) {
    return;
  }
  gen.install(*impl_->jit);
  compiled_ = eligible;
}

bool JITCompiler::hasCompiled(const std::string& name) const {
  return compiled_.count(name) != 0;
}

std::int32_t JITCompiler::invoke(const std::string& name, const std::vector<std::int32_t>& args) {
  if (!hasCompiled(name)) {
    throw JITError(0, "Function '" + name + "' was not JIT-compiled.");
  }
  auto addrOrErr = impl_->jit->lookup(name);
  if (!addrOrErr) {
    std::string msg;
    llvm::handleAllErrors(addrOrErr.takeError(), [&](const llvm::ErrorInfoBase& info) {
      msg = info.message();
    });
    throw JITError(0, "ORC lookup failed for '" + name + "': " + msg);
  }
  auto addr = *addrOrErr;

  auto asPtr = [&](auto dummy) {
    using Fn = decltype(dummy);
#if LLVM_VERSION_MAJOR >= 15
    return addr.toPtr<Fn>();
#else
    return reinterpret_cast<Fn>(static_cast<std::uintptr_t>(addr.getAddress()));
#endif
  };

  switch (args.size()) {
    case 0:
      return asPtr(static_cast<std::int32_t (*)()>(nullptr))();
    case 1:
      return asPtr(static_cast<std::int32_t (*)(std::int32_t)>(nullptr))(args[0]);
    case 2:
      return asPtr(static_cast<std::int32_t (*)(std::int32_t, std::int32_t)>(nullptr))(args[0],
                                                                                    args[1]);
    case 3:
      return asPtr(static_cast<std::int32_t (*)(std::int32_t, std::int32_t, std::int32_t)>(nullptr))(
          args[0], args[1], args[2]);
    case 4:
      return asPtr(static_cast<std::int32_t (*)(std::int32_t, std::int32_t, std::int32_t,
                                               std::int32_t)>(nullptr))(args[0], args[1], args[2],
                                                                        args[3]);
    default:
      throw JITError(0, "JIT invoke supports at most 4 arguments.");
  }
}

}  // namespace tinyrjit
