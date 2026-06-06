module bina.lexer;

import std;
import bina.lexer.token;

namespace {
struct Utf8CodePoint {
    std::uint32_t value;
    std::size_t length;
};

bool isContinuationByte(unsigned char c) { return (c & 0xC0) == 0x80; }

bool isUnicodeScalar(std::uint32_t value) {
    return value <= 0x10FFFF &&
           !(value >= 0xD800 && value <= 0xDFFF);
}

bool isDecimalDigit(char c) { return c >= '0' && c <= '9'; }

bool isBinaryDigit(char c) { return c == '0' || c == '1'; }

bool isAsciiAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool isIdentifierStart(char c) { return isAsciiAlpha(c) || c == '_'; }

bool isIdentifierContinuation(char c) {
    return isIdentifierStart(c) || isDecimalDigit(c);
}

int hexValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool isHexDigit(char c) { return hexValue(c) != -1; }

std::optional<Utf8CodePoint> decodeUtf8(const std::string& source,
                                        std::size_t pos) {
    if (pos >= source.size()) return std::nullopt;

    unsigned char b0 = static_cast<unsigned char>(source[pos]);

    if (b0 <= 0x7F) {
        return Utf8CodePoint{b0, 1};
    }

    if (b0 >= 0xC2 && b0 <= 0xDF) {
        if (pos + 1 >= source.size()) return std::nullopt;
        unsigned char b1 = static_cast<unsigned char>(source[pos + 1]);
        if (!isContinuationByte(b1)) return std::nullopt;

        std::uint32_t value = ((b0 & 0x1F) << 6) | (b1 & 0x3F);

        return Utf8CodePoint{value, 2};
    }

    if (b0 >= 0xE0 && b0 <= 0xEF) {
        if (pos + 2 >= source.size()) return std::nullopt;
        unsigned char b1 = static_cast<unsigned char>(source[pos + 1]);
        unsigned char b2 = static_cast<unsigned char>(source[pos + 2]);
        if (!isContinuationByte(b1) || !isContinuationByte(b2)) {
            return std::nullopt;
        }

        std::uint32_t value =
            ((b0 & 0x0F) << 12) | ((b1 & 0x3F) << 6) | (b2 & 0x3F);

        if (value < 0x800 || !isUnicodeScalar(value)) return std::nullopt;
        return Utf8CodePoint{value, 3};
    }

    if (b0 >= 0xF0 && b0 <= 0xF4) {
        if (pos + 3 >= source.size()) return std::nullopt;
        unsigned char b1 = static_cast<unsigned char>(source[pos + 1]);
        unsigned char b2 = static_cast<unsigned char>(source[pos + 2]);
        unsigned char b3 = static_cast<unsigned char>(source[pos + 3]);
        if (!isContinuationByte(b1) || !isContinuationByte(b2) ||
            !isContinuationByte(b3)) {
            return std::nullopt;
        }

        std::uint32_t value = ((b0 & 0x07) << 18) | ((b1 & 0x3F) << 12) |
                              ((b2 & 0x3F) << 6) | (b3 & 0x3F);

        if (value < 0x10000 || !isUnicodeScalar(value)) return std::nullopt;
        return Utf8CodePoint{value, 4};
    }

    return std::nullopt;
}
}  // namespace

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

        std::size_t startLine = m_line;
        std::size_t startCol = m_col;

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
        else if (isDecimalDigit(c)) {
            auto numericError = [&](const std::string& message) {
                return std::unexpected(m_filename + ":" +
                                       std::to_string(startLine) + ":" +
                                       std::to_string(startCol) +
                                       ": error: " + message);
            };

            auto readIntegerSuffix =
                [&]() -> std::expected<std::string, std::string> {
                std::string suffix;
                if (peek() == 'i' || peek() == 'u') {
                    suffix += advance();
                    while (!isEnd() && isDecimalDigit(peek())) {
                        suffix += advance();
                    }

                    if (suffix != "i8" && suffix != "i16" &&
                        suffix != "i32" && suffix != "i64" &&
                        suffix != "u8" && suffix != "u16" &&
                        suffix != "u32" && suffix != "u64") {
                        return std::unexpected("invalid numeric suffix");
                    }
                }

                return suffix;
            };

            auto readFloatSuffix =
                [&]() -> std::expected<std::string, std::string> {
                std::string suffix;
                if (peek() == 'f') {
                    suffix += advance();
                    while (!isEnd() && isDecimalDigit(peek())) {
                        suffix += advance();
                    }

                    if (suffix != "f32" && suffix != "f64") {
                        return std::unexpected("invalid numeric suffix");
                    }
                }

                return suffix;
            };

            auto ensureNoIdentifierContinuation =
                [&]() -> std::expected<void, std::string> {
                if (!isEnd() && isIdentifierContinuation(peek())) {
                    return std::unexpected("invalid numeric suffix");
                }
                return {};
            };

            std::string num(1, c);

            if (c == '0' && (peek() == 'x' || peek() == 'X')) {
                num += advance();

                std::size_t digits = 0;
                while (!isEnd() && isHexDigit(peek())) {
                    num += advance();
                    ++digits;
                }

                if (digits == 0) {
                    return numericError("invalid hexadecimal literal");
                }

                auto suffix = readIntegerSuffix();
                if (!suffix) return numericError(suffix.error());
                num += *suffix;

                auto no_tail = ensureNoIdentifierContinuation();
                if (!no_tail) return numericError(no_tail.error());

                tokens.push_back({TokenType::INT_LIT, num, startLine,
                                  startCol});
                continue;
            }

            if (c == '0' && (peek() == 'b' || peek() == 'B')) {
                num += advance();

                std::size_t digits = 0;
                while (!isEnd() && isBinaryDigit(peek())) {
                    num += advance();
                    ++digits;
                }

                if (digits == 0) {
                    return numericError("invalid binary literal");
                }

                auto suffix = readIntegerSuffix();
                if (!suffix) return numericError(suffix.error());
                num += *suffix;

                auto no_tail = ensureNoIdentifierContinuation();
                if (!no_tail) return numericError(no_tail.error());

                tokens.push_back({TokenType::INT_LIT, num, startLine,
                                  startCol});
                continue;
            }

            while (!isEnd() && isDecimalDigit(peek())) {
                num += advance();
            }

            bool is_float = false;

            if (peek() == '.' && isDecimalDigit(peekNext())) {
                is_float = true;
                num += advance();
                while (!isEnd() && isDecimalDigit(peek())) {
                    num += advance();
                }
            }

            if (peek() == 'e' || peek() == 'E') {
                is_float = true;
                num += advance();

                if (peek() == '+' || peek() == '-') {
                    num += advance();
                }

                if (!isDecimalDigit(peek())) {
                    return numericError("invalid float exponent");
                }

                while (!isEnd() && isDecimalDigit(peek())) {
                    num += advance();
                }
            }

            if (is_float) {
                auto suffix = readFloatSuffix();
                if (!suffix) return numericError(suffix.error());
                num += *suffix;

                auto no_tail = ensureNoIdentifierContinuation();
                if (!no_tail) return numericError(no_tail.error());

                tokens.push_back(
                    {TokenType::FLOAT_LIT, num, startLine, startCol});
            } else {
                auto suffix = readIntegerSuffix();
                if (!suffix) return numericError(suffix.error());
                num += *suffix;

                auto no_tail = ensureNoIdentifierContinuation();
                if (!no_tail) return numericError(no_tail.error());

                tokens.push_back(
                    {TokenType::INT_LIT, num, startLine, startCol});
            }
        }

        // символьные литералы
        else if (c == '\'') {
            auto charError = [&](const std::string& message) {
                return std::unexpected(
                    m_filename + ":" + std::to_string(startLine) + ":" +
                    std::to_string(startCol) + ": error: " + message);
            };

            if (isEnd()) {
                return charError("unterminated character literal");
            }

            if (peek() == '\'') {
                return charError("empty character literal");
            }

            if (peek() == '\n' || peek() == '\r') {
                return charError("unterminated character literal");
            }

            std::uint32_t codepoint = 0;

            if (peek() == '\\') {
                advance();

                if (isEnd()) {
                    return charError("unterminated character literal");
                }

                char esc = advance();

                switch (esc) {
                    case 'n':
                        codepoint = '\n';
                        break;
                    case 't':
                        codepoint = '\t';
                        break;
                    case 'r':
                        codepoint = '\r';
                        break;
                    case '\'':
                        codepoint = '\'';
                        break;
                    case '"':
                        codepoint = '"';
                        break;
                    case '\\':
                        codepoint = '\\';
                        break;
                    case 'u': {
                        if (peek() != '{') {
                            return charError("expected '{' after '\\u'");
                        }
                        advance();

                        std::uint32_t value = 0;
                        std::size_t digits = 0;

                        while (!isEnd() && peek() != '}') {
                            if (peek() == '\n' || peek() == '\r') {
                                return charError("unterminated Unicode escape");
                            }

                            int digit = hexValue(peek());
                            if (digit == -1) {
                                return charError(
                                    "invalid hex digit in Unicode escape");
                            }

                            if (digits == 6) {
                                return charError("Unicode escape is too large");
                            }

                            value =
                                value * 16 + static_cast<std::uint32_t>(digit);
                            ++digits;
                            advance();
                        }

                        if (digits == 0) {
                            return charError("empty Unicode escape");
                        }

                        if (isEnd()) {
                            return charError("unterminated Unicode escape");
                        }

                        advance();

                        if (!isUnicodeScalar(value)) {
                            return charError("invalid Unicode scalar value");
                        }

                        codepoint = value;
                        break;
                    }
                    default:
                        return charError("unknown escape sequence '\\" +
                                         std::string(1, esc) + "'");
                }
            } else {
                auto decoded = decodeUtf8(m_source, m_pos);
                if (!decoded) {
                    return charError("invalid UTF-8 in character literal");
                }

                codepoint = decoded->value;

                for (std::size_t i = 0; i < decoded->length; ++i) {
                    advance();
                }
            }

            if (!isUnicodeScalar(codepoint)) {
                return charError("invalid Unicode scalar value");
            }

            if (isEnd()) {
                return charError("unterminated character literal");
            }

            if (peek() != '\'') {
                return charError(
                    "character literal must contain exactly one character");
            }

            advance();

            tokens.push_back({TokenType::CHAR_LIT, std::to_string(codepoint),
                              startLine, startCol});
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
        else if (isIdentifierStart(c)) {
            std::string word(1, c);
            while (!isEnd() && isIdentifierContinuation(peek())) {
                word += advance();
            }

            if (word == "inf" || word == "nan" || word == "NaN") {
                tokens.push_back(
                    {TokenType::FLOAT_LIT, word, startLine, startCol});
            } else if (auto keyword = getKeywordType(word)) {
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
