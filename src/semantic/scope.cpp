module bina.semantic.scope;

import std;
import bina.semantic.symbol;
import bina.semantic.type;

namespace Semantic {
namespace {

std::string indent(int depth) { return std::string(static_cast<std::size_t>(depth) * 2, ' '); }

std::string functionSignatureToString(const FunctionSignature& sig) {
    std::string result = sig.name + "(";
    for (std::size_t i = 0; i < sig.param_types.size(); ++i) {
        if (i > 0) result += ", ";
        if (i < sig.param_names.size() && !sig.param_names[i].empty()) {
            result += sig.param_names[i] + ": ";
        }
        result += typeToString(sig.param_types[i]);
    }
    result += ") -> " + typeToString(sig.return_type);
    return result;
}

}  // namespace

Scope::Scope(Scope* parent) : m_parent(parent) {}

bool Scope::declareVariable(VariableSymbol sym) {
    if (m_variables.contains(sym.name) || m_functions.contains(sym.name) ||
        m_structs.contains(sym.name) || m_aliases.contains(sym.name) ||
        m_namespaces.contains(sym.name)) {
        return false;
    }

    m_variables.insert({sym.name, std::move(sym)});
    return true;
}

bool Scope::declareFunction(FunctionSignature sig) {
    if (m_variables.contains(sig.name) || m_structs.contains(sig.name) ||
        m_aliases.contains(sig.name) || m_namespaces.contains(sig.name)) {
        return false;
    }

    auto& overloads = m_functions[sig.name];
    for (const auto& existing : overloads) {
        if (existing.param_types == sig.param_types) return false;
    }
    overloads.push_back(std::move(sig));
    return true;
}

bool Scope::declareStruct(StructSymbol sym) {
    if (m_variables.contains(sym.name) || m_functions.contains(sym.name) ||
        m_structs.contains(sym.name) || m_aliases.contains(sym.name) ||
        m_namespaces.contains(sym.name)) {
        return false;
    }

    m_structs.emplace(sym.name, std::move(sym));
    return true;
}

bool Scope::declareTypeAlias(TypeAliasSymbol sym) {
    if (m_variables.contains(sym.name) || m_functions.contains(sym.name) ||
        m_structs.contains(sym.name) || m_aliases.contains(sym.name) ||
        m_namespaces.contains(sym.name)) {
        return false;
    }

    m_aliases.emplace(sym.name, std::move(sym));
    return true;
}

bool Scope::declareNamespace(std::string name, std::unique_ptr<Scope> ns) {
    if (m_variables.contains(name) || m_functions.contains(name) ||
        m_structs.contains(name) || m_aliases.contains(name) ||
        m_namespaces.contains(name)) {
        return false;
    }

    m_namespaces.emplace(name, std::move(ns));
    return true;
}

bool Scope::defineStruct(
    const std::string& name,
    std::vector<std::pair<std::string, Type>> fields) {
    auto it = m_structs.find(name);
    if (it == m_structs.end()) return false;

    it->second.fields = std::move(fields);
    return true;
}

bool Scope::defineTypeAlias(const std::string& name, Type target_type) {
    auto it = m_aliases.find(name);
    if (it == m_aliases.end()) return false;

    it->second.target_type = std::move(target_type);
    return true;
}

const VariableSymbol* Scope::findVariable(const std::string& name) const {
    auto it = m_variables.find(name);

    if (it != m_variables.end()) return &it->second;

    if (m_parent != nullptr) return m_parent->findVariable(name);

    return nullptr;
}

std::vector<const FunctionSignature*> Scope::findFunctions(
    const std::string& name) const {
    auto it = m_functions.find(name);

    if (it != m_functions.end()) {
        std::vector<const FunctionSignature*> result;
        for (const auto& sig : it->second) result.push_back(&sig);
        return result;
    }

    if (m_variables.contains(name) || m_structs.contains(name) ||
        m_aliases.contains(name) || m_namespaces.contains(name)) {
        return {};
    }

    if (m_parent != nullptr) {
        return m_parent->findFunctions(name);
    }

    return {};
}

const StructSymbol* Scope::findStruct(const std::string& name) const {
    auto it = m_structs.find(name);

    if (it != m_structs.end()) return &it->second;

    if (m_parent != nullptr) return m_parent->findStruct(name);

    return nullptr;
}

const StructSymbol* Scope::findLocalStruct(const std::string& name) const {
    auto it = m_structs.find(name);
    if (it != m_structs.end()) return &it->second;
    return nullptr;
}

const TypeAliasSymbol* Scope::findTypeAlias(const std::string& name) const {
    auto it = m_aliases.find(name);

    if (it != m_aliases.end()) return &it->second;

    if (m_parent != nullptr) return m_parent->findTypeAlias(name);

    return nullptr;
}

const Scope* Scope::findNamespace(const std::string& name) const {
    auto it = m_namespaces.find(name);
    if (it != m_namespaces.end()) return it->second.get();

    if (m_parent != nullptr) return m_parent->findNamespace(name);

    return nullptr;
}

Scope* Scope::findNamespaceMutable(const std::string& name) {
    auto it = m_namespaces.find(name);
    if (it != m_namespaces.end()) return it->second.get();

    if (m_parent != nullptr) return m_parent->findNamespaceMutable(name);

    return nullptr;
}

std::expected<SymbolRef, std::string> Scope::findByPath(
    const std::vector<std::string>& parts) const {
    if (parts.empty()) return std::unexpected("empty qualified name");

    if (parts.size() == 1) {
        const std::string& name = parts[0];

        if (auto it = m_variables.find(name); it != m_variables.end()) {
            return &it->second;
        }

        if (auto it = m_functions.find(name); it != m_functions.end()) {
            std::vector<const FunctionSignature*> result;
            result.reserve(it->second.size());

            for (const auto& sig : it->second) {
                result.push_back(&sig);
            }

            return result;
        }

        if (auto it = m_structs.find(name); it != m_structs.end()) {
            return &it->second;
        }

        if (auto it = m_aliases.find(name); it != m_aliases.end()) {
            return &it->second;
        }

        if (auto it = m_namespaces.find(name); it != m_namespaces.end()) {
            return static_cast<const Scope*>(it->second.get());
        }

        if (m_parent != nullptr) return m_parent->findByPath(parts);

        return std::unexpected("symbol '" + name + "' not found");
    }

    const Scope* current = this;
    while (current != nullptr) {
        auto it = current->m_namespaces.find(parts[0]);
        if (it != current->m_namespaces.end()) {
            current = it->second.get();
            break;
        }

        if (current->m_variables.contains(parts[0]) ||
            current->m_functions.contains(parts[0]) ||
            current->m_structs.contains(parts[0]) ||
            current->m_aliases.contains(parts[0])) {
            return std::unexpected("symbol '" + parts[0] +
                                   "' is not a namespace");
        }

        current = current->m_parent;
    }

    if (current == nullptr) {
        return std::unexpected("namespace '" + parts[0] + "' not found");
    }

    for (std::size_t i = 1; i + 1 < parts.size(); ++i) {
        const std::string& namespace_name = parts[i];

        auto it = current->m_namespaces.find(namespace_name);
        if (it == current->m_namespaces.end()) {
            return std::unexpected("namespace '" + namespace_name +
                                   "' not found");
        }

        current = it->second.get();
    }

    const std::string& final_name = parts.back();

    if (auto it = current->m_variables.find(final_name);
        it != current->m_variables.end()) {
        return &it->second;
    }

    if (auto it = current->m_functions.find(final_name);
        it != current->m_functions.end()) {
        std::vector<const FunctionSignature*> result;
        result.reserve(it->second.size());

        for (const auto& sig : it->second) {
            result.push_back(&sig);
        }

        return result;
    }

    if (auto it = current->m_structs.find(final_name);
        it != current->m_structs.end()) {
        return &it->second;
    }

    if (auto it = current->m_aliases.find(final_name);
        it != current->m_aliases.end()) {
        return &it->second;
    }

    if (auto it = current->m_namespaces.find(final_name);
        it != current->m_namespaces.end()) {
        return static_cast<const Scope*>(it->second.get());
    }

    return std::unexpected("symbol '" + final_name + "' not found");
}

void Scope::dumpSymbols(std::ostream& out, int depth) const {
    const std::string pad = indent(depth);

    for (const auto& [name, variable] : m_variables) {
        out << pad << "var " << name << ": " << typeToString(variable.type);
        out << (variable.is_mutable ? " mut" : " let") << '\n';
    }

    for (const auto& [name, overloads] : m_functions) {
        (void)name;
        for (const auto& sig : overloads) {
            out << pad << "fn " << functionSignatureToString(sig) << '\n';
        }
    }

    for (const auto& [name, structure] : m_structs) {
        out << pad << "struct " << structure.qualified_name << '\n';
        for (const auto& [field_name, field_type] : structure.fields) {
            out << pad << "  field " << field_name << ": "
                << typeToString(field_type) << '\n';
        }
    }

    for (const auto& [name, alias] : m_aliases) {
        out << pad << "type " << name << " = "
            << typeToString(alias.target_type) << '\n';
    }

    for (const auto& [name, ns] : m_namespaces) {
        out << pad << "namespace " << name << '\n';
        ns->dumpSymbols(out, depth + 1);
    }
}

}  // namespace Semantic
