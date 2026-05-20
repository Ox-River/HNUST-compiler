# SysY2022 编译器 — 编译原理课程实验

基于 **LLVM** 的 **SysY2022** 语言（C 语言子集）编译器前端实现。

---

## 目录

- [项目概述](#项目概述)
- [编译流程](#编译流程)
- [支持的语言特性](#支持的语言特性)
- [项目结构](#项目结构)
- [构建与运行](#构建与运行)
- [核心设计详解](#核心设计详解)
  - [整体架构思路](#整体架构思路)
  - [阶段一：词法分析 Lexer](#阶段一词法分析-lexer)
  - [阶段二：语法分析 Parser](#阶段二语法分析-parser)
  - [阶段三：AST 抽象语法树](#阶段三ast-抽象语法树)
  - [阶段四：语义分析 Semantic Analyzer](#阶段四语义分析-semantic-analyzer)
  - [阶段五：IR 代码生成 IR Generator](#阶段五ir-代码生成-ir-generator)
- [关键设计决策](#关键设计决策)
- [测试用例](#测试用例)
- [参考](#参考)

---

## 项目概述

本项目实现了一个完整的编译器前端，将 SysY2022 源代码编译为 **LLVM IR**（中间表示），再利用 LLVM 工具链生成可执行文件。整个编译器约 2000 行 C++ 代码，分为词法分析、语法分析（构建 AST）、语义分析、IR 代码生成四个清晰的阶段。

### 技术栈

| 技术 | 用途 |
|------|------|
| C++17 | 主语言，利用智能指针管理内存 |
| LLVM 14 | IRBuilder API 用于生成 LLVM IR |
| CMake | 跨平台构建系统 |

---

## 编译流程

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│ SysY源文件   │───→│  词法分析器   │───→│  语法分析器   │───→│  语义分析器   │───→│  IR生成器    │
│  (.sy)       │    │  Token流     │    │  AST        │    │  带类型AST   │    │  LLVM IR    │
└──────────────┘    └──────────────┘    └──────────────┘    └──────────────┘    └──────┬───────┘
                                                                                       │
                                                                              ┌────────▼───────┐
                                                                              │  LLVM后端      │
                                                                              │  (clang/llc)   │
                                                                              └────────┬───────┘
                                                                                       │
                                                                              ┌────────▼───────┐
                                                                              │  可执行文件    │
                                                                              └────────────────┘
```

---

## 支持的语言特性

| 特性 | 示例 |
|------|------|
| 基本类型 | `int`, `float`, `void` |
| 变量定义与赋值 | `int a; a = 10;` |
| 常量 | `const int N = 100;` |
| 浮点运算 | `float pi = 3.14; pi = pi * 2.0;` |
| 一维数组 | `int arr[10]; arr[0] = 5;` |
| 多维数组 | `int mat[3][4]; mat[0][1] = 10;` |
| 算术运算 | `+`, `-`, `*`, `/`, `%` |
| 比较运算 | `==`, `!=`, `<`, `>`, `<=`, `>=` |
| 逻辑运算 | `&&`, `\|\|`, `!` |
| if/else 分支 | `if (x > 0) { ... } else { ... }` |
| while 循环 | `while (i < 10) { ... }` |
| break/continue | `break;` `continue;` |
| 函数定义 | `int add(int x, int y) { return x + y; }` |
| 函数调用 | `int s = add(3, 5);` |
| 递归函数 | `int fib(int n) { if (n <= 1) return n; ... }` |
| 输出 | `print(x); print(3.14);` |

---

## 项目结构

```
.
├── CMakeLists.txt              # CMake 构建配置（链接 LLVM）
├── README.md                   # 本文档
├── src/
│   ├── main.cpp                # 程序入口：串联各阶段、命令行参数、文件读写
│   ├── lexer/
│   │   ├── Token.h             # Token 类型枚举与 Token 结构体定义
│   │   ├── Lexer.h             # 词法分析器接口
│   │   └── Lexer.cpp           # 词法分析器实现（手写 DFA）
│   ├── parser/
│   │   ├── Parser.h            # 语法分析器接口与文法注释
│   │   └── Parser.cpp          # 递归下降语法分析器实现
│   ├── ast/
│   │   ├── AST.h               # AST 节点定义（7种Expr + 9种Stmt）
│   │   └── AST.cpp             # AST 节点的 toString 实现
│   ├── semantic/
│   │   ├── SemanticAnalyzer.h  # 符号表 + 语义分析器接口
│   │   └── SemanticAnalyzer.cpp# 语义分析器实现（作用域栈、类型检查）
│   └── codegen/
│       ├── IRGenerator.h       # LLVM IR 生成器接口
│       └── IRGenerator.cpp     # IR 生成器实现（约400行，最核心模块）
└── test/
    ├── test1_vars.sy           # 测试1: 变量声明、赋值与表达式
    ├── test2_func.sy           # 测试2: 函数定义与调用
    ├── test3_if.sy             # 测试3: if/else 分支与比较运算
    ├── test4_while.sy          # 测试4: while 循环、break/continue
    ├── test5_array.sy          # 测试5: 数组定义、赋值与访问
    └── test6_comprehensive.sy  # 测试6: 综合测试（递归+迭代+浮点+循环）
```

---

## 构建与运行

### 环境要求

- **C++17** 及以上
- **CMake** >= 3.20
- **LLVM** >= 14.0（需预编译或安装开发库）

### 编译

```bash
mkdir build && cd build
cmake .. -DLLVM_DIR=/path/to/llvm/lib/cmake/llvm
cmake --build .
```

### 使用

```bash
# 编译 SysY 源文件为 LLVM IR
./sysy-compiler test/test1_vars.sy -o output.ll

# 查看 AST（调试用）
./sysy-compiler test/test1_vars.sy --emit-ast

# 使用 clang 将 IR 编译为可执行文件
clang output.ll -o program
./program
```

---

## 核心设计详解

### 整体架构思路

编译器的核心任务是将**源代码字符串**逐步转换为**目标代码**。这个过程中存在两个关键问题：

1. **如何组织转换的中间状态？** — 我们引入 **Token 流** 和 **AST** 作为中间表示（IR）。
2. **各个阶段之间如何解耦？** — 每个阶段是一个独立的模块，只依赖上一个阶段的输出。

这种分阶段（pipeline）架构的好处：
- 每个阶段职责单一，便于理解和维护
- 可以独立测试每个阶段（比如只测词法分析，不跑完整编译）
- 中间状态（Token流、AST）可以可视化输出，方便调试

```
字符串 → Lexer → Token流 → Parser → AST → SemanticAnalyzer → 带类型AST → IRGenerator → LLVM IR
```

### 阶段一：词法分析 Lexer

**为什么手写而不是用 flex/lex？** 对于 SysY 这种简单语言，手写词法分析器代码量很小（约150行），而且不引入额外的工具依赖和生成步骤。同时也帮助我们深入理解 DFA（确定有限自动机）的工作原理。

**设计思路：**
- 维护一个指针 `pos_` 在源代码字符串上移动，每次读取一个字符
- 通过前瞻（lookahead）区分：`=` vs `==`，`&` vs `&&`，`/` vs `//` vs `/*`
- 标识符和关键字统一先识别为标识符，再查关键字表判断

**关键实现细节：**

| 问题 | 解决方案 |
|------|---------|
| 单字符运算符 (`+`) vs 双字符 (`++`) | SysY 无 `++`，但需区分 `=` 和 `==`，用一个 `advance()` 后 `peek()` 判断第二个字符 |
| 关键字 vs 标识符 | 先按标识符规则收集字符，然后查 `unordered_map<string, TokenType>` |
| 浮点数识别 | 整数部分 → 可选 `.` + 小数部分 → 可选 `e/E` + 指数部分 |
| 注释处理 | `//` 跳过直到 `\n`，`/* */` 跳过直到 `*/`，未闭合的块注释报错 |
| 错误恢复 | 遇到非法字符时生成 ERROR token 而不是直接崩溃，使编译器能继续分析后续代码 |

### 阶段二：语法分析 Parser

**为什么用递归下降法？** 对于 SysY2022 这种 LL(1) 兼容的文法，递归下降法是最直观的方法——每个非终结符对应一个解析函数，代码结构直接反映文法结构。相比 LR 类方法（如 yacc/bison），递归下降更容易理解和调试，错误信息也更友好。

**设计思路：**
- 严格按照 SysY2022 语法规范的 EBNF 编写解析函数
- 每个解析函数返回对应的 AST 节点指针
- 通过递归调用实现嵌套结构（如 `if { while { ... } }`）
- 运算符优先级通过函数调用层次实现：`parseLOrExp()` → `parseLAndExp()` → `parseEqExp()` → ... → `parseUnaryExp()` → `parsePrimaryExp()`

**运算符优先级实现（由低到高）：**

```
优先级        解析函数              运算符
───────      ────────              ──────
最低          parseLOrExp()        ||
  ↑           parseLAndExp()       &&
  ↑           parseEqExp()         ==  !=
  ↑           parseRelExp()        <  >  <=  >=
  ↑           parseAddExp()        +  -
  ↑           parseMulExp()        *  /  %
  ↑           parseUnaryExp()      -  !  +  (一元)
  ↑           parsePrimaryExp()    字面量 标识符 调用 括号
```

**关键设计决策 — 如何区分变量声明和函数定义：**
这两种语法都以 `Type Identifier` 开头（比如 `int foo`），需要看第三个 token 才能区分：
- 是 `(` → 函数定义：`int foo(int x) { ... }`
- 是 `,` / `;` / `[` / `=` → 变量声明：`int foo, bar;` 或 `int foo = 5;`

Solution: 我们先保存位置 → 读取 `Type` → 读取 `Identifier` → 检查第三个 token：
```cpp
// 使用 savedPos 实现无限前瞻（LL(k)）
if (next token is '(') → 函数定义
else → 变量声明
```

**左值 vs 表达式调用的歧义消解：**
当解析语句时遇到 `Identifier`，它可能是：
1. 赋值语句的左值：`a = expr;`
2. 普通表达式语句：`foo();` 或 `a + b;`

Solution: 读取标识符后，根据后续 token 判断：`[` 为数组访问，`=` 为赋值，其余回退按表达式处理。

### 阶段三：AST 抽象语法树

AST 是编译器各阶段之间传递的核心数据结构。我们使用 C++ 的类继承体系来表达不同类型的 AST 节点。

**设计思路：**

```
                    ASTNode (概念基类)
                   /          \
              Expr            Stmt
           (表达式)           (语句)
              │                │
    ┌─────────┼─────────┐      ├─── BlockStmt
    │         │         │      ├─── IfStmt
 IntLiteral BinaryExpr CallExpr  ├─── WhileStmt
 FloatLiteral UnaryExpr          ├─── AssignStmt
 Identifier                       ├─── ReturnStmt
 ArrayAccess                      ├─── ExprStmt
                                  ├─── BreakStmt/ContinueStmt
                                  ├─── VarDecl
                                  └─── FuncDef
```

**为什么用 `unique_ptr` 而非裸指针？**
- AST 节点构成严格的树形结构（每个子节点只有一个父节点），`unique_ptr` 完美表达所有权语义
- 自动释放内存，无需手动 `delete`
- 避免悬空指针问题

**表达式类型标注：**
每个 `Expr` 节点上都有一个 `exprType` 字段，在语义分析阶段被填充。这样 IR 生成器可以直接查询表达式类型，不需要重新推导。

### 阶段四：语义分析 Semantic Analyzer

语义分析器的作用是检查程序是否符合语言的语义规则——这些规则是文法无法表达的。

**设计思路 — 两次遍历：**

1. **第一次遍历（声明收集）：** 收集所有全局变量和函数签名，注册到全局符号表。这样函数就可以相互调用（支持前向引用）。
2. **第二次遍历（体检查）：** 逐个分析函数体，进行类型检查和语句检查。

**符号表设计：**
```
SymbolTable
  └── vector<Scope>           ← 作用域栈
        └── Scope
              └── unordered_map<string, SymbolInfo>   ← 符号映射
```

- 进入新作用域（`{` / 函数体）→ `enterScope()` 压入新 Scope
- 离开作用域（`}` / 函数结束）→ `leaveScope()` 弹出当前 Scope
- 查找符号 → 从栈顶向栈底搜索，实现作用域嵌套规则
- 同一作用域内不能重复定义

**检查规则：**

| 检查项 | 说明 |
|--------|------|
| 变量先声明后使用 | 查找符号表，未找到则报错 |
| 类型兼容性 | `int` → `float` 允许隐式转换，`float` → `int` 不允许 |
| 取模限制 | `%` 运算符的操作数必须都是 `int` |
| break/continue | 记录 `loopDepth_`，非循环内使用则报错 |
| 返回值检查 | 检查 `return` 表达式的类型是否匹配函数声明 |
| 函数实参匹配 | 检查参数数量和类型是否与声明一致 |
| 常量赋值 | `const` 变量不能作为左值 |

### 阶段五：IR 代码生成 IR Generator

这是最核心的模块，将 AST 转换为 LLVM IR。我们使用 LLVM 提供的 **IRBuilder API** 来生成 IR。

**为什么用 IRBuilder 而不是手写 IR 字符串？**
- IRBuilder 自动处理 SSA（静态单赋值）命名和基本块管理
- 类型安全：API 确保生成合法 IR，减少格式错误
- LLVM 版本升级时 API 保持兼容

**核心概念映射：**

| SysY 概念 | LLVM IR 实现 |
|-----------|-------------|
| 局部变量 | `alloca` 分配栈空间 + `load`/`store` 读写 |
| 全局变量 | `GlobalVariable`（代码中暂时用内部链接实现） |
| 表达式求值 | 递归计算子表达式，结果作为 `Value*` 返回 |
| 赋值语句 | 获取左值指针 → 计算右值 → `store` |
| if/else | `BasicBlock` × 3（then/else/merge），`CondBr` 跳转 |
| while | `BasicBlock` × 3（cond/body/exit），break/continue 跳转到对应块 |
| 函数调用 | `CallInst`，`print` 特殊映射到 `printf` |
| 类型转换 | `SIToFP`（int→float），`FPToSI`（float→int），`FPExt`（float→double） |

**控制流生成示意（if/else）：**

```
        [条件表达式]
             │
        i1 condVal
             │
   CondBr(condVal, thenBB, elseBB)
             │
    ┌────────┴────────┐
 [thenBB]          [elseBB]
    │                  │
  生成then体         生成else体
    │                  │
  Br(mergeBB)       Br(mergeBB)
    │                  │
    └────────┬────────┘
        [mergeBB]
           │
        继续后续代码
```

**while 循环控制流：**

```
     ┌──────────────┐
     │   condBB     │←──────────┐
     │  条件检查     │           │
     └──────┬───────┘           │
       CondBr(cond, body, exit)  │
            │                    │
     ┌──────▼───────┐           │
     │   bodyBB     │           │
     │  循环体       │           │
     │  break → exit │          │
     │  ───────────  │           │
     └──────────────┘           │
       Br(condBB) ──────────────┘
            │
     ┌──────▼───────┐
     │   exitBB     │
     │  循环后代码   │
     └──────────────┘
```

**print 函数的特殊处理：**
由于 SysY 只要求一个 `print` 输出函数，我们将其映射到 C 标准库的 `printf`：
- `print(42)` → `printf("%d\n", 42)`
- `print(3.14)` → `printf("%f\n", (double)3.14)`
- `print()` → `printf("\n")`

要根据参数类型选择正确的格式字符串，并且 `float` 类型的值需要先 `FPExt` 为 `double`（因为 printf 的可变参数规则）。

**变量存储策略：**
每个声明的变量通过 `CreateAlloca` 在栈上分配空间，然后将其地址记录在 `namedValues_` 映射中。之后对该变量的读写都通过 `CreateLoad` / `CreateStore` 操作这个地址。LLVM 的 `mem2reg` Pass 可以后续优化这些 alloca/load/store 为寄存器操作。

---

## 关键设计决策

### 1. 为什么分五个独立模块而不是写在一个文件里？

大型软件项目中，模块化是最基本的原则。编译器的五个阶段各自有清晰的输入输出接口：
- **可测试性：** 可以单独加载 Lexer 检查 token 输出
- **可替换性：** 如果想升级语法分析策略（如改用 LR），只需替换 Parser 模块
- **可读性：** 每个文件 200-400 行，而非单文件 2000 行

### 2. 错误处理策略

我们没有在第一个错误处就停止编译，而是：
- 收集所有错误（`errors_` 向量）
- 在可能的情况下尝试恢复并继续解析
- 最后统一输出所有错误

这样用户一次可以看到所有问题，而不是修一个错编译一次。

### 3. 为什么用递归下降而不是 yacc/bison？

对于教学项目来说，递归下降法的优势是：
- 代码即文法，不需要学额外的 DSL 语法
- 调试时可以单步跟踪 C++ 调用栈
- 可以嵌入任意 C++ 逻辑（如运算符优先级直接用函数调用层次表达）
- 错误信息可以包含丰富的上下文

---

## 测试用例

| 文件 | 测试内容 | 期望 |
|------|---------|------|
| `test1_vars.sy` | int/float 变量定义、赋值、类型转换 | 输出 10, 15.0 |
| `test2_func.sy` | 有参函数定义与调用 | 输出 8 |
| `test3_if.sy` | if/else 分支、比较运算、abs 函数 | 输出 5, 3, 5 |
| `test4_while.sy` | while 循环、break、累加 | 输出 55, 15 |
| `test5_array.sy` | 一维数组定义、赋值、访问 | 输出 10, 30, 50 |
| `test6_comprehensive.sy` | 递归(阶乘) + 迭代(斐波那契) + 浮点 + 循环 | 输出 120, 55, 3.14, 55 |

---

## 参考

- [LLVM IR 教程（中文）](https://evian-zhang.github.io/llvm-ir-tutorial/)
- [LLVM 官方 Kaleidoscope 教程](https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/index.html)
- [SysY2022 语言定义](编译原理-实验附件/SysY2022语言定义-V1.pdf)
- [SysY2022 运行时库](编译原理-实验附件/SysY2022运行时库-V1.pdf)
