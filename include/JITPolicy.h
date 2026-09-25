#pragma once

#include "AST.h"
#include "Profiler.h"
#include "ResourceChecker.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace tinyrjit {

struct Eligibility {
  std::string function;
  bool hot = false;
  bool resourceEligible = false;
  bool jit = false;
};

// JIT only when a function is hot AND does not use Handle resources.
class JITPolicy {
 public:
  JITPolicy(const Profiler& profiler, const ResourceChecker& checker);

  bool isNumericEligible(const FunctionDecl& fn) const;
  bool resourceEligible(const std::string& name) const;
  Eligibility decide(const std::string& name, const FunctionDecl* fn) const;
  std::string dump(const Program& program) const;

  void markNumeric(const std::string& name, bool eligible);
  static std::unordered_set<std::string> computeEligible(const Program& program,
                                                         const ResourceChecker& checker);

 private:
  const Profiler& profiler_;
  const ResourceChecker& checker_;
  std::unordered_map<std::string, bool> numericEligible_;
};

bool functionIsNumeric(const FunctionDecl& fn);

}  // namespace tinyrjit
