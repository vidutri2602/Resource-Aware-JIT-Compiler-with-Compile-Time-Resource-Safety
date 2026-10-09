#include "CodeGenerator.h"
#include "Error.h"
#include "Interpreter.h"
#include "JITCompiler.h"
#include "JITPolicy.h"
#include "Lexer.h"
#include "Parser.h"
#include "Profiler.h"
#include "ResourceChecker.h"
#include "SemanticAnalyzer.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

std::string readFile(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    throw std::runtime_error("cannot open file: " + path);
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

void printUsage(const char* argv0) {
  std::cerr << "Usage: " << argv0 << " [options] <file.tiny>\n"
            << "Options:\n"
            << "  --tokens       print token stream\n"
            << "  --ast          print AST\n"
            << "  --check        semantic + resource analysis only\n"
            << "  --run          interpret (default)\n"
            << "  --profile      interpret and print profiler / JIT policy\n"
            << "  --print-ir     generate LLVM IR for JIT-eligible functions\n"
            << "  --jit          compile eligible functions with LLVM ORC JIT\n"
            << "  --benchmark    prototype interpreter vs JIT timing\n"
            << "  --threshold N  hotness threshold (default 1000)\n";
}

std::vector<tinyrjit::Token> runLex(const std::string& source, bool dump) {
  tinyrjit::Lexer lexer(source);
  auto tokens = lexer.tokenize();
  if (dump) {
    for (const auto& token : tokens) {
      std::cout << tinyrjit::tokenTypeName(token.type);
      if (token.type == tinyrjit::TokenType::Identifier ||
          token.type == tinyrjit::TokenType::Number ||
          token.type == tinyrjit::TokenType::String) {
        std::cout << "(" << token.lexeme << ")";
      }
      std::cout << "\n";
    }
  }
  return tokens;
}

tinyrjit::Program runParse(std::vector<tinyrjit::Token> tokens, bool dump) {
  tinyrjit::Parser parser(std::move(tokens));
  tinyrjit::Program program = parser.parse();
  if (dump) {
    std::cout << tinyrjit::dumpAst(program);
  }
  return program;
}

void runCheck(tinyrjit::Program& program, tinyrjit::ResourceChecker& checker) {
  tinyrjit::SemanticAnalyzer semantics;
  semantics.analyze(program);
  checker.check(program);
}

void printResourceOk(const tinyrjit::ResourceChecker& checker) {
  std::cout << "Resource Analysis:\n";
  for (const auto& event : checker.trace()) {
    std::cout << event.handle << ": " << tinyrjit::resourceStateName(event.state) << "\n";
  }
  std::cout << "\n[OK] Resource analysis passed.\n";
}

}  // namespace

