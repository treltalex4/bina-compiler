export module bina.parser.ast;

import std;
import bina.lexer.token;

export namespace Parser {
struct NodeLocation {
    std::size_t line = 0;
    std::size_t col = 0;
};

// forward declaration
// выражения
struct IntLiteral;
struct FloatLiteral;
struct CharLiteral;
struct StringLiteral;
struct BoolLiteral;
struct Identifier;
struct BinaryExpr;
struct UnaryExpr;
struct CallExpr;
struct IndexExpr;
struct FieldAccess;
struct MethodCall;
struct CastExpr;
struct ArrayLiteral;
struct StructLiteral;

// типы
struct SimpleType;
struct ArrayType;
struct QualifiedType;

// инструкции
struct LetStmt;
struct AssignStmt;
struct IfStmt;
struct WhileStmt;
struct ReturnStmt;
struct BreakStmt;
struct ContinueStmt;
struct ExprStmt;
struct Block;

// объявления верхнего уровня
struct FunctionDecl;
struct StructDecl;
struct NamespaceDecl;
struct TypeAliasDecl;
struct ImplDecl;

// определение простых типов
struct IntLiteral {
    std::string value;
};

struct FloatLiteral {
    std::string value;
};

struct CharLiteral {
    std::uint32_t value;
};

struct StringLiteral {
    std::string value;
};

struct BoolLiteral {
    bool value;
};

struct Identifier {
    std::vector<std::string> parts;
};

struct SimpleType {
    std::string name;
};

struct QualifiedType {
    std::vector<std::string> parts;
};

struct BreakStmt {};

struct ContinueStmt {};

struct NullStmt {};

// выражения
using ExprNode =
    std::variant<IntLiteral, FloatLiteral, CharLiteral, StringLiteral,
                 BoolLiteral, Identifier, std::unique_ptr<BinaryExpr>,
                 std::unique_ptr<UnaryExpr>, std::unique_ptr<CallExpr>,
                 std::unique_ptr<IndexExpr>, std::unique_ptr<FieldAccess>,
                 std::unique_ptr<MethodCall>, std::unique_ptr<CastExpr>,
                 std::unique_ptr<ArrayLiteral>, std::unique_ptr<StructLiteral>>;

struct Expr {
    ExprNode node;
    NodeLocation loc;
};

// типы
using TypeNode =
    std::variant<SimpleType, std::unique_ptr<ArrayType>, QualifiedType>;

struct TypeExpr {
    TypeNode node;
    NodeLocation loc;
};

// инструкции
using StmtNode =
    std::variant<std::unique_ptr<LetStmt>, std::unique_ptr<AssignStmt>,
                 std::unique_ptr<IfStmt>, std::unique_ptr<WhileStmt>,
                 std::unique_ptr<ReturnStmt>, std::unique_ptr<ExprStmt>,
                 std::unique_ptr<Block>, BreakStmt, ContinueStmt, NullStmt>;

struct Stmt {
    StmtNode node;
    NodeLocation loc;
};

// объявления
using DeclNode =
    std::variant<std::unique_ptr<FunctionDecl>, std::unique_ptr<StructDecl>,
                 std::unique_ptr<NamespaceDecl>, std::unique_ptr<TypeAliasDecl>,
                 std::unique_ptr<ImplDecl>>;

struct Decl {
    DeclNode node;
    NodeLocation loc;
};

// определение сложных (рекурсивных) структур
// выражения
struct BinaryExpr {
    TokenType op;
    Expr left;
    Expr right;
};

struct UnaryExpr {
    TokenType op;
    Expr operand;
};

struct CallExpr {
    Identifier name;
    std::vector<Expr> args;
};

struct IndexExpr {
    Expr object;
    Expr index;
};

struct FieldAccess {
    Expr object;
    std::string field;
};

struct MethodCall {
    Expr object;
    std::string method;
    std::vector<Expr> args;
};

struct CastExpr {
    TypeExpr target_type;
    Expr value;
};

struct ArrayLiteral {
    std::vector<Expr> elements;
};

struct FieldInit {
    std::string name;
    Expr value;
    NodeLocation loc;
};

struct StructLiteral {
    Identifier name;
    std::vector<FieldInit> fields;
};

// структуры
struct ArrayType {
    std::string size;
    TypeExpr element;
};

// инструкции
struct Param {
    std::string name;
    TypeExpr type;
    NodeLocation loc;
};

struct Block {
    std::vector<Stmt> statements;
};

struct LetStmt {
    bool is_mutable;
    std::string name;
    std::optional<TypeExpr> type;
    Expr init;
};

struct AssignStmt {
    Expr target;
    Expr value;
};

struct ReturnStmt {
    std::optional<Expr> value;
};

struct IfStmt {
    Expr condition;
    Block then_block;
    std::optional<std::unique_ptr<Stmt>> else_branch;
};

struct WhileStmt {
    Expr condition;
    Block body;
};

struct ExprStmt {
    Expr expression;
};

// объявления верхнего уровня
struct FunctionDecl {
    std::string name;
    std::vector<Param> params;
    TypeExpr return_type;
    Block body;
};

struct StructField {
    std::string name;
    TypeExpr type;
    NodeLocation loc;
};

struct StructDecl {
    std::string name;
    std::vector<StructField> fields;
};

struct NamespaceDecl {
    std::string name;
    std::vector<Decl> declarations;
};

struct TypeAliasDecl {
    std::string name;
    TypeExpr type;
};

struct ImplDecl {
    std::string struct_name;
    std::vector<std::unique_ptr<FunctionDecl>> methods;
};

// корень
struct Program {
    std::vector<Decl> declarations;
};

inline Expr makeExpr(ExprNode node, NodeLocation loc) {
    return Expr{std::move(node), loc};
}
inline Stmt makeStmt(StmtNode node, NodeLocation loc) {
    return Stmt{std::move(node), loc};
}
inline Decl makeDecl(DeclNode node, NodeLocation loc) {
    return Decl{std::move(node), loc};
}
inline TypeExpr makeType(TypeNode node, NodeLocation loc) {
    return TypeExpr{std::move(node), loc};
}

// AST-принтер
void printAst(const Program& program, std::ostream& out);

}  // namespace Parser
