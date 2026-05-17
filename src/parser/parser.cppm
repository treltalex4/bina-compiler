export module bina.parser;

import std;
import bina.lexer.token;
import bina.parser.ast;

export namespace Parser {
class Parser {
   public:
    Parser(const std::vector<Token>& tokens, const std::string& filename);

    std::expected<Program, std::vector<std::string>> parse();

   private:
    std::vector<Token> m_token;
    std::string m_filename;
    std::size_t m_pos = 0;

    std::vector<std::string> m_errors;
    bool m_allow_struct_lit = true;

    const Token& peek() const;
    const Token& peekNext() const;
    const Token& advance();

    std::expected<Token, std::string> expect(TokenType type,
                                             const std::string& message);

    void consume(TokenType type, const std::string& message);

    bool isEnd() const;

    NodeLocation makeLoc(const Token& token) const;

    std::string error(const std::string& message);
    void synchronize();

    // парсинг объявлений верхнего уровня
    Decl parseDeclaration();
    Decl parseFunctionDecl();
    Decl parseStructDecl();
    Decl parseNamespaceDecl();
    Decl parseTypeAliasDecl();
    Decl parseImplDecl();

    Param parseParam();
    StructField parseStructField();

    // парсинг инструкций
    Stmt parseStatement();
    Stmt parseLetStmt();
    Stmt parseIfStmt();
    Stmt parseWhileStmt();
    Stmt parseReturnStmt();
    Stmt parseAssignOrExprStmt();
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

    Identifier parseQualifiedIdent();
    Expr parseArrayLiteral();
    Expr parseCastExpr();
    Expr parseStructLiteralRest(Identifier name, NodeLocation loc);
    FieldInit parseFieldInit();

    // парсинг типов
    TypeExpr parseType();
    TypeExpr parseArrayType();
    TypeExpr parseQualifiedName();
};

}  // namespace Parser
