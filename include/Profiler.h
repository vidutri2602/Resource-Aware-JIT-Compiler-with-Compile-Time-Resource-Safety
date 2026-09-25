#pragma once

#include <string>
#include <unordered_map>

namespace tinyrjit {

class Profiler {
 public:
  explicit Profiler(int hotThreshold = 1000);

  void recordFunctionCall(const std::string& name);
  int getCallCount(const std::string& name) const;
  bool isHot(const std::string& name) const;
  int hotThreshold() const { return hotThreshold_; }
  const std::unordered_map<std::string, int>& counts() const { return counts_; }

  std::string dump() const;

 private:
  int hotThreshold_;
  std::unordered_map<std::string, int> counts_;
};

}  // namespace tinyrjit
