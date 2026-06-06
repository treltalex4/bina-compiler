module bina.parser;

import std;

namespace Parser {

Parser::Parser(const std::vector<Token>& tokens, const std::string& filename)
    : m_token(tokens), m_filename(filename) {}

// служебные методы

const Token& Parser::peek() const { return m_token[m_pos]; }

const Token& Parser::peekNext() const {
    if (m_pos + 1 < m_token.size()) return m_token[m_pos + 1];
    return m_token.back();  // EOF
}

const Token& Parser::advance() {
    const Token& t = m_token[m_pos];
    if (!isEnd()) ++m_pos;
    return t;
}

std::expected<Token, std::string> Parser::expect(TokenType type,
                                                 const std::string& message) {
    if (peek().type == type) return advance();
    return std::unexpected(error(message));
}

void Parser::consume(TokenType type, const std::string& message) {
    if (peek().type == type) {
        advance();
        return;
    }
    error(message);
}

bool Parser::isEnd() const {
    return m_token[m_pos].type == TokenType::END_OF_FILE;
}

NodeLocation Parser::makeLoc(const Token& token) const {
    return NodeLocation{token.line, token.col};
}

std::string Parser::error(const std::string& message) {
    const Token& t = peek();
    std::ostringstream oss;
    oss << m_filename << ':' << t.line << ':' << t.col
        << ": error: " << message;
    std::string msg = oss.str();
    m_errors.push_back(msg);
    return msg;
}

// Пропускаем токены до границы синхронизации: ; } или начала stmt/decl
void Parser::synchronize() {
    while (!isEnd()) {
        TokenType t = peek().type;

        if (t == TokenType::SEMICOLON) {
            advance();
            return;
        }
        if (t == TokenType::RBRACE) return;

        switch (t) {
            case TokenType::KW_FN:
            case TokenType::KW_STRUCT:
            case TokenType::KW_NAMESPACE:
            case TokenType::KW_TYPE:
            case TokenType::KW_IMPL:
            case TokenType::KW_LET:
            case TokenType::KW_MUT:
            case TokenType::KW_IF:
            case TokenType::KW_WHILE:
            case TokenType::KW_RETURN:
            case TokenType::KW_BREAK:
            case TokenType::KW_CONTINUE:
                return;
            default:
                advance();
        }
    }
}

// выражения

namespace {
// RAII-гард для временного изменения m_allow_struct_lit
class StructLitGuard {
   public:
    StructLitGuard(bool& flag, bool new_value) : m_flag(flag), m_old(flag) {
        m_flag = new_value;
    }
    ~StructLitGuard() { m_flag = m_old; }

