#include <fstream>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>

#include "lexer/lexer.hpp"
#include "lexer/token.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage bina <source.bina>" << std::endl;
        return 1;
    }

    std::string filename = argv[1];

    std::fstream file(filename);
    if (!file.is_open()) {
        std::cerr << "error: cannot open file '" << filename << "'"
                  << std::endl;
        return 1;
    }

    std::stringstream buf;
    buf << file.rdbuf();
    std::string source = buf.str();

    Lexer::Lexer lexer(source, filename);
    auto result = lexer.tokenize();

    if (!result) {
        std::cerr << result.error() << std::endl;
        return 1;
    }

    for (const auto& token : result.value()) {
        std::cout << token.line << ":" << token.col << "  "
                  << tokenTypeToString(token.type);
        if (!token.lexeme.empty()) {
            std::cout << " \"" << token.lexeme << "\"";
        }
        std::cout << std::endl;
    }
    return 0;
}