#pragma once

#include <optional>
#include <string>
#include <unordered_map>  // IWYU pragma: keep

enum class TokenType {
    // Литералы
    INT_LIT,     // 42, 255u8
    FLOAT_LIT,   // 3.14, 3.14f64
    STRING_LIT,  // "hello"
    BOOL_TRUE,   // true
    BOOL_FALSE,  // false

    // Идентификатор
    IDENT,  // x, myVar, Point

    // Ключевые слова
    KW_FN,         // fn
    KW_LET,        // let
    KW_MUT,        // mut
    KW_RETURN,     // return
    KW_IF,         // if
    KW_ELSE,       // else
    KW_WHILE,      // while
    KW_BREAK,      // break
    KW_CONTINUE,   // continue
    KW_STRUCT,     // struct
    KW_NAMESPACE,  // namespace
    KW_TYPE,       // type
    KW_CAST,       // cast
    KW_VOID,       // void
    KW_PRINT,      // print
    KW_INPUT,      // input
    KW_EXIT,       // exit
    KW_PANIC,      // panic
    KW_LEN,        // len

    // Операторы
    PLUS,           // +
    MINUS,          // -
    STAR,           // *
    SLASH,          // /
    PERCENT,        // %
    EQUAL,          // ==
    NOT_EQUAL,      // !=
    LESS,           // <
    GREATER,        // >
    LESS_EQUAL,     // <=
    GREATER_EQUAL,  // >=
    AND_AND,        // &&
    OR_OR,          // ||
    NOT,            // !
    ASSIGN,         // =
    ARROW,          // ->
    COLON_COLON,    // ::
    DOT,            // .

    // Разделители
    COMMA,      // ,
    SEMICOLON,  // ;
    COLON,      // :

    // Скобки
    LPAREN,    // (
    RPAREN,    // )
    LBRACE,    // {
    RBRACE,    // }
    LBRACKET,  // [
    RBRACKET,  // ]

    // Конец файла
    END_OF_FILE,  // EOF
};

struct Token {
    TokenType type;
    std::string lexeme;
    size_t line;
    size_t col;
};

std::string tokenTypeToString(TokenType type);

std::optional<TokenType> getKeywordType(const std::string& word);