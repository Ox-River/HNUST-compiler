cmake_minimum_required(VERSION 3.20)
project(SysY2022-Compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找LLVM (需要先安装LLVM 14+)
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# 源文件
set(SOURCES
    src/main.cpp
    src/lexer/Lexer.cpp
    src/parser/Parser.cpp
    src/ast/AST.cpp
    src/semantic/SemanticAnalyzer.cpp
    src/codegen/IRGenerator.cpp
)

# 可执行文件
add_executable(sysy-compiler ${SOURCES})

# 链接LLVM库
llvm_map_components_to_libnames(llvm_libs core support irreader)
target_link_libraries(sysy-compiler ${llvm_libs})
