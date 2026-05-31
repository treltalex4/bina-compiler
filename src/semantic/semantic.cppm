export module bina.semantic;

import std;
import bina.parser.ast;
import bina.semantic.type;
import bina.semantic.symbol;
import bina.semantic.scope;

export namespace Semantic {

struct TypedProgram {
    const Parser::Program* program;
    TypeMap expr_types;
    TypeMap decl_types;
    std::unordered_map<const void*, const FunctionSignature*> call_targets;
    std::unique_ptr<Scope> global_scope;
};

class Semantic {
   public:
    Semantic(const Parser::Program& program, const std::string& filename);
    std::expected<TypedProgram, std::vector<std::string>> analyze();

   private:
    const Parser::Program& m_program;
    std::string m_filename;
    std::vector<std::string> m_errors;
    std::unique_ptr<Scope> m_global;
    TypeMap m_expr_types;
    TypeMap m_decl_types;
    std::unordered_map<const void*, const FunctionSignature*> m_call_targets;
    std::unordered_set<const TypeAliasSymbol*> m_resolving_aliases;
    std::unordered_set<std::string> m_impl_structs;

    Scope* m_current_scope = nullptr;
    Type m_current_return_type = makeError();
    int m_loop_depth = 0;

    std::expected<Type, std::string> resolveTypeExpr(const Parser::TypeExpr& te,
                                                     const Scope& scope);
    std::expected<Type, std::string> resolveTypeAlias(
        const TypeAliasSymbol& alias, const Scope& scope);

    void collectTopLevel(const Parser::Program& p, Scope& scope);
    void collectDecl(const Parser::Decl& d, Scope& scope);
    void collectStruct(const Parser::StructDecl& s, Scope& scope,
                       Parser::NodeLocation loc);
    void collectTypeAlias(const Parser::TypeAliasDecl& ta, Scope& scope,
                          Parser::NodeLocation loc);
    void collectNamespace(const Parser::NamespaceDecl& ns, Scope& scope,
                          Parser::NodeLocation loc);
    void collectFunction(const Parser::FunctionDecl& fn, Scope& scope,
                         Parser::NodeLocation loc);
    void collectImpl(const Parser::ImplDecl& impl, Scope& scope,
                     Parser::NodeLocation loc);

    void analyzeProgram();
    void analyzeNamespace(const Parser::NamespaceDecl& ns);
    void analyzeImpl(const Parser::ImplDecl& impl);
    void analyzeFunction(const Parser::FunctionDecl& fn, Scope& parent);
    void analyzeBlock(const Parser::Block& b);
    void analyzeStmt(const Parser::Stmt& s);

    void analyzeLet(const Parser::LetStmt& l, Parser::NodeLocation loc);
    void analyzeAssign(const Parser::AssignStmt& a, Parser::NodeLocation loc);
    void analyzeIf(const Parser::IfStmt& i, Parser::NodeLocation loc);
    void analyzeWhile(const Parser::WhileStmt& w, Parser::NodeLocation loc);
    void analyzeReturn(const Parser::ReturnStmt& r, Parser::NodeLocation loc);
    void analyzeBreak(Parser::NodeLocation loc);
    void analyzeContinue(Parser::NodeLocation loc);
    void analyzeExprStmt(const Parser::ExprStmt& e, Parser::NodeLocation loc);

    Type checkExpr(const Parser::Expr& e, const Type* expected = nullptr);
    Type checkBinary(const Parser::BinaryExpr& b, Parser::NodeLocation loc);
    Type checkUnary(const Parser::UnaryExpr& u, Parser::NodeLocation loc,
                    const Type* expected = nullptr);
    Type checkCall(const Parser::CallExpr& c, Parser::NodeLocation loc);
    Type checkMethodCall(const Parser::MethodCall& c, Parser::NodeLocation loc);
    Type checkIndex(const Parser::IndexExpr& ix, Parser::NodeLocation loc);
    Type checkFieldAccess(const Parser::FieldAccess& fa,
                          Parser::NodeLocation loc);
    Type checkCast(const Parser::CastExpr& c, Parser::NodeLocation loc);
    Type checkArrayLit(const Parser::ArrayLiteral& al, Parser::NodeLocation loc,
                       const Type* expected = nullptr);
    Type checkStructLit(const Parser::StructLiteral& sl,
                        Parser::NodeLocation loc);
    Type checkIdentifier(const Parser::Identifier& id,
                         Parser::NodeLocation loc);

    std::optional<Type> tryBuiltinCall(const Parser::CallExpr& c,
                                       Parser::NodeLocation loc);

    bool isLvalue(const Parser::Expr& e);
    bool isMutableLvalue(const Parser::Expr& e);

    bool allPathsReturn(const Parser::Block& b);

    void error(Parser::NodeLocation loc, const std::string& msg);
    void recordExprType(const Parser::Expr& e, Type t);
    void recordDeclType(const void* node, Type t);
};

}  // namespace Semantic
