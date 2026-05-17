#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "lexer/lexer.hpp"
#include "lexer/token.hpp"
#include "parser/ast.hpp"
#include "parser/parser.hpp"

int main(int argc, char* argv[]) {
    bool dump_tokens = false;
    bool dump_ast = false;
    std::string filename;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dump-tokens") {
            dump_tokens = true;
        } else if (arg == "--dump-ast") {
            dump_ast = true;
        } else {
            filename = arg;
        }
    }

    if (filename.empty()) {
        std::cerr << "Usage: bina [--dump-tokens] [--dump-ast] <source.bina>\n";
        return 1;
    }

    std::fstream file(filename);
    if (!file.is_open()) {
        std::cerr << "error: cannot open file '" << filename << "'\n";
        return 1;
    }

    std::stringstream buf;
    buf << file.rdbuf();
    std::string source = buf.str();

    Lexer::Lexer lexer(source, filename);
    auto tokens = lexer.tokenize();

    if (!tokens) {
        std::cerr << tokens.error() << '\n';
        return 1;
    }

    if (dump_tokens) {
        for (const auto& tok : tokens.value()) {
            std::cout << tok.line << ":" << tok.col << "  "
                      << tokenTypeToString(tok.type);
            if (!tok.lexeme.empty())
                std::cout << " \"" << tok.lexeme << "\"";
            std::cout << '\n';
        }
    }

    Parser::Parser parser(tokens.value(), filename);
    auto ast = parser.parse();

    if (!ast) {
        std::cerr << ast.error() << '\n';
        return 1;
    }

    if (dump_ast) {
        Parser::printAst(*ast, std::cout);
    }

    return 0;
}
