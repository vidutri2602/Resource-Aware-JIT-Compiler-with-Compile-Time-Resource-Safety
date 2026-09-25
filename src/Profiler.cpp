#include "Profiler.h"

#include <iomanip>
#include <sstream>

namespace tinyrjit {

Profiler::Profiler(int hotThreshold) : hotThreshold_(hotThreshold) {}

void Profiler::recordFunctionCall(const std::string& name) { counts_[name]++; }

int Profiler::getCallCount(const std::string& name) const {
  auto it = counts_.find(name);
  return it == counts_.end() ? 0 : it->second;
}

bool Profiler::isHot(const std::string& name) const {
  return getCallCount(name) >= hotThreshold_;
}

std::string Profiler::dump() const {
  std::ostringstream out;
  out << "Function           Calls       Status\n";
  out << "----------------------------------------\n";
  for (const auto& kv : counts_) {
    out << std::left << std::setw(18) << kv.first << " "
        << std::setw(11) << kv.second << " "
        << (kv.second >= hotThreshold_ ? "HOT" : "COLD") << "\n";
  }
  return out.str();
}

}  // namespace tinyrjit
