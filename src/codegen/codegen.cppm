export module bina.codegen;

import std;
import bina.parser.ast;
import bina.semantic;
import bina.semantic.scope;
import bina.semantic.symbol;
import bina.semantic.type;

export namespace Codegen {

class Codegen {
   public:
    Codegen(const Semantic::TypedProgram& typed, std::string source_filename);

    std::expected<std::string, std::vector<std::string>> emit();

   private:
    struct Value {
        std::string ssa;
        Semantic::Type type;
    };

    struct LocalSlot {
        std::string addr;
        Semantic::Type type;
    };

    const Semantic::TypedProgram& m_typed;
    std::string m_source_filename;

    std::ostringstream m_header;
    std::ostringstream m_body;
    std::ostringstream* m_emit = nullptr;

    int m_value_id = 0;
    int m_label_id = 0;
    int m_str_const_id = 0;

    std::vector<std::unordered_map<std::string, LocalSlot>> m_env;
    std::unordered_map<const void*, std::string> m_local_addrs;
    std::unordered_set<std::string> m_emitted_structs;

    Semantic::Type m_current_return_type;
    bool m_in_main = false;
    bool m_block_terminated = false;
    std::string m_current_label;
    std::vector<std::pair<std::string, std::string>> m_loop_stack;

    void emitModuleHeader();
    void emitRuntimeDecls();
    void emitStructTypes();
    void emitFunctions();
    void emitFunction(const Parser::FunctionDecl& fn,
                      const Semantic::FunctionSignature& sig);

    void genStmt(const Parser::Stmt& s);
    void genLet(const Parser::LetStmt& l, Parser::NodeLocation loc);
    void genAssign(const Parser::AssignStmt& a, Parser::NodeLocation loc);
    void genIf(const Parser::IfStmt& i, Parser::NodeLocation loc);
    void genWhile(const Parser::WhileStmt& w, Parser::NodeLocation loc);
    void genReturn(const Parser::ReturnStmt& r, Parser::NodeLocation loc);
    void genBreak();
    void genContinue();
    void genBlock(const Parser::Block& b);
    void genExprStmt(const Parser::ExprStmt& e);

    Value genExpr(const Parser::Expr& e);
    Value genIntLit(const Parser::IntLiteral& l, const Semantic::Type& t);
    Value genFloatLit(const Parser::FloatLiteral& l, const Semantic::Type& t);
    Value genCharLit(const Parser::CharLiteral& l);
    Value genStringLit(const Parser::StringLiteral& l);
    Value genBoolLit(const Parser::BoolLiteral& l);
    Value genIdentifier(const Parser::Identifier& id,
                        const Semantic::Type& t);
    Value genBinary(const Parser::BinaryExpr& b,
                    const Semantic::Type& result_t,
                    Parser::NodeLocation loc);
    Value genUnary(const Parser::UnaryExpr& u,
                   const Semantic::Type& result_t,
                   Parser::NodeLocation loc);
    Value genCall(const Parser::CallExpr& c, const Semantic::Type& result_t,
                  Parser::NodeLocation loc);
    Value genMethodCall(const Parser::MethodCall& c,
                        const Semantic::Type& result_t,
                        Parser::NodeLocation loc);
    Value genIndex(const Parser::IndexExpr& ix,
                   const Semantic::Type& result_t,
                   Parser::NodeLocation loc);
    Value genFieldAccess(const Parser::FieldAccess& fa,
                         const Semantic::Type& result_t);
    Value genCast(const Parser::CastExpr& c, const Semantic::Type& result_t,
                  Parser::NodeLocation loc);
    Value genArrayLit(const Parser::Expr& wrap,
                      const Parser::ArrayLiteral& al,
                      Parser::NodeLocation loc);
    Value genStructLit(const Parser::StructLiteral& sl,
                       const Semantic::Type& result_t);

