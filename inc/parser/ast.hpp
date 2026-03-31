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
struct GroupedExpr;

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

// выражения
using ExprNode =
    std::variant<IntLiteral, FloatLiteral, StringLiteral, BoolLiteral,
                 Identifier, std::unique_ptr<BinaryExpr>,
                 std::unique_ptr<UnaryExpr>, std::unique_ptr<CallExpr>,
                 std::unique_ptr<IndexExpr>, std::unique_ptr<FieldAccess>,
                 std::unique_ptr<CastExpr>, std::unique_ptr<ArrayLiteral>,
                 std::unique_ptr<StructLiteral>, std::unique_ptr<GroupedExpr>>;

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
}  // namespace Parser