#include "parser/ast.hpp"

#include <ostream>
#include <string>

#include "lexer/token.hpp"

namespace Parser {

namespace {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

std::string ind(int depth) { return std::string(depth * 2, ' '); }

std::string opStr(TokenType op) {
    switch (op) {
        case TokenType::PLUS: return "+";
        case TokenType::MINUS: return "-";
        case TokenType::STAR: return "*";
        case TokenType::SLASH: return "/";
        case TokenType::PERCENT: return "%";
        case TokenType::EQUAL: return "==";
        case TokenType::NOT_EQUAL: return "!=";
        case TokenType::LESS: return "<";
        case TokenType::GREATER: return ">";
        case TokenType::LESS_EQUAL: return "<=";
        case TokenType::GREATER_EQUAL: return ">=";
        case TokenType::AND_AND: return "&&";
        case TokenType::OR_OR: return "||";
        case TokenType::NOT: return "!";
        default: return "?";
    }
}

std::string qualName(const std::vector<std::string>& parts) {
    std::string s;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) s += "::";
        s += parts[i];
    }
    return s;
}

void printExpr(const Expr& e, std::ostream& out, int depth);
void printStmt(const Stmt& s, std::ostream& out, int depth);
void printDecl(const Decl& d, std::ostream& out, int depth);

void printType(const TypeExpr& t, std::ostream& out, int depth) {
    std::visit(
        overloaded{
            [&](const SimpleType& s) { out << ind(depth) << s.name << '\n'; },
            [&](const QualifiedType& q) {
                out << ind(depth) << qualName(q.parts) << '\n';
            },
            [&](const std::unique_ptr<ArrayType>& a) {
                out << ind(depth) << "[" << a->size << "]\n";
                printType(a->element, out, depth + 1);
            },
        },
        t.node);
}

void printExpr(const Expr& e, std::ostream& out, int depth) {
    std::visit(
        overloaded{
            [&](const IntLiteral& v) {
                out << ind(depth) << "IntLiteral " << v.value << '\n';
            },
            [&](const FloatLiteral& v) {
                out << ind(depth) << "FloatLiteral " << v.value << '\n';
            },
            [&](const StringLiteral& v) {
                out << ind(depth) << "StringLiteral \"" << v.value << "\"\n";
            },
            [&](const BoolLiteral& v) {
                out << ind(depth) << "BoolLiteral "
                    << (v.value ? "true" : "false") << '\n';
            },
            [&](const Identifier& id) {
                out << ind(depth) << "Identifier " << qualName(id.parts)
                    << '\n';
            },
            [&](const std::unique_ptr<BinaryExpr>& b) {
                out << ind(depth) << "BinaryExpr " << opStr(b->op) << '\n';
                printExpr(b->left, out, depth + 1);
                printExpr(b->right, out, depth + 1);
            },
            [&](const std::unique_ptr<UnaryExpr>& u) {
                out << ind(depth) << "UnaryExpr " << opStr(u->op) << '\n';
                printExpr(u->operand, out, depth + 1);
            },
            [&](const std::unique_ptr<CallExpr>& c) {
                out << ind(depth) << "CallExpr "
                    << qualName(c->name.parts) << '\n';
                for (const auto& arg : c->args)
                    printExpr(arg, out, depth + 1);
            },
            [&](const std::unique_ptr<IndexExpr>& ix) {
                out << ind(depth) << "IndexExpr\n";
                printExpr(ix->object, out, depth + 1);
                printExpr(ix->index, out, depth + 1);
            },
            [&](const std::unique_ptr<FieldAccess>& fa) {
                out << ind(depth) << "FieldAccess ." << fa->field << '\n';
                printExpr(fa->object, out, depth + 1);
            },
            [&](const std::unique_ptr<CastExpr>& ce) {
                out << ind(depth) << "CastExpr\n";
                printType(ce->target_type, out, depth + 1);
                printExpr(ce->value, out, depth + 1);
            },
            [&](const std::unique_ptr<ArrayLiteral>& al) {
                out << ind(depth) << "ArrayLiteral\n";
                for (const auto& el : al->elements)
                    printExpr(el, out, depth + 1);
            },
            [&](const std::unique_ptr<StructLiteral>& sl) {
                out << ind(depth) << "StructLiteral "
                    << qualName(sl->name.parts) << '\n';
                for (const auto& f : sl->fields) {
                    out << ind(depth + 1) << "." << f.name << ":\n";
                    printExpr(f.value, out, depth + 2);
                }
            },
        },
        e.node);
}

