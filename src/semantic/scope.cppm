export module bina.semantic.scope;

import std;
import bina.semantic.symbol;
import bina.semantic.type;

export namespace Semantic {

class Scope;

using SymbolRef =
    std::variant<const VariableSymbol*, std::vector<const FunctionSignature*>,
                 const StructSymbol*, const TypeAliasSymbol*, const Scope*>;

class Scope {
   public:
    explicit Scope(Scope* parent = nullptr);

    // объявления символов
    bool declareVariable(VariableSymbol sym);
    bool declareFunction(FunctionSignature sig);
    bool declareStruct(StructSymbol sym);
    bool declareTypeAlias(TypeAliasSymbol sym);
    bool declareNamespace(std::string name, std::unique_ptr<Scope> ns);
    bool defineStruct(const std::string& name,
                      std::vector<StructFieldSymbol> fields);
    bool defineTypeAlias(const std::string& name, Type target_type);

    // поиск
    const VariableSymbol* findVariable(const std::string& name) const;
    std::vector<const FunctionSignature*> findFunctions(
        const std::string& name) const;
    const StructSymbol* findStruct(const std::string& name) const;
    const StructSymbol* findLocalStruct(const std::string& name) const;
    const TypeAliasSymbol* findTypeAlias(const std::string& name) const;
    const Scope* findNamespace(const std::string& name) const;
    Scope* findNamespaceMutable(const std::string& name);

    std::expected<SymbolRef, std::string> findByPath(
        const std::vector<std::string>& parts) const;

    void dumpSymbols(std::ostream& out, int depth = 0) const;

   private:
    Scope* m_parent;
    std::unordered_map<std::string, VariableSymbol> m_variables;
    std::unordered_map<std::string, std::vector<FunctionSignature>>
        m_functions;  // перегрузки
    std::unordered_map<std::string, StructSymbol> m_structs;
    std::unordered_map<std::string, TypeAliasSymbol> m_aliases;
    std::unordered_map<std::string, std::unique_ptr<Scope>> m_namespaces;
};
}  // namespace Semantic
