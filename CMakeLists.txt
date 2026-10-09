cmake_minimum_required(VERSION 3.16)
project(TinyRAJIT LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(TINYRAJIT_ENABLE_JIT
       "Build LLVM IR generator and ORC JIT (disable for frontend-only builds)"
       ON)

set(RAJIT_CORE
  src/Lexer.cpp
  src/AST.cpp
  src/Parser.cpp
  src/SymbolTable.cpp
  src/SemanticAnalyzer.cpp
  src/ResourceChecker.cpp
  src/Interpreter.cpp
  src/Profiler.cpp
  src/JITPolicy.cpp
)

add_library(rajit_core STATIC ${RAJIT_CORE})
target_include_directories(rajit_core PUBLIC include)

set(RAJIT_DRIVER_SOURCES src/main.cpp)

if(TINYRAJIT_ENABLE_JIT)
  find_package(LLVM CONFIG)
  if(NOT LLVM_FOUND)
    message(WARNING
      "LLVM was not found. Building TinyRAJIT without --print-ir/--jit/--benchmark. "
      "Install LLVM and pass -DLLVM_DIR=<prefix>/lib/cmake/llvm to enable JIT.")
    set(TINYRAJIT_ENABLE_JIT OFF)
  endif()
endif()

if(TINYRAJIT_ENABLE_JIT)
  message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
  include_directories(${LLVM_INCLUDE_DIRS})
  separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
  add_definitions(${LLVM_DEFINITIONS_LIST})
  list(APPEND RAJIT_DRIVER_SOURCES src/CodeGenerator.cpp src/JITCompiler.cpp)
else()
  list(APPEND RAJIT_DRIVER_SOURCES src/NoLLVMBackend.cpp)
endif()

add_executable(resourcejit ${RAJIT_DRIVER_SOURCES})
target_include_directories(resourcejit PRIVATE include)
target_link_libraries(resourcejit PRIVATE rajit_core)

if(TINYRAJIT_ENABLE_JIT)
  target_include_directories(resourcejit PRIVATE ${LLVM_INCLUDE_DIRS})
  if(LLVM_LINK_LLVM_DYLIB)
    target_link_libraries(resourcejit PRIVATE LLVM)
  else()
    llvm_map_components_to_libnames(LLVM_LIBS
      Core
      Support
      native
      orcjit
      ExecutionEngine
      MC
      MCJIT
      Target
      TransformUtils
      Analysis
    )
    target_link_libraries(resourcejit PRIVATE ${LLVM_LIBS})
  endif()
  if(NOT LLVM_ENABLE_RTTI)
    message(STATUS
      "LLVM was built without RTTI. TinyRAJIT keeps RTTI for AST dynamic_cast.")
  endif()
endif()

if(MSVC)
  target_compile_options(rajit_core PRIVATE /W4 /permissive-)
  target_compile_options(resourcejit PRIVATE /W4 /permissive-)
else()
  target_compile_options(rajit_core PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(resourcejit PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()

function(add_rajit_test name)
  add_executable(${name} tests/${name}.cpp)
  target_include_directories(${name} PRIVATE include ${CMAKE_SOURCE_DIR}/tests)
  target_link_libraries(${name} PRIVATE rajit_core)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_rajit_test(lexer_test)
add_rajit_test(parser_test)
add_rajit_test(semantic_test)
add_rajit_test(resource_test)
add_rajit_test(interpreter_test)
add_rajit_test(profiler_test)

if(TINYRAJIT_ENABLE_JIT)
  add_executable(jit_test tests/jit_test.cpp src/CodeGenerator.cpp src/JITCompiler.cpp)
  target_include_directories(jit_test PRIVATE include ${CMAKE_SOURCE_DIR}/tests ${LLVM_INCLUDE_DIRS})
  target_link_libraries(jit_test PRIVATE rajit_core)
  if(LLVM_LINK_LLVM_DYLIB)
    target_link_libraries(jit_test PRIVATE LLVM)
  else()
    target_link_libraries(jit_test PRIVATE ${LLVM_LIBS})
  endif()
  if(NOT MSVC)
    target_compile_options(jit_test PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME jit_test COMMAND jit_test)
endif()
