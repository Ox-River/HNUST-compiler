#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "semantic/SemanticAnalyzer.h"
#include "codegen/IRGenerator.h"

using namespace sysy;

// 从文件读取源代码
static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "错误: 无法打开文件 '" << path << "'\n";
        std::exit(1);
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

// 打印分隔线
static void printSection(const std::string& title) {
    std::cout << "\n========== " << title << " ==========\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "用法: sysy-compiler <输入文件.sy> [-o <输出文件.ll>] [--emit-ast]\n";
        std::cerr << "  --emit-ast  输出AST树形结构（调试用）\n";
        std::cerr << "  -o <file>   指定LLVM IR输出文件（默认为out.ll）\n";
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = "out.ll";
    bool emitAST = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            outputPath = argv[++i];
        } else if (arg == "--emit-ast") {
            emitAST = true;
        }
    }

    // 1. 读取源代码
    std::string source = readFile(inputPath);
    std::cout << "输入文件: " << inputPath << " (" << source.size() << " 字节)\n";

    // 2. 词法分析
    printSection("词法分析");
    Lexer lexer(source);
    auto tokens = lexer.tokenize();

    if (!lexer.errors().empty()) {
        std::cerr << "词法错误:\n";
        for (auto& err : lexer.errors()) {
            std::cerr << "  " << err << "\n";
        }
        return 1;
    }
    std::cout << "词法分析通过 ✓  (" << tokens.size() << " 个Token)\n";

    // 3. 语法分析
    printSection("语法分析");
    Parser parser(std::move(tokens));
    auto compUnit = parser.parse();

    if (!parser.errors().empty()) {
        std::cerr << "语法错误:\n";
        for (auto& err : parser.errors()) {
            std::cerr << "  " << err << "\n";
        }
        return 1;
    }
    std::cout << "语法分析通过 ✓\n";

    // 输出AST（可选）
    if (emitAST) {
        printSection("AST");
        std::cout << compUnit->toString() << "\n";
    }

    // 4. 语义分析
    printSection("语义分析");
    SemanticAnalyzer semAnalyzer;
    bool semOK = semAnalyzer.analyze(compUnit.get());

    if (!semOK || !semAnalyzer.errors().empty()) {
        std::cerr << "语义错误:\n";
        for (auto& err : semAnalyzer.errors()) {
            std::cerr << "  " << err << "\n";
        }
        if (!semOK) return 1;
    }
    std::cout << "语义分析通过 ✓\n";

    // 5. IR代码生成
    printSection("IR代码生成");
    IRGenerator irGen;
    auto module = irGen.generate(compUnit.get());

    if (!irGen.errors().empty()) {
        std::cerr << "IR生成错误:\n";
        for (auto& err : irGen.errors()) {
            std::cerr << "  " << err << "\n";
        }
        return 1;
    }

    // 6. 输出LLVM IR
    std::string irStr;
    llvm::raw_string_ostream irStream(irStr);
    module->print(irStream, nullptr);

    std::ofstream outFile(outputPath);
    if (!outFile.is_open()) {
        std::cerr << "错误: 无法写入输出文件 '" << outputPath << "'\n";
        return 1;
    }
    outFile << irStr;
    outFile.close();

    std::cout << "LLVM IR已生成到: " << outputPath << " ✓\n";
    std::cout << "\n编译完成！可以使用以下命令编译IR:\n";
    std::cout << "  clang " << outputPath << " -o program\n";

    return 0;
}
