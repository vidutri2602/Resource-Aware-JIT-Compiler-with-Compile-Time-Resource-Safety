cmake_minimum_required(VERSION 3.16)
project(ResourceAwareJIT LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(rajit_lib
  src/Lexer.cpp
  src/Parser.cpp
  src/AST.cpp
  src/SymbolTable.cpp
  src/ResourceChecker.cpp
  src/Interpreter.cpp
  src/Profiler.cpp
  src/JITCompiler.cpp
  src/CodeGenerator.cpp
)

target_include_directories(rajit_lib PUBLIC include)

add_executable(rajit src/main.cpp)
target_link_libraries(rajit PRIVATE rajit_lib)

enable_testing()

function(rajit_test name)
  add_executable(${name} tests/${name}.cpp)
  target_link_libraries(${name} PRIVATE rajit_lib)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

rajit_test(lexer_test)
rajit_test(parser_test)
rajit_test(resource_test)
rajit_test(jit_test)