    Value genBuiltinPrint(const Parser::CallExpr& c);
    Value genBuiltinInput();
    Value genBuiltinLen(const Parser::CallExpr& c);
    Value genBuiltinAssert(const Parser::CallExpr& c, Parser::NodeLocation loc);
    Value genBuiltinCode(const Parser::CallExpr& c);
    Value genBuiltinCharFrom(const Parser::CallExpr& c,
                             Parser::NodeLocation loc);
    Value genBuiltinExit(const Parser::CallExpr& c);
    Value genBuiltinPanic(const Parser::CallExpr& c, Parser::NodeLocation loc);

    std::string genLvalueAddr(const Parser::Expr& e);
    bool isLvalueExpr(const Parser::Expr& e);
    std::string materializeAddr(const Value& v);

    Value genCompareEq(const Semantic::Type& t, const std::string& lhs_ptr,
                       const std::string& rhs_ptr);

    std::string freshValue();
    std::string freshLabel(std::string_view prefix);
    std::string freshStrConst();
    std::string freshLocalAddr(std::string_view source_name);
    std::string sanitizeIdent(std::string_view source_name);
    std::string emitStringConstant(std::string_view bytes);

    std::string llvmType(const Semantic::Type& t);
    std::string llvmStoredType(const Semantic::Type& t);
    std::string mangle(const Semantic::FunctionSignature& sig);
    std::string mangleType(const Semantic::Type& t);
    std::string mangleStruct(const std::string& qualified_name);

    void emitTerminator(std::string_view instr);
    void startLabel(std::string_view label);
    Value emitConversion(Value v, const Semantic::Type& to,
                         bool explicit_cast, Parser::NodeLocation loc);
    std::string convertNumeric(Value v, const Semantic::Type& to);
    Value makeBinOp(std::string_view op, std::string_view llvm_ty,
                    std::string_view lhs, std::string_view rhs,
                    const Semantic::Type& result_t);
    Value emitCheckedAddSubMul(std::string_view base_op,
                               const Semantic::Type& t,
                               const std::string& lhs,
                               const std::string& rhs,
                               Parser::NodeLocation loc);
    void emitDivByZeroCheck(const std::string& rhs, std::string_view llvm_ty,
                            Parser::NodeLocation loc);
    void emitSignedDivOverflowCheck(const std::string& lhs,
                                    const std::string& rhs,
                                    const Semantic::Type& t,
                                    Parser::NodeLocation loc);
    void emitBoundsCheck(const std::string& index, std::size_t len,
                         Parser::NodeLocation loc);
    void emitRangeCheck(const std::string& i64_val, Semantic::TypeKind to,
                        Parser::NodeLocation loc);
    Value genShortCircuit(const Parser::BinaryExpr& b, bool is_and);
    void emitPrintArray(const Semantic::Type& at, const std::string& addr);
    void emitPrintStruct(const Semantic::Type& st, const std::string& addr);
    void emitPrintValueAtAddr(const Semantic::Type& t,
                              const std::string& ptr);
    void emitPrintScalarSsa(const Semantic::Type& t, const std::string& ssa);
    void emitPrintLiteralString(std::string_view text);
    void storeValue(const Semantic::Type& t, const std::string& value,
                    const std::string& addr);
    Value loadValueAtAddr(const Semantic::Type& t, const std::string& addr);
    bool isSigned(Semantic::TypeKind k);
    int bitWidth(Semantic::TypeKind k);
    int structFieldIndex(const Semantic::StructSymbol& s,
                         const std::string& field);
    const Semantic::StructSymbol* findStructByQualifiedName(
        const std::string& qname);
    const Semantic::FunctionSignature* findSignatureForFunctionDecl(
        const std::vector<std::string>& ns, const Parser::FunctionDecl& fn);
    const Semantic::FunctionSignature* findSignatureForMethodDecl(
        const std::vector<std::string>& ns, const Parser::ImplDecl& impl,
        const Parser::FunctionDecl& method);

    Semantic::Type exprType(const Parser::Expr& e);
    const Semantic::FunctionSignature* callTarget(const void* call_node);

    void envPush();
    void envPop();
    void envDeclare(const std::string& name, std::string addr,
                    Semantic::Type type, const void* decl_node);
    const LocalSlot* envLookup(const std::string& name) const;
};

}  // namespace Codegen
