#pragma once

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "lexer/token.hpp"

namespace Parser {
struct NodeLocation {
    size_t line = 0;
    size_t col = 0;
};

// forward declaration
// выражения
struct IntLiteral;
struct FloatLiteral;
struct StringLiteral;
struct BoolLiteral;
struct Identifier;
struct BinaryExpr;
struct UnaryExpr;
struct CallExpr;
struct IndexExpr;
struct FieldAccess;
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

// определение простых типов
struct IntLiteral {
    std::string value;
};

struct FloatLiteral {
    std::string value;
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

// выражения
using ExprNode =
    std::variant<IntLiteral, FloatLiteral, StringLiteral, BoolLiteral,
                 Identifier, std::unique_ptr<BinaryExpr>,
                 std::unique_ptr<UnaryExpr>, std::unique_ptr<CallExpr>,
                 std::unique_ptr<IndexExpr>, std::unique_ptr<FieldAccess>,
                 std::unique_ptr<CastExpr>, std::unique_ptr<ArrayLiteral>,
                 std::unique_ptr<StructLiteral>>;

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
                 std::unique_ptr<Block>, BreakStmt, ContinueStmt>;

struct Stmt {
    StmtNode node;
    NodeLocation loc;
};

// объявления
using DeclNode =
    std::variant<std::unique_ptr<FunctionDecl>, std::unique_ptr<StructDecl>,
                 std::unique_ptr<NamespaceDecl>,
                 std::unique_ptr<TypeAliasDecl>>;

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
    TypeExpr type;
    Expr init;
};

struct AssignStmt {
    Expr target;  // lvalue: Identifier, IndexExpr или FieldAccess
    Expr value;
};

struct ReturnStmt {
    std::optional<Expr> value;  // void-функции: return; без значения
};

struct IfStmt {
    Expr condition;
    Block then_block;
    // unique_ptr: else необязателен, а если есть — Block или ещё один IfStmt
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

// корень
struct Program {
    std::vector<Decl> declarations;
};
}  // namespace Parser