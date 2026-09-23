#include "Profiler.h"

namespace rajit {

Profiler::Profiler(std::int64_t hotThreshold) : hotThreshold_(hotThreshold) {}

void Profiler::reset() { counts_.clear(); }

void Profiler::hit(int loopId) { counts_[loopId]++; }

std::int64_t Profiler::count(int loopId) const {
  auto it = counts_.find(loopId);
  return it == counts_.end() ? 0 : it->second;
}

bool Profiler::isHot(int loopId) const { return count(loopId) >= hotThreshold_; }

}  // namespace rajit
