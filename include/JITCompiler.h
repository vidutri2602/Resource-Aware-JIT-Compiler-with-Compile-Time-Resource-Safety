#pragma once

#include "AST.h"
#include "Interpreter.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace tinyrjit {

class JITCompiler : public CompiledFunctionHost {
 public:
  JITCompiler();
  ~JITCompiler();

  JITCompiler(const JITCompiler&) = delete;
  JITCompiler& operator=(const JITCompiler&) = delete;

  void compile(const Program& program, const std::unordered_set<std::string>& eligible);
  const std::string& ir() const { return ir_; }
  bool hasCompiled(const std::string& name) const override;
  std::int32_t invoke(const std::string& name, const std::vector<std::int32_t>& args) override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  std::string ir_;
  std::unordered_set<std::string> compiled_;
};

}  // namespace tinyrjit
