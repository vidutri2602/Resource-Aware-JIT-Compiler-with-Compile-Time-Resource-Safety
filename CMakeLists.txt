cmake_minimum_required(VERSION 3.16)
project(ResourceAwareJIT LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Expected LLVM: 14–21 (ORC LLJIT + IRBuilder). Discovery uses LLVMConfig.cmake
# from a standard LLVM install (no hardcoded paths).
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

if(NOT DEFINED LLVM_ENABLE_RTTI)
  set(LLVM_ENABLE_RTTI ON)
endif()

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

add_executable(resourcejit
  src/main.cpp
  src/CodeGenerator.cpp
  src/JITCompiler.cpp
)
target_include_directories(resourcejit PRIVATE include ${LLVM_INCLUDE_DIRS})
target_link_libraries(resourcejit PRIVATE rajit_core)

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
    "LLVM was built without RTTI. TinyRAJIT keeps RTTI for AST dynamic_cast; "
    "this is the usual setup when linking the LLVM shared library.")
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
