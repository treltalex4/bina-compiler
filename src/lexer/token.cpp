#include "lexer/token.hpp"

#include <optional>

std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::INT_LIT:
            return "INT_LIT";
        case TokenType::FLOAT_LIT:
            return "FLOAT_LIT";
        case TokenType::STRING_LIT:
            return "STRING_LIT";
        case TokenType::BOOL_TRUE:
            return "BOOL_TRUE";
        case TokenType::BOOL_FALSE:
            return "BOOL_FALSE";
        case TokenType::IDENT:
            return "IDENT";
        case TokenType::KW_FN:
            return "KW_FN";
        case TokenType::KW_LET:
            return "KW_LET";
        case TokenType::KW_MUT:
            return "KW_MUT";
        case TokenType::KW_RETURN:
            return "KW_RETURN";
        case TokenType::KW_IF:
            return "KW_IF";
        case TokenType::KW_ELSE:
            return "KW_ELSE";
        case TokenType::KW_WHILE:
            return "KW_WHILE";
        case TokenType::KW_BREAK:
            return "KW_BREAK";
        case TokenType::KW_CONTINUE:
            return "KW_CONTINUE";
        case TokenType::KW_STRUCT:
            return "KW_STRUCT";
        case TokenType::KW_NAMESPACE:
            return "KW_NAMESPACE";
        case TokenType::KW_TYPE:
            return "KW_TYPE";
        case TokenType::KW_CAST:
            return "KW_CAST";
        case TokenType::KW_VOID:
            return "KW_VOID";
        case TokenType::KW_PRINT:
            return "KW_PRINT";
        case TokenType::KW_INPUT:
            return "KW_INPUT";
        case TokenType::KW_EXIT:
            return "KW_EXIT";
        case TokenType::KW_PANIC:
            return "KW_PANIC";
        case TokenType::KW_LEN:
            return "KW_LEN";
        case TokenType::PLUS:
            return "PLUS";
        case TokenType::MINUS:
            return "MINUS";
        case TokenType::STAR:
            return "STAR";
        case TokenType::SLASH:
            return "SLASH";
        case TokenType::PERCENT:
            return "PERCENT";
        case TokenType::EQUAL:
            return "EQ_EQ";
        case TokenType::NOT_EQUAL:
            return "NOT_EQ";
        case TokenType::LESS:
            return "LESS";
        case TokenType::GREATER:
            return "GREATER";
        case TokenType::LESS_EQUAL:
            return "LESS_EQ";
        case TokenType::GREATER_EQUAL:
            return "GREATER_EQ";
        case TokenType::AND_AND:
            return "AND_AND";
        case TokenType::OR_OR:
            return "OR_OR";
        case TokenType::NOT:
            return "NOT";
        case TokenType::ASSIGN:
            return "ASSIGN";
        case TokenType::ARROW:
            return "ARROW";
        case TokenType::COLON_COLON:
            return "COLON_COLON";
        case TokenType::DOT:
            return "DOT";
        case TokenType::COMMA:
            return "COMMA";
        case TokenType::SEMICOLON:
            return "SEMICOLON";
        case TokenType::COLON:
            return "COLON";
        case TokenType::LPAREN:
            return "LPAREN";
        case TokenType::RPAREN:
            return "RPAREN";
        case TokenType::LBRACE:
            return "LBRACE";
        case TokenType::RBRACE:
            return "RBRACE";
        case TokenType::LBRACKET:
            return "LBRACKET";
        case TokenType::RBRACKET:
            return "RBRACKET";
        case TokenType::END_OF_FILE:
            return "EOF";
    }
    return "UNKNOWN";
}

std::optional<TokenType> getKeywordType(const std::string& word) {
    static const std::unordered_map<std::string, TokenType> keywords = {
        {"fn", TokenType::KW_FN},
        {"let", TokenType::KW_LET},
        {"mut", TokenType::KW_MUT},
        {"return", TokenType::KW_RETURN},
        {"if", TokenType::KW_IF},
        {"else", TokenType::KW_ELSE},
        {"while", TokenType::KW_WHILE},
        {"break", TokenType::KW_BREAK},
        {"continue", TokenType::KW_CONTINUE},
        {"struct", TokenType::KW_STRUCT},
        {"namespace", TokenType::KW_NAMESPACE},
        {"type", TokenType::KW_TYPE},
        {"cast", TokenType::KW_CAST},
        {"void", TokenType::KW_VOID},
        {"print", TokenType::KW_PRINT},
        {"input", TokenType::KW_INPUT},
        {"exit", TokenType::KW_EXIT},
        {"panic", TokenType::KW_PANIC},
        {"len", TokenType::KW_LEN},
        {"true", TokenType::BOOL_TRUE},
        {"false", TokenType::BOOL_FALSE},
    };
    auto it = keywords.find(word);
    if (it != keywords.end()) {
        return it->second;
    }
    return std::nullopt;
}