void printBlock(const Block& b, std::ostream& out, int depth) {
    out << ind(depth) << "Block\n";
    for (const auto& st : b.statements)
        printStmt(st, out, depth + 1);
}

void printFn(const FunctionDecl& fn, std::ostream& out, int depth) {
    out << ind(depth) << "FunctionDecl " << fn.name << " ->\n";
    printType(fn.return_type, out, depth + 1);
    for (const auto& p : fn.params) {
        out << ind(depth + 1) << "Param " << p.name << ":\n";
        printType(p.type, out, depth + 2);
    }
    printBlock(fn.body, out, depth + 1);
}

void printStmt(const Stmt& s, std::ostream& out, int depth) {
    std::visit(
        overloaded{
            [&](const std::unique_ptr<LetStmt>& l) {
                out << ind(depth) << "LetStmt "
                    << (l->is_mutable ? "mut" : "let") << " " << l->name
                    << '\n';
                if (l->type) {
                    out << ind(depth + 1) << "Type:\n";
                    printType(*l->type, out, depth + 2);
                }
                out << ind(depth + 1) << "Init:\n";
                printExpr(l->init, out, depth + 2);
            },
            [&](const std::unique_ptr<AssignStmt>& a) {
                out << ind(depth) << "AssignStmt\n";
                out << ind(depth + 1) << "Target:\n";
                printExpr(a->target, out, depth + 2);
                out << ind(depth + 1) << "Value:\n";
                printExpr(a->value, out, depth + 2);
            },
            [&](const std::unique_ptr<IfStmt>& i) {
                out << ind(depth) << "IfStmt\n";
                out << ind(depth + 1) << "Cond:\n";
                printExpr(i->condition, out, depth + 2);
                out << ind(depth + 1) << "Then:\n";
                for (const auto& st : i->then_block.statements)
                    printStmt(st, out, depth + 2);
                if (i->else_branch) {
                    out << ind(depth + 1) << "Else:\n";
                    printStmt(**i->else_branch, out, depth + 2);
                }
            },
            [&](const std::unique_ptr<WhileStmt>& w) {
                out << ind(depth) << "WhileStmt\n";
                out << ind(depth + 1) << "Cond:\n";
                printExpr(w->condition, out, depth + 2);
                out << ind(depth + 1) << "Body:\n";
                for (const auto& st : w->body.statements)
                    printStmt(st, out, depth + 2);
            },
            [&](const std::unique_ptr<ReturnStmt>& r) {
                out << ind(depth) << "ReturnStmt\n";
                if (r->value) printExpr(*r->value, out, depth + 1);
            },
            [&](const std::unique_ptr<ExprStmt>& e) {
                out << ind(depth) << "ExprStmt\n";
                printExpr(e->expression, out, depth + 1);
            },
            [&](const std::unique_ptr<Block>& b) {
                printBlock(*b, out, depth);
            },
            [&](const BreakStmt&) { out << ind(depth) << "BreakStmt\n"; },
            [&](const ContinueStmt&) {
                out << ind(depth) << "ContinueStmt\n";
            },
            [&](const NullStmt&) { out << ind(depth) << "NullStmt\n"; },
        },
        s.node);
}

void printDecl(const Decl& d, std::ostream& out, int depth) {
    std::visit(
        overloaded{
            [&](const std::unique_ptr<FunctionDecl>& fn) {
                printFn(*fn, out, depth);
            },
            [&](const std::unique_ptr<StructDecl>& s) {
                out << ind(depth) << "StructDecl " << s->name << '\n';
                for (const auto& f : s->fields) {
                    out << ind(depth + 1) << "Field " << f.name << ":\n";
                    printType(f.type, out, depth + 2);
                }
            },
            [&](const std::unique_ptr<NamespaceDecl>& ns) {
                out << ind(depth) << "NamespaceDecl " << ns->name << '\n';
                for (const auto& decl : ns->declarations)
                    printDecl(decl, out, depth + 1);
            },
            [&](const std::unique_ptr<TypeAliasDecl>& ta) {
                out << ind(depth) << "TypeAliasDecl " << ta->name << " =\n";
                printType(ta->type, out, depth + 1);
            },
            [&](const std::unique_ptr<ImplDecl>& impl) {
                out << ind(depth) << "ImplDecl " << impl->struct_name << '\n';
                for (const auto& m : impl->methods)
                    printFn(*m, out, depth + 1);
            },
        },
        d.node);
}

}  // namespace

void printAst(const Program& program, std::ostream& out) {
    out << "Program\n";
    for (const auto& decl : program.declarations)
        printDecl(decl, out, 1);
}

}  // namespace Parser