   private:
    bool& m_flag;
    bool m_old;
};

bool isCmpOp(TokenType t) {
    return t == TokenType::EQUAL || t == TokenType::NOT_EQUAL ||
           t == TokenType::LESS || t == TokenType::GREATER ||
           t == TokenType::LESS_EQUAL || t == TokenType::GREATER_EQUAL;
}
}  // namespace

Expr Parser::parseExpression() { return parseOr(); }

Expr Parser::parseOr() {
    Expr left = parseAnd();
    while (peek().type == TokenType::OR_OR) {
        NodeLocation loc = makeLoc(peek());
        TokenType op = advance().type;
        Expr right = parseAnd();
        auto bin = std::make_unique<BinaryExpr>(
            BinaryExpr{op, std::move(left), std::move(right)});
        left = Expr{std::move(bin), loc};
    }
    return left;
}

Expr Parser::parseAnd() {
    Expr left = parseCmp();
    while (peek().type == TokenType::AND_AND) {
        NodeLocation loc = makeLoc(peek());
        TokenType op = advance().type;
        Expr right = parseCmp();
        auto bin = std::make_unique<BinaryExpr>(
            BinaryExpr{op, std::move(left), std::move(right)});
        left = Expr{std::move(bin), loc};
    }
    return left;
}

// cmp_expr = add_expr [ cmp_op add_expr ] — без цепочек
Expr Parser::parseCmp() {
    Expr left = parseAdd();
    if (isCmpOp(peek().type)) {
        NodeLocation loc = makeLoc(peek());
        TokenType op = advance().type;
        Expr right = parseAdd();
        auto bin = std::make_unique<BinaryExpr>(
            BinaryExpr{op, std::move(left), std::move(right)});
        left = Expr{std::move(bin), loc};
        // Запрет цепочек сравнений: a < b < c
        if (isCmpOp(peek().type)) {
            error("chained comparison is not allowed");
        }
    }
    return left;
}

Expr Parser::parseAdd() {
    Expr left = parseMul();
    while (peek().type == TokenType::PLUS || peek().type == TokenType::MINUS) {
        NodeLocation loc = makeLoc(peek());
        TokenType op = advance().type;
        Expr right = parseMul();
        auto bin = std::make_unique<BinaryExpr>(
            BinaryExpr{op, std::move(left), std::move(right)});
        left = Expr{std::move(bin), loc};
    }
    return left;
}

Expr Parser::parseMul() {
    Expr left = parseUnary();
    while (peek().type == TokenType::STAR || peek().type == TokenType::SLASH ||
           peek().type == TokenType::PERCENT) {
        NodeLocation loc = makeLoc(peek());
        TokenType op = advance().type;
        Expr right = parseUnary();
        auto bin = std::make_unique<BinaryExpr>(
            BinaryExpr{op, std::move(left), std::move(right)});
        left = Expr{std::move(bin), loc};
    }
    return left;
}

// unary_expr = ("-" | "!") unary_expr | postfix_expr  — правая ассоциативность
Expr Parser::parseUnary() {
    if (peek().type == TokenType::MINUS || peek().type == TokenType::NOT) {
        NodeLocation loc = makeLoc(peek());
        TokenType op = advance().type;
        Expr operand = parseUnary();
        auto un =
            std::make_unique<UnaryExpr>(UnaryExpr{op, std::move(operand)});
        return Expr{std::move(un), loc};
    }
    return parsePostfix();
}

// postfix_expr = primary_expr {
//     "[" expr "]" | "." IDENT | "." IDENT "(" [args] ")"
// }
Expr Parser::parsePostfix() {
    Expr expr = parsePrimary();
    while (true) {
        if (peek().type == TokenType::LBRACKET) {
            NodeLocation loc = makeLoc(peek());
            advance();
            Expr index;
            {
                StructLitGuard guard(m_allow_struct_lit, true);
                index = parseExpression();
            }
            consume(TokenType::RBRACKET, "expected ']' after index expression");
            auto idx = std::make_unique<IndexExpr>(
                IndexExpr{std::move(expr), std::move(index)});
            expr = Expr{std::move(idx), loc};
        } else if (peek().type == TokenType::DOT) {
            NodeLocation loc = makeLoc(peek());
            advance();
            auto name_tok =
                expect(TokenType::IDENT, "expected field name after '.'");
            std::string field = name_tok ? name_tok->lexeme : "";

            if (peek().type == TokenType::LPAREN) {
                advance();
                std::vector<Expr> args;
                if (peek().type != TokenType::RPAREN) {
                    StructLitGuard guard(m_allow_struct_lit, true);
                    args.push_back(parseExpression());
                    while (peek().type == TokenType::COMMA) {
                        advance();
                        args.push_back(parseExpression());
                    }
                }
                consume(TokenType::RPAREN,
                        "expected ')' after method arguments");
                auto call = std::make_unique<MethodCall>(MethodCall{
                    std::move(expr), std::move(field), std::move(args)});
                expr = Expr{std::move(call), loc};
            } else {
                auto fa = std::make_unique<FieldAccess>(
                    FieldAccess{std::move(expr), std::move(field)});
                expr = Expr{std::move(fa), loc};
            }
        } else {
            break;
        }
    }
    return expr;
}

Expr Parser::parsePrimary() {
    const Token& t = peek();
    NodeLocation loc = makeLoc(t);

    switch (t.type) {
        case TokenType::INT_LIT: {
            std::string value = advance().lexeme;
            return Expr{IntLiteral{std::move(value)}, loc};
        }
        case TokenType::FLOAT_LIT: {
            std::string value = advance().lexeme;
            return Expr{FloatLiteral{std::move(value)}, loc};
        }
        case TokenType::CHAR_LIT: {
            std::uint32_t value =
                static_cast<std::uint32_t>(std::stoul(advance().lexeme));
            return Expr{CharLiteral{value}, loc};
        }
        case TokenType::STRING_LIT: {
            std::string value = advance().lexeme;
            return Expr{StringLiteral{std::move(value)}, loc};
        }
        case TokenType::BOOL_TRUE:
            advance();
            return Expr{BoolLiteral{true}, loc};
        case TokenType::BOOL_FALSE:
            advance();
            return Expr{BoolLiteral{false}, loc};
        case TokenType::LPAREN: {
            advance();
            StructLitGuard guard(m_allow_struct_lit, true);
            Expr inner = parseExpression();
            consume(TokenType::RPAREN, "expected ')' after expression");
            return inner;
        }
        case TokenType::LBRACKET:
            return parseArrayLiteral();
        case TokenType::KW_CAST:
            return parseCastExpr();
        case TokenType::IDENT:
        case TokenType::KW_PRINT:
        case TokenType::KW_INPUT:
        case TokenType::KW_EXIT:
        case TokenType::KW_PANIC:
        case TokenType::KW_ASSERT:
        case TokenType::KW_LEN: {
            Identifier name = parseQualifiedIdent();

            // вызов функции: qualified_name '(' [args] ')'
            if (peek().type == TokenType::LPAREN) {
                advance();
                std::vector<Expr> args;
                if (peek().type != TokenType::RPAREN) {
                    StructLitGuard guard(m_allow_struct_lit, true);
                    args.push_back(parseExpression());
                    while (peek().type == TokenType::COMMA) {
                        advance();
                        args.push_back(parseExpression());
                    }
                }
                consume(TokenType::RPAREN, "expected ')' after arguments");
                auto call = std::make_unique<CallExpr>(
                    CallExpr{std::move(name), std::move(args)});
                return Expr{std::move(call), loc};
            }

            // литерал структуры: разрешён, если флаг позволяет и следом '{' ...
            if (m_allow_struct_lit && peek().type == TokenType::LBRACE) {
                TokenType t1 = (m_pos + 1 < m_token.size())
                                   ? m_token[m_pos + 1].type
                                   : TokenType::END_OF_FILE;
                TokenType t2 = (m_pos + 2 < m_token.size())
                                   ? m_token[m_pos + 2].type
                                   : TokenType::END_OF_FILE;
                // '{}' (пустой литерал) или '{ IDENT :' (первое поле)
                if (t1 == TokenType::RBRACE ||
                    (t1 == TokenType::IDENT && t2 == TokenType::COLON)) {
                    return parseStructLiteralRest(std::move(name), loc);
                }
            }

            // просто идентификатор
            return Expr{std::move(name), loc};
        }
        default: {
            error("expected expression");
            if (!isEnd()) advance();            // не зацикливаться
            return Expr{IntLiteral{"0"}, loc};  // заглушка
        }
    }
}

Identifier Parser::parseQualifiedIdent() {
    Identifier id;
    const Token& first = peek();
    switch (first.type) {
        case TokenType::IDENT:
        case TokenType::KW_PRINT:
        case TokenType::KW_INPUT:
        case TokenType::KW_EXIT:
        case TokenType::KW_PANIC:
        case TokenType::KW_ASSERT:
        case TokenType::KW_LEN:
            id.parts.push_back(advance().lexeme);
            break;
        default:
            error("expected identifier");
            return id;
    }
    while (peek().type == TokenType::COLON_COLON) {
        advance();
        auto next = expect(TokenType::IDENT, "expected identifier after '::'");
        if (next) id.parts.push_back(next->lexeme);
    }
    return id;
}

Expr Parser::parseArrayLiteral() {
    NodeLocation loc = makeLoc(peek());
    consume(TokenType::LBRACKET, "expected '['");
    std::vector<Expr> elements;
    StructLitGuard guard(m_allow_struct_lit, true);
    if (peek().type != TokenType::RBRACKET) {
        elements.push_back(parseExpression());
        while (peek().type == TokenType::COMMA) {
            advance();
            if (peek().type == TokenType::RBRACKET) break;  // trailing comma
            elements.push_back(parseExpression());
        }
    }
    consume(TokenType::RBRACKET, "expected ']' after array elements");
    auto arr =
        std::make_unique<ArrayLiteral>(ArrayLiteral{std::move(elements)});
    return Expr{std::move(arr), loc};
}

Expr Parser::parseCastExpr() {
    NodeLocation loc = makeLoc(peek());
    consume(TokenType::KW_CAST, "expected 'cast'");
    consume(TokenType::LESS, "expected '<' after 'cast'");
    TypeExpr target = parseType();
    consume(TokenType::GREATER, "expected '>' after cast type");
    consume(TokenType::LPAREN, "expected '(' after cast<type>");
    Expr value;
    {
        StructLitGuard guard(m_allow_struct_lit, true);
        value = parseExpression();
    }
    consume(TokenType::RPAREN, "expected ')' after cast expression");
    auto cast = std::make_unique<CastExpr>(
        CastExpr{std::move(target), std::move(value)});
    return Expr{std::move(cast), loc};
}

Expr Parser::parseStructLiteralRest(Identifier name, NodeLocation loc) {
    consume(TokenType::LBRACE, "expected '{'");
    std::vector<FieldInit> fields;
    if (peek().type != TokenType::RBRACE) {
        fields.push_back(parseFieldInit());
        while (peek().type == TokenType::COMMA) {
            advance();
            if (peek().type == TokenType::RBRACE) break;  // trailing comma
            fields.push_back(parseFieldInit());
        }
    }
    consume(TokenType::RBRACE, "expected '}' after struct fields");
    auto lit = std::make_unique<StructLiteral>(
        StructLiteral{std::move(name), std::move(fields)});
    return Expr{std::move(lit), loc};
}

FieldInit Parser::parseFieldInit() {
    NodeLocation loc = makeLoc(peek());
    auto name_tok = expect(TokenType::IDENT, "expected field name");
    std::string name = name_tok ? name_tok->lexeme : "";
    consume(TokenType::COLON, "expected ':' after field name");
    StructLitGuard guard(m_allow_struct_lit, true);
    Expr value = parseExpression();
    return FieldInit{std::move(name), std::move(value), loc};
}

// типы

TypeExpr Parser::parseType() {
    const Token& t = peek();
    NodeLocation loc = makeLoc(t);

    if (t.type == TokenType::KW_VOID) {
        advance();
        return TypeExpr{SimpleType{"void"}, loc};
    }
    if (t.type == TokenType::LBRACKET) {
        return parseArrayType();
    }
    if (t.type == TokenType::IDENT) {
        return parseQualifiedName();
    }
    error("expected type");
    if (!isEnd()) advance();
    return TypeExpr{SimpleType{"void"}, loc};  // заглушка
}

TypeExpr Parser::parseArrayType() {
    NodeLocation loc = makeLoc(peek());
    consume(TokenType::LBRACKET, "expected '['");
    auto size_tok = expect(TokenType::INT_LIT, "expected array size literal");
    std::string size = size_tok ? size_tok->lexeme : "0";
    consume(TokenType::RBRACKET, "expected ']' after array size");
    TypeExpr element = parseType();
    auto arr = std::make_unique<ArrayType>(
        ArrayType{std::move(size), std::move(element)});
    return TypeExpr{std::move(arr), loc};
}

TypeExpr Parser::parseQualifiedName() {
    NodeLocation loc = makeLoc(peek());
    std::vector<std::string> parts;
    auto first = expect(TokenType::IDENT, "expected type name");
    if (first) parts.push_back(first->lexeme);
    while (peek().type == TokenType::COLON_COLON) {
        advance();
        auto next = expect(TokenType::IDENT, "expected identifier after '::'");
        if (next) parts.push_back(next->lexeme);
    }
    if (parts.size() == 1) {
        return TypeExpr{SimpleType{std::move(parts[0])}, loc};
    }
    return TypeExpr{QualifiedType{std::move(parts)}, loc};
}

// инструкции

Block Parser::parseBlock() {
    Block block;
    consume(TokenType::LBRACE, "expected '{'");
    while (!isEnd() && peek().type != TokenType::RBRACE) {
        std::size_t before = m_errors.size();
        Stmt s = parseStatement();
        if (m_errors.size() > before) synchronize();
        block.statements.push_back(std::move(s));
    }
    consume(TokenType::RBRACE, "expected '}'");
    return block;
}

Stmt Parser::parseStatement() {
    const Token& t = peek();
    NodeLocation loc = makeLoc(t);
    switch (t.type) {
        case TokenType::LBRACE: {
            auto block = std::make_unique<Block>(parseBlock());
            return Stmt{std::move(block), loc};
        }
        case TokenType::KW_LET:
        case TokenType::KW_MUT:
            return parseLetStmt();
        case TokenType::KW_IF:
            return parseIfStmt();
        case TokenType::KW_WHILE:
            return parseWhileStmt();
        case TokenType::KW_RETURN:
            return parseReturnStmt();
        case TokenType::KW_BREAK:
            advance();
            consume(TokenType::SEMICOLON, "expected ';' after 'break'");
            return Stmt{BreakStmt{}, loc};
        case TokenType::KW_CONTINUE:
            advance();
            consume(TokenType::SEMICOLON, "expected ';' after 'continue'");
            return Stmt{ContinueStmt{}, loc};
        case TokenType::SEMICOLON:
            advance();
            return Stmt{NullStmt{}, loc};
        default:
            return parseAssignOrExprStmt();
    }
}

Stmt Parser::parseLetStmt() {
    NodeLocation loc = makeLoc(peek());
    bool is_mutable = (peek().type == TokenType::KW_MUT);
    advance();  // let | mut
    auto name_tok = expect(TokenType::IDENT, "expected variable name");
    std::string name = name_tok ? name_tok->lexeme : "";
    std::optional<TypeExpr> type;
    if (peek().type == TokenType::COLON) {
        advance();
        type = parseType();
    }
    consume(TokenType::ASSIGN, "expected '=' in let statement");
    Expr init = parseExpression();
    consume(TokenType::SEMICOLON, "expected ';' after let statement");
    auto let = std::make_unique<LetStmt>(
        LetStmt{is_mutable, std::move(name), std::move(type), std::move(init)});
    return Stmt{std::move(let), loc};
}

Stmt Parser::parseIfStmt() {
    NodeLocation loc = makeLoc(peek());
    consume(TokenType::KW_IF, "expected 'if'");
    Expr cond;
    {
        StructLitGuard guard(m_allow_struct_lit, false);
        cond = parseExpression();
    }
    Block then_block = parseBlock();
    std::optional<std::unique_ptr<Stmt>> else_branch;
    if (peek().type == TokenType::KW_ELSE) {
        advance();
        if (peek().type == TokenType::KW_IF) {
            Stmt inner = parseIfStmt();
            else_branch = std::make_unique<Stmt>(std::move(inner));
        } else {
            NodeLocation bloc = makeLoc(peek());
            auto blk = std::make_unique<Block>(parseBlock());
            Stmt s{std::move(blk), bloc};
            else_branch = std::make_unique<Stmt>(std::move(s));
        }
    }
    auto stmt = std::make_unique<IfStmt>(
        IfStmt{std::move(cond), std::move(then_block), std::move(else_branch)});
    return Stmt{std::move(stmt), loc};
}

Stmt Parser::parseWhileStmt() {
    NodeLocation loc = makeLoc(peek());
    consume(TokenType::KW_WHILE, "expected 'while'");
    Expr cond;
    {
        StructLitGuard guard(m_allow_struct_lit, false);
        cond = parseExpression();
    }
    Block body = parseBlock();
    auto stmt = std::make_unique<WhileStmt>(
        WhileStmt{std::move(cond), std::move(body)});
    return Stmt{std::move(stmt), loc};
}

Stmt Parser::parseReturnStmt() {
    NodeLocation loc = makeLoc(peek());
    consume(TokenType::KW_RETURN, "expected 'return'");
    std::optional<Expr> value;
    if (peek().type != TokenType::SEMICOLON) {
        value = parseExpression();
    }
    consume(TokenType::SEMICOLON, "expected ';' after return");
    auto ret = std::make_unique<ReturnStmt>(ReturnStmt{std::move(value)});
    return Stmt{std::move(ret), loc};
}

Stmt Parser::parseAssignOrExprStmt() {
    NodeLocation loc = makeLoc(peek());
    Expr expr = parseExpression();
    if (peek().type == TokenType::ASSIGN) {
        advance();
        Expr value = parseExpression();
        consume(TokenType::SEMICOLON, "expected ';' after assignment");
        auto assign = std::make_unique<AssignStmt>(
            AssignStmt{std::move(expr), std::move(value)});
        return Stmt{std::move(assign), loc};
    }
    consume(TokenType::SEMICOLON, "expected ';' after expression");
    auto es = std::make_unique<ExprStmt>(ExprStmt{std::move(expr)});
    return Stmt{std::move(es), loc};
}

// объявления

Decl Parser::parseDeclaration() {
    const Token& t = peek();
    NodeLocation loc = makeLoc(t);
    switch (t.type) {
        case TokenType::KW_FN:
            return parseFunctionDecl();
        case TokenType::KW_STRUCT:
            return parseStructDecl();
        case TokenType::KW_NAMESPACE:
            return parseNamespaceDecl();
        case TokenType::KW_TYPE:
            return parseTypeAliasDecl();
        case TokenType::KW_IMPL:
            return parseImplDecl();
        default: {
            error("expected top-level declaration");
            if (!isEnd()) advance();  // защита от цикла
            synchronize();
            auto fn = std::make_unique<FunctionDecl>();  // заглушка
            return Decl{std::move(fn), loc};
        }
    }
}

Param Parser::parseParam() {
    NodeLocation loc = makeLoc(peek());
    auto name_tok = expect(TokenType::IDENT, "expected parameter name");
    std::string name = name_tok ? name_tok->lexeme : "";
    consume(TokenType::COLON, "expected ':' after parameter name");
    TypeExpr type = parseType();
    return Param{std::move(name), std::move(type), loc};
}

StructField Parser::parseStructField() {
    NodeLocation loc = makeLoc(peek());
    auto name_tok = expect(TokenType::IDENT, "expected field name");
    std::string name = name_tok ? name_tok->lexeme : "";
    consume(TokenType::COLON, "expected ':' after field name");
    TypeExpr type = parseType();
    consume(TokenType::SEMICOLON, "expected ';' after struct field");
    return StructField{std::move(name), std::move(type), loc};
}

Decl Parser::parseFunctionDecl() {
    NodeLocation loc = makeLoc(peek());
    consume(TokenType::KW_FN, "expected 'fn'");
    auto name_tok = expect(TokenType::IDENT, "expected function name");
    std::string name = name_tok ? name_tok->lexeme : "";
    consume(TokenType::LPAREN, "expected '(' after function name");
    std::vector<Param> params;
    if (peek().type != TokenType::RPAREN) {
        params.push_back(parseParam());
        while (peek().type == TokenType::COMMA) {
            advance();
            params.push_back(parseParam());
        }
    }
    consume(TokenType::RPAREN, "expected ')' after parameters");
    consume(TokenType::ARROW, "expected '->' after parameters");
    TypeExpr ret_type = parseType();
    Block body = parseBlock();
    auto fn = std::make_unique<FunctionDecl>(
        FunctionDecl{std::move(name), std::move(params), std::move(ret_type),
                     std::move(body)});
    return Decl{std::move(fn), loc};
}

Decl Parser::parseStructDecl() {
    NodeLocation loc = makeLoc(peek());
    consume(TokenType::KW_STRUCT, "expected 'struct'");
    auto name_tok = expect(TokenType::IDENT, "expected struct name");
    std::string name = name_tok ? name_tok->lexeme : "";
    consume(TokenType::LBRACE, "expected '{' in struct");
    std::vector<StructField> fields;
    while (!isEnd() && peek().type != TokenType::RBRACE) {
        std::size_t before = m_errors.size();
        fields.push_back(parseStructField());
        if (m_errors.size() > before) synchronize();
    }
    consume(TokenType::RBRACE, "expected '}' in struct");
    auto s = std::make_unique<StructDecl>(
        StructDecl{std::move(name), std::move(fields)});
    return Decl{std::move(s), loc};
}

Decl Parser::parseNamespaceDecl() {
    NodeLocation loc = makeLoc(peek());
    consume(TokenType::KW_NAMESPACE, "expected 'namespace'");
    auto name_tok = expect(TokenType::IDENT, "expected namespace name");
    std::string name = name_tok ? name_tok->lexeme : "";
    consume(TokenType::LBRACE, "expected '{' in namespace");
    std::vector<Decl> decls;
    while (!isEnd() && peek().type != TokenType::RBRACE) {
        std::size_t before = m_errors.size();
        Decl d = parseDeclaration();
        if (m_errors.size() > before) synchronize();
        decls.push_back(std::move(d));
    }
    consume(TokenType::RBRACE, "expected '}' in namespace");
    auto ns = std::make_unique<NamespaceDecl>(
        NamespaceDecl{std::move(name), std::move(decls)});
    return Decl{std::move(ns), loc};
}

Decl Parser::parseTypeAliasDecl() {
    NodeLocation loc = makeLoc(peek());
    consume(TokenType::KW_TYPE, "expected 'type'");
    auto name_tok = expect(TokenType::IDENT, "expected type alias name");
    std::string name = name_tok ? name_tok->lexeme : "";
    consume(TokenType::ASSIGN, "expected '=' in type alias");
    TypeExpr type = parseType();
    consume(TokenType::SEMICOLON, "expected ';' after type alias");
    auto a = std::make_unique<TypeAliasDecl>(
        TypeAliasDecl{std::move(name), std::move(type)});
    return Decl{std::move(a), loc};
}

Decl Parser::parseImplDecl() {
    NodeLocation loc = makeLoc(peek());
    consume(TokenType::KW_IMPL, "expected 'impl'");
    auto name_tok = expect(TokenType::IDENT, "expected struct name");
    std::string name = name_tok ? name_tok->lexeme : "";
    consume(TokenType::LBRACE, "expected '{' in impl");
    std::vector<std::unique_ptr<FunctionDecl>> methods;
    while (!isEnd() && peek().type != TokenType::RBRACE) {
        if (peek().type != TokenType::KW_FN) {
            error("expected 'fn' in impl block");
            if (!isEnd()) advance();
            synchronize();
            continue;
        }
        Decl d = parseFunctionDecl();
        if (auto* pfn = std::get_if<std::unique_ptr<FunctionDecl>>(&d.node)) {
            methods.push_back(std::move(*pfn));
        }
    }
    consume(TokenType::RBRACE, "expected '}' in impl");
    auto impl = std::make_unique<ImplDecl>(
        ImplDecl{std::move(name), std::move(methods)});
    return Decl{std::move(impl), loc};
}

// точка входа

std::expected<Program, std::vector<std::string>> Parser::parse() {
    Program program;
    while (!isEnd()) {
        program.declarations.push_back(parseDeclaration());
    }
    if (!m_errors.empty()) {
        return std::unexpected(m_errors);
    }
    return program;
}

}  // namespace Parser