int main(int argc, char** argv) {
  bool dumpTokens = false;
  bool dumpAst = false;
  bool checkOnly = false;
  bool run = false;
  bool profile = false;
  bool printIr = false;
  bool jit = false;
  bool benchmark = false;
  int threshold = 1000;
  std::string path;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--tokens") {
      dumpTokens = true;
    } else if (arg == "--ast") {
      dumpAst = true;
    } else if (arg == "--check") {
      checkOnly = true;
    } else if (arg == "--run") {
      run = true;
    } else if (arg == "--profile") {
      profile = true;
    } else if (arg == "--print-ir") {
      printIr = true;
    } else if (arg == "--jit") {
      jit = true;
    } else if (arg == "--benchmark") {
      benchmark = true;
    } else if (arg == "--threshold") {
      if (i + 1 >= argc) {
        printUsage(argv[0]);
        return 2;
      }
      threshold = std::stoi(argv[++i]);
    } else if (arg == "--help" || arg == "-h") {
      printUsage(argv[0]);
      return 0;
    } else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "unknown option: " << arg << "\n";
      printUsage(argv[0]);
      return 2;
    } else {
      path = arg;
    }
  }

  if (path.empty()) {
    printUsage(argv[0]);
    return 2;
  }
  if (!dumpTokens && !dumpAst && !checkOnly && !run && !profile && !printIr && !jit &&
      !benchmark) {
    run = true;
  }

  try {
    std::string source = readFile(path);

    // Stage 1: Lexical Analysis
    auto tokens = runLex(source, dumpTokens);
    if (dumpTokens) {
      return 0;
    }

    // Stage 2: Syntax Analysis (Parser & AST)
    tinyrjit::Program program = runParse(std::move(tokens), dumpAst);
    if (dumpAst) {
      return 0;
    }

    // Stage 3: Semantic Analysis & Compile-Time Resource Safety Checking
    tinyrjit::ResourceChecker checker;
    runCheck(program, checker);

    if (checkOnly) {
      printResourceOk(checker);
      return 0;
    }

    auto eligible = tinyrjit::JITPolicy::computeEligible(program, checker);

    // Stage 4: LLVM IR Generation
    if (printIr) {
      tinyrjit::CodeGenerator gen;
      std::cout << gen.generate(program, eligible);
      if (!jit && !run && !profile && !benchmark) {
        return 0;
      }
    }

    tinyrjit::Profiler profiler(threshold);
    tinyrjit::JITPolicy policy(profiler, checker);
    for (const auto& fn : program.functions) {
      policy.markNumeric(fn->name, eligible.count(fn->name) != 0);
    }

    // Stage 5: LLVM ORC JIT Compilation and Execution
    if (jit || benchmark) {
      auto t0 = std::chrono::high_resolution_clock::now();
      tinyrjit::JITCompiler compiler;
      compiler.compile(program, eligible);
      auto t1 = std::chrono::high_resolution_clock::now();
      double compileMs =
          std::chrono::duration<double, std::milli>(t1 - t0).count();

      if (jit && !benchmark) {
        if (compiler.hasCompiled("main")) {
          std::int32_t result = compiler.invoke("main", {});
          std::cout << "Program Result: " << result << "\n";
        } else {
          tinyrjit::Interpreter interpreter(profiler, &compiler);
          std::int32_t result = interpreter.interpret(program);
          std::cout << "Program Result: " << result << "\n";
        }
        return 0;
      }

      if (benchmark) {
        tinyrjit::Profiler interpProfiler(threshold);
        tinyrjit::Interpreter interpreter(interpProfiler, nullptr);
        auto i0 = std::chrono::high_resolution_clock::now();
        std::int32_t interpResult = interpreter.interpret(program);
        auto i1 = std::chrono::high_resolution_clock::now();
        double interpMs = std::chrono::duration<double, std::milli>(i1 - i0).count();

        double jitExecMs = 0.0;
        std::int32_t jitResult = interpResult;
        if (compiler.hasCompiled("main")) {
          auto j0 = std::chrono::high_resolution_clock::now();
          jitResult = compiler.invoke("main", {});
          auto j1 = std::chrono::high_resolution_clock::now();
          jitExecMs = std::chrono::duration<double, std::milli>(j1 - j0).count();
        } else {
          std::cout << "Prototype measurement: main is not JIT-eligible; "
                       "JIT execution time is not reported.\n";
        }

        std::cout << "Prototype measurement (not a production benchmark)\n";
        std::cout << "Interpreter Time: " << interpMs << " ms\n";
        std::cout << "JIT Compilation Time: " << compileMs << " ms\n";
        if (compiler.hasCompiled("main")) {
          std::cout << "JIT Execution Time: " << jitExecMs << " ms\n";
          std::cout << "Interpreter Result: " << interpResult << "\n";
          std::cout << "JIT Result: " << jitResult << "\n";
        }
        return 0;
      }
    }

    // Stage 6: AST-Based Interpreter & Profiler
    tinyrjit::Interpreter interpreter(profiler, nullptr);
    std::int32_t result = interpreter.interpret(program);
    if (run || profile) {
      std::cout << "Program Result: " << result << "\n";
    }
    if (profile) {
      std::cout << "\n" << profiler.dump() << "\n";
      std::cout << policy.dump(program);
    }
    return 0;
  } catch (const tinyrjit::CompilerError& error) {
    std::cerr << error.what() << "\n";
    return 1;
  } catch (const std::exception& error) {
    std::cerr << "ERROR:\n" << error.what() << "\n";
    return 1;
  }
}
