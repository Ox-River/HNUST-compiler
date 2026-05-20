# SysY2022 编译器

编译原理课程实验 — 基于 LLVM 的 SysY2022 语言编译器前端。

## 项目概述

本项目实现了一个完整的编译器前端，将 **SysY2022**（C 语言的精简子集）源代码编译为 **LLVM IR**（中间表示），再由 LLVM 后端生成可执行文件。

### 编译流程

```
SysY源代码 → 词法分析 → 语法分析(AST) → 语义分析 → IR代码生成 → LLVM IR → 可执行文件
```

### 支持的语言特性

| 特性 | 说明 |
|------|------|
| 基本类型 | `int`、`float` |
| 变量 | 变量定义与赋值 |
| 常量 | `const` 常量定义 |
| 数组 | 一维/多维数组的定义和访问 |
| 表达式 | 四则运算 `+ - * / %`，比较运算 `== != < > <= >=`，逻辑运算 `&& \|\| !` |
| 控制流 | `if`/`else` 分支、`while` 循环、`break`、`continue` |
| 函数 | 函数定义、调用、递归、多参数 |
| 输入输出 | `print()` 函数（映射到 `printf`） |

## 项目结构

```
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 项目文档
├── src/
│   ├── main.cpp                # 主程序入口
│   ├── lexer/
│   │   ├── Token.h             # Token 类型定义
│   │   ├── Lexer.h             # 词法分析器头文件
│   │   └── Lexer.cpp           # 词法分析器实现
│   ├── parser/
│   │   ├── Parser.h            # 语法分析器头文件
│   │   └── Parser.cpp          # 递归下降语法分析器实现
│   ├── ast/
│   │   ├── AST.h               # AST 节点定义（表达式/语句/声明）
│   │   └── AST.cpp             # AST 辅助函数实现
│   ├── semantic/
│   │   ├── SemanticAnalyzer.h  # 语义分析器头文件（符号表+类型检查）
│   │   └── SemanticAnalyzer.cpp# 语义分析器实现
│   └── codegen/
│       ├── IRGenerator.h       # LLVM IR 代码生成器头文件
│       └── IRGenerator.cpp     # IR 代码生成器实现
└── test/
    ├── test1_vars.sy           # 测试：变量与表达式
    ├── test2_func.sy           # 测试：函数定义与调用
    ├── test3_if.sy             # 测试：if/else 分支
    ├── test4_while.sy          # 测试：while 循环
    ├── test5_array.sy          # 测试：数组操作
    └── test6_comprehensive.sy  # 综合测试
```

## 构建与运行

### 环境要求

- **C++17** 或更高
- **CMake** >= 3.20
- **LLVM** >= 14.0（需要从源码编译或安装开发包）

### 编译步骤

```bash
# 1. 创建构建目录
mkdir build && cd build

# 2. CMake 配置（需指定 LLVM 路径）
cmake .. -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm

# 3. 编译
cmake --build .

# 4. 运行编译器
./sysy-compiler ../test/test1_vars.sy -o test1.ll --emit-ast
```

### 编译生成的 IR

```bash
# 使用 clang 将 LLVM IR 编译为可执行文件
clang out.ll -o program

# 运行
./program
```

## 核心设计

### 词法分析器 (Lexer)

手写的 DFA 词法分析器，将源代码字符串转换为 Token 流。支持：
- 关键字识别（`int`, `float`, `void`, `const`, `if`, `else`, `while`, `break`, `continue`, `return`）
- 标识符（以字母/下划线开头）
- 整型和浮点型字面量（含科学计数法）
- 运算符和分隔符
- 行注释 `//` 和块注释 `/* */`

### 语法分析器 (Parser)

递归下降法实现，严格按照 SysY2022 语法规范。运算符优先级：

```
LOrExp  ('||')
  LAndExp  ('&&')
    EqExp  ('==' '!=')
      RelExp  ('<' '>' '<=' '>=')
        AddExp  ('+' '-')
          MulExp  ('*' '/' '%')
            UnaryExp  ('-' '!' '+')
              PrimaryExp
```

### 语义分析器 (Semantic Analyzer)

- 基于作用域栈的符号表管理
- 变量/函数先声明后使用检查
- 类型兼容性检查（`int` ↔ `float` 隐式转换）
- `break`/`continue` 仅在循环内有效
- 函数调用实参/形参匹配

### IR 代码生成器 (IR Generator)

使用 LLVM IRBuilder API 生成 LLVM IR：
- 变量通过 `alloca`/`load`/`store` 访问
- 控制流通过基本块 + `br`/`condbr` 实现
- 函数调用映射到 LLVM `call` 指令
- `print()` 通过 `printf` 外部函数实现

## 参考

- [LLVM IR 教程](https://evian-zhang.github.io/llvm-ir-tutorial/)
- [LLVM 官方 Kaleidoscope 教程](https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/index.html)
- SysY2022 语言定义（见 `编译原理-实验附件/SysY2022语言定义-V1.pdf`）
