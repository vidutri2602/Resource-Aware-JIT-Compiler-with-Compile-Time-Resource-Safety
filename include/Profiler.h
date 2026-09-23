#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace rajit {

class Profiler {
 public:
  explicit Profiler(std::int64_t hotThreshold = 1000);

  void reset();
  void hit(int loopId);
  std::int64_t count(int loopId) const;
  bool isHot(int loopId) const;
  std::int64_t threshold() const { return hotThreshold_; }
  void setThreshold(std::int64_t threshold) { hotThreshold_ = threshold; }

  const std::unordered_map<int, std::int64_t>& counts() const { return counts_; }

 private:
  std::int64_t hotThreshold_;
  std::unordered_map<int, std::int64_t> counts_;
};

}  // namespace rajit
