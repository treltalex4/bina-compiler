#include "lexer/lexer.hpp"

#include <cstddef>
#include <expected>
#include <string>
#include <vector>

#include "lexer/token.hpp"

namespace Lexer {
Lexer::Lexer(const std::string& source, const std::string& filename)
    : m_source(source), m_filename(filename) {}

char Lexer::peek() const {
    if (isEnd()) return '\0';
    return m_source[m_pos];
}

char Lexer::peekNext() const {
    if (m_pos + 1 >= m_source.size()) return '\0';
    return m_source[m_pos + 1];
}

char Lexer::advance() {
    char c = m_source[m_pos];
    m_pos++;

    if (c == '\n') {
        m_line++;
        m_col = 1;
    } else {
        m_col++;
    }

    return c;
}

void Lexer::skipWhitespace() {
    while (!isEnd()) {
        char c = peek();

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peekNext() == '/') {
            while (!isEnd() && peek() != '\n') {
                advance();
            }
        } else {
            break;
        }
    }
}

bool Lexer::isEnd() const { return m_pos >= m_source.size(); }

std::expected<std::vector<Token>, std::string> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!isEnd()) {
        skipWhitespace();

        if (isEnd()) break;

        size_t startLine = m_line;
        size_t startCol = m_col;

        char c = advance();

        // односимвольные операторы и разделители
        if (c == '+') {
            tokens.push_back({TokenType::PLUS, "+", startLine, startCol});
        } else if (c == '*') {
            tokens.push_back({TokenType::STAR, "*", startLine, startCol});
        } else if (c == '/') {
            tokens.push_back({TokenType::SLASH, "/", startLine, startCol});
        } else if (c == '%') {
            tokens.push_back({TokenType::PERCENT, "%", startLine, startCol});
        } else if (c == '(') {
            tokens.push_back({TokenType::LPAREN, "(", startLine, startCol});
        } else if (c == ')') {
            tokens.push_back({TokenType::RPAREN, ")", startLine, startCol});
        } else if (c == '{') {
            tokens.push_back({TokenType::LBRACE, "{", startLine, startCol});
        } else if (c == '}') {
            tokens.push_back({TokenType::RBRACE, "}", startLine, startCol});
        } else if (c == '[') {
            tokens.push_back({TokenType::LBRACKET, "[", startLine, startCol});
        } else if (c == ']') {
            tokens.push_back({TokenType::RBRACKET, "]", startLine, startCol});
        } else if (c == ',') {
            tokens.push_back({TokenType::COMMA, ",", startLine, startCol});
        } else if (c == ';') {
            tokens.push_back({TokenType::SEMICOLON, ";", startLine, startCol});
        } else if (c == '.') {
            tokens.push_back({TokenType::DOT, ".", startLine, startCol});
        }
        // двухсимвольные операторы
        else if (c == '-') {
            if (peek() == '>') {
                advance();
                tokens.push_back({TokenType::ARROW, "->", startLine, startCol});
            } else {
                tokens.push_back({TokenType::MINUS, "-", startLine, startCol});
            }
        } else if (c == '=') {
            if (peek() == '=') {
                advance();
                tokens.push_back({TokenType::EQUAL, "==", startLine, startCol});
            } else {
                tokens.push_back({TokenType::ASSIGN, "=", startLine, startCol});
            }
        } else if (c == '!') {
            if (peek() == '=') {
                advance();
                tokens.push_back(
                    {TokenType::NOT_EQUAL, "!=", startLine, startCol});
            } else {
                tokens.push_back({TokenType::NOT, "!", startLine, startCol});
            }
        } else if (c == '<') {
            if (peek() == '=') {
                advance();
                tokens.push_back(
                    {TokenType::LESS_EQUAL, "<=", startLine, startCol});
            } else {
                tokens.push_back({TokenType::LESS, "<", startLine, startCol});
            }
        } else if (c == '>') {
            if (peek() == '=') {
                advance();
                tokens.push_back(
                    {TokenType::GREATER_EQUAL, ">=", startLine, startCol});
            } else {
                tokens.push_back(
                    {TokenType::GREATER, ">", startLine, startCol});
            }
        } else if (c == '&') {
            if (peek() == '&') {
                advance();
                tokens.push_back(
                    {TokenType::AND_AND, "&&", startLine, startCol});
            } else {
                return std::unexpected(m_filename + ":" +
                                       std::to_string(startLine) + ":" +
                                       std::to_string(startCol) +
                                       ": error: unexpected character '&'");
            }
        } else if (c == '|') {
            if (peek() == '|') {
                advance();
                tokens.push_back({TokenType::OR_OR, "||", startLine, startCol});
            } else {
                return std::unexpected(m_filename + ":" +
                                       std::to_string(startLine) + ":" +
                                       std::to_string(startCol) +
                                       ": error: unexpected character '|'");
            }
        } else if (c == ':') {
            if (peek() == ':') {
                advance();
                tokens.push_back(
                    {TokenType::COLON_COLON, "::", startLine, startCol});
            } else {
                tokens.push_back({TokenType::COLON, ":", startLine, startCol});
            }
        }
        // числа
        else if (std::isdigit(c)) {
            std::string num(1, c);
            while (!isEnd() && std::isdigit(peek())) {
                num += advance();
            }
            if (peek() == '.' && std::isdigit(peekNext())) {
                num += advance();
                while (!isEnd() && std::isdigit(peek())) {
                    num += advance();
                }
                if (peek() == 'f' && (peekNext() == '3' || peekNext() == '6')) {
                    num += advance();
                    num += advance();
                    if (!isEnd() && std::isdigit(peek())) {
                        num += advance();
                    }
                }
                tokens.push_back(
                    {TokenType::FLOAT_LIT, num, startLine, startCol});
            } else {
                if (peek() == 'i' || peek() == 'u') {
                    num += advance();
                    while (!isEnd() && std::isdigit(peek())) {
                        num += advance();
                    }
                }
                tokens.push_back(
                    {TokenType::INT_LIT, num, startLine, startCol});
            }
        }
        // строковые литералы
        else if (c == '"') {
            std::string str;
            while (!isEnd() && peek() != '"') {
                if (peek() == '\\') {
                    advance();
                    if (isEnd()) {
                        return std::unexpected(m_filename + ":" +
                                               std::to_string(startLine) + ":" +
                                               std::to_string(startCol) +
                                               ": error: unterminated string");
                    }
                    char esc = advance();
                    switch (esc) {
                        case 'n':
                            str += '\n';
                            break;
                        case 't':
                            str += '\t';
                            break;
                        case 'r':
                            str += '\r';
                            break;
                        case '"':
                            str += '"';
                            break;
                        case '\\':
                            str += '\\';
                            break;
                        default:
                            return std::unexpected(
                                m_filename + ":" + std::to_string(m_line) +
                                ":" + std::to_string(m_col) +
                                ": error: unknown escape sequence '\\" +
                                std::string(1, esc) + "'");
                    }
                } else {
                    str += advance();
                }
            }
            if (isEnd()) {
                return std::unexpected(
                    m_filename + ":" + std::to_string(startLine) + ":" +
                    std::to_string(startCol) + ": error: unterminated string");
            }
            advance();
            tokens.push_back({TokenType::STRING_LIT, str, startLine, startCol});
        }
        // идентификаторы и ключевые слова
        else if (std::isalpha(c) || c == '_') {
            std::string word(1, c);
            while (!isEnd() && (std::isalnum(peek()) || peek() == '_')) {
                word += advance();
            }
            auto keyword = getKeywordType(word);
            if (keyword) {
                tokens.push_back({keyword.value(), word, startLine, startCol});
            } else {
                tokens.push_back({TokenType::IDENT, word, startLine, startCol});
            }
        }
        // неизвестные символы
        else {
            return std::unexpected(
                m_filename + ":" + std::to_string(startLine) + ":" +
                std::to_string(startCol) + ": error: unexpected character '" +
                std::string(1, c) + "'");
        }
    }
    tokens.push_back({TokenType::END_OF_FILE, "", m_line, m_col});

    return tokens;
}
}  // namespace Lexer