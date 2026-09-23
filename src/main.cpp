#include "Interpreter.h"
#include "Lexer.h"
#include "Parser.h"
#include "ResourceChecker.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
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
            << "  --tokens          dump tokens\n"
            << "  --ast             dump AST\n"
            << "  --check           type/resource check only\n"
            << "  --no-jit          interpret without compiling hot loops\n"
            << "  --dump-jit        print bytecode after a hot loop compiles\n"
            << "  --threshold N     loop hits before JIT (default 32)\n";
}

}  // namespace

int main(int argc, char** argv) {
  bool dumpTokens = false;
  bool dumpAst = false;
  bool checkOnly = false;
  bool jitEnabled = true;
  bool dumpJit = false;
  std::int64_t threshold = 32;
  std::string path;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--tokens") {
      dumpTokens = true;
    } else if (arg == "--ast") {
      dumpAst = true;
    } else if (arg == "--check") {
      checkOnly = true;
    } else if (arg == "--no-jit") {
      jitEnabled = false;
    } else if (arg == "--dump-jit") {
      dumpJit = true;
    } else if (arg == "--threshold") {
      if (i + 1 >= argc) {
        printUsage(argv[0]);
        return 2;
      }
      threshold = std::stoll(argv[++i]);
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

  try {
    std::string source = readFile(path);
    rajit::Lexer lexer(source);
    auto tokens = lexer.tokenize();
    if (dumpTokens) {
      for (const auto& token : tokens) {
        std::cout << token.line << ":" << token.column << " "
                  << rajit::tokenTypeName(token.type) << " " << token.lexeme << "\n";
      }
    }

    rajit::Parser parser(tokens);
    rajit::Program program = parser.parse();
    if (dumpAst) {
      std::cout << rajit::dumpAst(program);
    }

    rajit::ResourceChecker checker;
    checker.check(program);
    if (checkOnly) {
      std::cout << "resource check passed\n";
      return 0;
    }

    rajit::Profiler profiler(threshold);
    rajit::JITCompiler jit(profiler);
    jit.setEnabled(jitEnabled);
    rajit::Interpreter interpreter(std::cout, profiler, jit);
    interpreter.interpret(program);

    if (dumpJit) {
      rajit::CodeGenerator gen;
      for (const auto& [id, count] : profiler.counts()) {
        if (const rajit::Chunk* chunk = jit.chunk(id)) {
          std::cout << "--- JIT loop " << id << " hits=" << count << " ---\n";
          std::cout << gen.disassemble(*chunk);
        }
      }
    }
    return 0;
  } catch (const rajit::ParseError& error) {
    std::cerr << error.what() << "\n";
    return 1;
  } catch (const rajit::CheckError& error) {
    std::cerr << error.what() << "\n";
    return 1;
  } catch (const rajit::RuntimeError& error) {
    std::cerr << error.what() << "\n";
    return 1;
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
