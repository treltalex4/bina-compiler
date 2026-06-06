export module bina.lexer.token;

import std;

export enum class TokenType {
    // Литералы
    INT_LIT,
    FLOAT_LIT,
    CHAR_LIT,
    STRING_LIT,
    BOOL_TRUE,
    BOOL_FALSE,

    // Идентификатор
    IDENT,

    // Ключевые слова
    KW_FN,
    KW_LET,
    KW_MUT,
    KW_RETURN,
    KW_IF,
    KW_ELSE,
    KW_WHILE,
    KW_BREAK,
    KW_CONTINUE,
    KW_STRUCT,
    KW_NAMESPACE,
    KW_IMPL,
    KW_TYPE,
    KW_CAST,
    KW_VOID,
    KW_PRINT,
    KW_INPUT,
    KW_EXIT,
    KW_PANIC,
    KW_ASSERT,
    KW_LEN,

    // Операторы
    PLUS,
    MINUS,
    STAR,
    SLASH,
    PERCENT,
    EQUAL,
    NOT_EQUAL,
    LESS,
    GREATER,
    LESS_EQUAL,
    GREATER_EQUAL,
    AND_AND,
    OR_OR,
    NOT,
    ASSIGN,
    ARROW,
    COLON_COLON,
    DOT,

    // Разделители
    COMMA,
    SEMICOLON,
    COLON,

    // Скобки
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,

    // Конец файла
    END_OF_FILE,
};

export struct Token {
    TokenType type;
    std::string lexeme;
    std::size_t line;
    std::size_t col;
};

export std::string tokenTypeToString(TokenType type);
export std::optional<TokenType> getKeywordType(const std::string& word);
