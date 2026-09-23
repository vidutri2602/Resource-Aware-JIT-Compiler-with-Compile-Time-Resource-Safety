#pragma once

#include "AST.h"
#include "CodeGenerator.h"
#include "Profiler.h"

#include <cstdint>
#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>

namespace rajit {

class JITCompiler {
 public:
  explicit JITCompiler(Profiler& profiler);

  void setEnabled(bool enabled) { enabled_ = enabled; }
  bool enabled() const { return enabled_; }

  bool maybeCompile(const WhileStmt& loop, const std::vector<std::string>& locals);
  bool has(int loopId) const;
  // Runs compiled loop; reads/writes integer locals in `env`.
  bool run(int loopId, std::unordered_map<std::string, std::int64_t>& env,
           std::ostream& output);

  const Chunk* chunk(int loopId) const;

 private:
  Profiler& profiler_;
  bool enabled_ = true;
  std::unordered_map<int, Chunk> compiled_;
  CodeGenerator generator_;

  bool execute(const Chunk& chunk, std::unordered_map<std::string, std::int64_t>& env,
               std::ostream& output) const;
};

}  // namespace rajit
