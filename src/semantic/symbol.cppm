export module bina.semantic.symbol;

import std;

import bina.parser.ast;
import bina.semantic.type;

export namespace Semantic {

enum class SymbolKind { VARIABLE, FUNCTION, STRUCT, TYPE_ALIAS, NAMESPACE };

struct VariableSymbol {
    std::string name;
    Type type;
    bool is_mutable;
    Parser::NodeLocation loc;
};

struct FunctionSignature {
    std::string name;
    std::vector<Type> param_types;
    std::vector<std::string> param_names;
    Type return_type;
    Parser::NodeLocation loc;
    const Parser::FunctionDecl* decl;
};

struct StructSymbol {
    std::string name;
    std::string qualified_name;
    std::vector<std::pair<std::string, Type>> fields;
    Parser::NodeLocation loc;
};

struct TypeAliasSymbol {
    std::string name;
    Type target_type;
    const Parser::TypeAliasDecl* decl = nullptr;
    Parser::NodeLocation loc;
};

}  // namespace Semantic
