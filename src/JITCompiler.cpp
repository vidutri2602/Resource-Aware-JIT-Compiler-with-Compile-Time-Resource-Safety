#include "JITCompiler.h"

#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace rajit {

namespace {

std::int64_t readI64(const std::vector<std::uint8_t>& code, std::size_t ip) {
  std::int64_t value = 0;
  std::memcpy(&value, code.data() + ip, sizeof(value));
  return value;
}

std::int32_t readI32(const std::vector<std::uint8_t>& code, std::size_t ip) {
  std::int32_t value = 0;
  std::memcpy(&value, code.data() + ip, sizeof(value));
  return value;
}

std::uint16_t readU16(const std::vector<std::uint8_t>& code, std::size_t ip) {
  std::uint16_t value = 0;
  std::memcpy(&value, code.data() + ip, sizeof(value));
  return value;
}

}  // namespace

JITCompiler::JITCompiler(Profiler& profiler) : profiler_(profiler) {}

bool JITCompiler::has(int loopId) const { return compiled_.count(loopId) != 0; }

const Chunk* JITCompiler::chunk(int loopId) const {
  auto it = compiled_.find(loopId);
  return it == compiled_.end() ? nullptr : &it->second;
}

bool JITCompiler::maybeCompile(const WhileStmt& loop, const std::vector<std::string>& locals) {
  if (!enabled_ || has(loop.loopId) || !profiler_.isHot(loop.loopId)) {
    return has(loop.loopId);
  }
  Chunk chunk;
  std::string error;
  if (!generator_.compileWhile(loop, locals, chunk, error)) {
    return false;
  }
  compiled_[loop.loopId] = std::move(chunk);
  return true;
}

bool JITCompiler::run(int loopId, std::unordered_map<std::string, std::int64_t>& env,
                      std::ostream& output) {
  auto it = compiled_.find(loopId);
  if (it == compiled_.end()) {
    return false;
  }
  return execute(it->second, env, output);
}

bool JITCompiler::execute(const Chunk& chunk, std::unordered_map<std::string, std::int64_t>& env,
                          std::ostream& output) const {
  std::vector<std::int64_t> slots(chunk.slotNames.size(), 0);
  for (std::size_t i = 0; i < chunk.slotNames.size(); ++i) {
    auto found = env.find(chunk.slotNames[i]);
    if (found != env.end()) {
      slots[i] = found->second;
    }
  }

  std::vector<std::int64_t> stack;
  std::size_t ip = 0;
  auto push = [&](std::int64_t v) { stack.push_back(v); };
  auto pop = [&]() {
    if (stack.empty()) {
      throw std::runtime_error("JIT stack underflow");
    }
    std::int64_t v = stack.back();
    stack.pop_back();
    return v;
  };

  while (ip < chunk.code.size()) {
    auto op = static_cast<OpCode>(chunk.code[ip++]);
    switch (op) {
      case OpCode::Push:
        push(readI64(chunk.code, ip));
        ip += 8;
        break;
      case OpCode::Load: {
        std::uint16_t slot = readU16(chunk.code, ip);
        ip += 2;
        push(slots[slot]);
        break;
      }
      case OpCode::Store: {
        std::uint16_t slot = readU16(chunk.code, ip);
        ip += 2;
        slots[slot] = pop();
        break;
      }
      case OpCode::Add: {
        auto b = pop();
        auto a = pop();
        push(a + b);
        break;
      }
      case OpCode::Sub: {
        auto b = pop();
        auto a = pop();
        push(a - b);
        break;
      }
      case OpCode::Mul: {
        auto b = pop();
        auto a = pop();
        push(a * b);
        break;
      }
      case OpCode::Div: {
        auto b = pop();
        auto a = pop();
        if (b == 0) {
          throw std::runtime_error("JIT division by zero");
        }
        push(a / b);
        break;
      }
      case OpCode::Mod: {
        auto b = pop();
        auto a = pop();
        if (b == 0) {
          throw std::runtime_error("JIT division by zero");
        }
        push(a % b);
        break;
      }
      case OpCode::Neg:
        push(-pop());
        break;
      case OpCode::Eq: {
        auto b = pop();
        auto a = pop();
        push(a == b);
        break;
      }
      case OpCode::Ne: {
        auto b = pop();
        auto a = pop();
        push(a != b);
        break;
      }
      case OpCode::Lt: {
        auto b = pop();
        auto a = pop();
        push(a < b);
        break;
      }
      case OpCode::Le: {
        auto b = pop();
        auto a = pop();
        push(a <= b);
        break;
      }
      case OpCode::Gt: {
        auto b = pop();
        auto a = pop();
        push(a > b);
        break;
      }
      case OpCode::Ge: {
        auto b = pop();
        auto a = pop();
        push(a >= b);
        break;
      }
      case OpCode::Jmp:
        ip = static_cast<std::size_t>(readI32(chunk.code, ip));
        break;
      case OpCode::JmpZ: {
        std::int32_t target = readI32(chunk.code, ip);
        ip += 4;
        if (pop() == 0) {
          ip = static_cast<std::size_t>(target);
        }
        break;
      }
      case OpCode::Print:
        output << pop() << "\n";
        break;
      case OpCode::Halt:
        goto done;
    }
  }

done:
  for (std::size_t i = 0; i < chunk.slotNames.size(); ++i) {
    env[chunk.slotNames[i]] = slots[i];
  }
  return true;
}

}  // namespace rajit
