#pragma once

#include <expected>
#include <string>
#include <vector>

#include "lexer/token.hpp"
#include "parser/ast.hpp"

namespace Parser {
class Parser {
   public:
    Parser(const std::vector<Token>& tokens, const std::string& filename);

    std::expected<Program, std::string> parse();

   private:
    std::vector<Token> m_token;
    std::string m_filename;
    size_t m_pos = 0;

    const Token& peek() const;
    const Token& peekNext() const;
    const Token& advance();

    bool match(TokenType type);

    std::expected<Token, std::string> expect(TokenType type,
                                             const std::string& message);

    bool isEnd() const;

    std::string error(const std::string& message) const;

    // парсинг объявлений врехнего уровня
    Decl parseDeclaration();
    Decl parseFunctionDecl();
    Decl parseStructDecl();
    Decl parseNamespaceDecl();
    Decl parseTypaAliasDecl();

    Param parseParam();

    // парсинг инструкций
    Stmt parseStatement();
    Stmt parseLetStmt();
    Stmt parseIfStmt();
    Stmt parseWhileStmt();
    Stmt parseReturnStmt();
    Block parseBlock();

    // парсинг выражений
    Expr parseExpression();
    Expr parseOr();
    Expr parseAnd();
    Expr parseCmp();
    Expr parseAdd();
    Expr parseMul();
    Expr parseUnary();
    Expr parsePostfix();
    Expr parsePrimary();

    // парсинг типов
    TypeExpr parseType();
    TypeExpr parseQualifiedName();
};

struct Test {
    int lex = 2342;
};

};  // namespace Parser