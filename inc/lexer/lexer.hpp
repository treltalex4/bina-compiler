#pragma once

#include <expected>
#include <string>
#include <vector>

#include "token.hpp"

namespace Lexer {

class Lexer {
   public:
    Lexer(const std::string& source, const std::string& filename);

    std::expected<std::vector<Token>, std::string> tokenize();

   private:
    std::string m_source;
    std::string m_filename;

    size_t m_pos = 0;
    size_t m_line = 1;
    size_t m_col = 1;

    char peek() const;      // взять текущий символ
    char peekNext() const;  // взять следующий символ
    char advance();         // взять текущий символ и продвинуться дальше
    void skipWhitespace();  // пропустить пробельные символы
    bool isEnd() const;     // проверить на конец файла
};
}  // namespace Lexer