module bina.semantic;

import std;
import bina.lexer.token;
import bina.semantic.overload;

namespace Semantic {
namespace {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

std::string joinName(const std::vector<std::string>& parts) {
    std::string result;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += "::";
        result += parts[i];
    }
    return result;
}

std::vector<std::string> appendName(std::vector<std::string> parts,
                                    const std::string& name) {
    parts.push_back(name);
    return parts;
}

std::vector<std::string> splitQualifiedName(const std::string& name) {
    std::vector<std::string> parts;
    std::size_t start = 0;

    while (start < name.size()) {
        std::size_t sep = name.find("::", start);
        if (sep == std::string::npos) {
            parts.push_back(name.substr(start));
            break;
        }

        parts.push_back(name.substr(start, sep - start));
        start = sep + 2;
    }

    return parts;
}

std::vector<const FunctionSignature*> findMethodOverloads(
    const Scope& scope, const std::string& qualified_struct_name,
    const std::string& method) {
    auto struct_parts = splitQualifiedName(qualified_struct_name);
    if (struct_parts.empty()) return {};

    const std::string method_name = struct_parts.back() + "::" + method;
    if (struct_parts.size() == 1) return scope.findFunctions(method_name);

    std::vector<std::string> namespace_parts(struct_parts.begin(),
                                             struct_parts.end() - 1);
    auto found = scope.findByPath(namespace_parts);
    if (!found) return {};

    if (const auto* ns = std::get_if<const Scope*>(&found.value())) {
        return (*ns)->findFunctions(method_name);
    }

    return {};
}

std::vector<const FunctionSignature*> findQualifiedMethodOverloads(
    const Scope& scope, const std::vector<std::string>& parts) {
    if (parts.size() < 2) return {};

    std::vector<std::string> struct_parts(parts.begin(), parts.end() - 1);
    auto found = scope.findByPath(struct_parts);
    if (!found) return {};

    if (const auto* structure =
            std::get_if<const StructSymbol*>(&found.value())) {
        return findMethodOverloads(scope, (*structure)->qualified_name,
                                   parts.back());
    }

    return {};
}

std::optional<TypeKind> builtinTypeKind(const std::string& name) {
    static const std::unordered_map<std::string, TypeKind> types = {
        {"int8", TypeKind::INT8},       {"int16", TypeKind::INT16},
        {"int32", TypeKind::INT32},     {"int64", TypeKind::INT64},
        {"int", TypeKind::INT64},       {"uint8", TypeKind::UINT8},
        {"uint16", TypeKind::UINT16},   {"uint32", TypeKind::UINT32},
        {"uint64", TypeKind::UINT64},   {"float32", TypeKind::FLOAT32},
        {"float64", TypeKind::FLOAT64}, {"bool", TypeKind::BOOL},
        {"char", TypeKind::CHAR},       {"string", TypeKind::STRING},
        {"void", TypeKind::VOID},
    };

    if (auto it = types.find(name); it != types.end()) return it->second;
    return std::nullopt;
}

std::string trimLeadingZeros(std::string value) {
    std::size_t first = value.find_first_not_of('0');
    if (first == std::string::npos) return "0";
    return value.substr(first);
}

bool decimalLessEqual(std::string value, std::string limit) {
    value = trimLeadingZeros(std::move(value));
    limit = trimLeadingZeros(std::move(limit));
    if (value.size() != limit.size()) return value.size() < limit.size();
    return value <= limit;
}

std::optional<TypeKind> integerSuffixKind(const std::string& suffix) {
    if (suffix.empty()) return std::nullopt;
    if (suffix == "i8") return TypeKind::INT8;
    if (suffix == "i16") return TypeKind::INT16;
    if (suffix == "i32") return TypeKind::INT32;
    if (suffix == "i64") return TypeKind::INT64;
    if (suffix == "u8") return TypeKind::UINT8;
    if (suffix == "u16") return TypeKind::UINT16;
    if (suffix == "u32") return TypeKind::UINT32;
    if (suffix == "u64") return TypeKind::UINT64;
    return std::nullopt;
}

struct ParsedIntLiteral {
    std::string digits;
    int base;
    std::string suffix;
};

int digitValue(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool isDigitForBase(char c, int base) {
    int value = digitValue(c);
    return value >= 0 && value < base;
}

std::string invalidBaseLiteralMessage(int base) {
    if (base == 16) return "invalid hexadecimal literal";
    if (base == 2) return "invalid binary literal";
    return "invalid integer literal";
}

std::expected<ParsedIntLiteral, std::string> parseIntLiteral(
    const std::string& value) {
    if (value.empty()) {
        return std::unexpected("invalid integer literal");
    }

    int base = 10;
    std::size_t pos = 0;

    if (value.size() >= 2 && value[0] == '0' &&
        (value[1] == 'x' || value[1] == 'X')) {
        base = 16;
        pos = 2;
    } else if (value.size() >= 2 && value[0] == '0' &&
               (value[1] == 'b' || value[1] == 'B')) {
        base = 2;
        pos = 2;
    }

    std::size_t digits_begin = pos;
    while (pos < value.size() && isDigitForBase(value[pos], base)) {
        ++pos;
    }

    if (pos == digits_begin) {
        return std::unexpected(invalidBaseLiteralMessage(base));
    }

    std::string suffix = value.substr(pos);
    if (!suffix.empty() && !integerSuffixKind(suffix)) {
        return std::unexpected("invalid integer literal suffix '" + suffix +
                               "'");
    }

    return ParsedIntLiteral{
        .digits = value.substr(digits_begin, pos - digits_begin),
        .base = base,
        .suffix = std::move(suffix),
    };
}

std::string multiplyDecimalString(std::string value, int multiplier) {
    value = trimLeadingZeros(std::move(value));

    int carry = 0;
    std::string result;
    result.reserve(value.size() + 1);

    for (auto it = value.rbegin(); it != value.rend(); ++it) {
        int product = (*it - '0') * multiplier + carry;
        result.push_back(static_cast<char>('0' + (product % 10)));
        carry = product / 10;
    }

    while (carry > 0) {
        result.push_back(static_cast<char>('0' + (carry % 10)));
        carry /= 10;
    }

    std::reverse(result.begin(), result.end());
    return trimLeadingZeros(std::move(result));
}

std::string addToDecimalString(std::string value, int addend) {
    value = trimLeadingZeros(std::move(value));

    int carry = addend;
    for (auto it = value.rbegin(); it != value.rend() && carry > 0; ++it) {
        int sum = (*it - '0') + carry;
        *it = static_cast<char>('0' + (sum % 10));
        carry = sum / 10;
    }

    while (carry > 0) {
        value.insert(value.begin(), static_cast<char>('0' + (carry % 10)));
        carry /= 10;
    }

    return trimLeadingZeros(std::move(value));
}

std::string literalDigitsToDecimal(std::string_view digits, int base) {
    std::string result = "0";
    for (char c : digits) {
        result = multiplyDecimalString(std::move(result), base);
        result = addToDecimalString(std::move(result), digitValue(c));
    }
    return trimLeadingZeros(std::move(result));
}

std::string integerMagnitudeLimit(TypeKind kind, bool negated) {
    switch (kind) {
        case TypeKind::INT8:
            return negated ? "128" : "127";
        case TypeKind::INT16:
            return negated ? "32768" : "32767";
        case TypeKind::INT32:
            return negated ? "2147483648" : "2147483647";
        case TypeKind::INT64:
            return negated ? "9223372036854775808" : "9223372036854775807";
        case TypeKind::UINT8:
            return negated ? "0" : "255";
        case TypeKind::UINT16:
            return negated ? "0" : "65535";
        case TypeKind::UINT32:
            return negated ? "0" : "4294967295";
        case TypeKind::UINT64:
            return negated ? "0" : "18446744073709551615";
        default:
            return "0";
    }
}

std::expected<Type, std::string> intLiteralType(const std::string& value,
                                                const Type* expected,
                                                bool negated) {
    auto parsed = parseIntLiteral(value);
    if (!parsed) return std::unexpected(parsed.error());

    TypeKind kind = TypeKind::INT32;
    if (!parsed->suffix.empty()) {
        kind = *integerSuffixKind(parsed->suffix);
    } else if (expected != nullptr && isInteger(*expected)) {
        kind = expected->kind;
    }

    std::string magnitude =
        literalDigitsToDecimal(parsed->digits, parsed->base);
    if (!decimalLessEqual(magnitude, integerMagnitudeLimit(kind, negated))) {
        return std::unexpected("integer literal '" +
                               std::string(negated ? "-" : "") + value +
                               "' is out of range for " +
                               typeToString(makePrimitive(kind)));
    }

    return makePrimitive(kind);
}

std::expected<std::size_t, std::string> parseArraySizeLiteral(
    const std::string& value) {
    auto parsed = parseIntLiteral(value);
    if (!parsed) return std::unexpected(parsed.error());

    std::string magnitude =
        literalDigitsToDecimal(parsed->digits, parsed->base);
    std::string limit = std::to_string(std::numeric_limits<std::size_t>::max());
    if (!decimalLessEqual(magnitude, limit)) {
        return std::unexpected("array size is too large");
    }

    std::size_t result = 0;
    for (char c : trimLeadingZeros(std::move(magnitude))) {
        result = result * 10 + static_cast<std::size_t>(c - '0');
    }
    if (result == 0) {
        return std::unexpected("array size must be positive");
    }
    return result;
}

Type floatLiteralType(const std::string& value, const Type* expected) {
    if (value.ends_with("f32")) return makePrimitive(TypeKind::FLOAT32);
    if (value.ends_with("f64")) return makePrimitive(TypeKind::FLOAT64);
    if (expected != nullptr && isFloat(*expected)) return *expected;
    return makePrimitive(TypeKind::FLOAT64);
}

bool isBuiltinFunctionName(const std::string& name) {
    return name == "print" || name == "input" || name == "len" ||
           name == "exit" || name == "panic" || name == "assert" ||
           name == "code" || name == "char_from";
}

bool isExplicitCastAllowed(const Type& from, const Type& to) {
    if (from.kind == TypeKind::ERROR || to.kind == TypeKind::ERROR) return true;
    if (typeEquals(from, to)) return true;

    if (isArithmetic(from) && isArithmetic(to)) return true;
    if (isArithmetic(from) && isString(to)) return true;
    if (isString(from) && isArithmetic(to)) return true;
    if (isBool(from) && isInteger(to)) return true;
    if (isInteger(from) && isBool(to)) return true;

    return false;
}

const StructSymbol* findStructForType(const Scope& scope, const Type& type) {
    if (type.kind != TypeKind::STRUCT) return nullptr;

    const std::string& struct_name =
        std::get<StructTypeInfo>(type.data).struct_name;
    auto parts = splitQualifiedName(struct_name);
    if (parts.empty()) return nullptr;

    auto found = scope.findByPath(parts);
    if (!found) return nullptr;

    if (auto structure = std::get_if<const StructSymbol*>(&found.value())) {
        return *structure;
    }

    return nullptr;
}

const StructSymbol* findStructByQualifiedName(const Scope& scope,
                                              const std::string& qname) {
    auto found = scope.findByPath(splitQualifiedName(qname));
    if (!found) return nullptr;

    if (const auto* structure =
            std::get_if<const StructSymbol*>(&found.value())) {
        return *structure;
    }

    return nullptr;
}

bool typeHasInfiniteSize(const Type& type, const Scope& scope,
                         std::unordered_set<std::string>& on_path) {
    if (type.kind == TypeKind::ARRAY) {
        const auto& array = std::get<ArrayTypeInfo>(type.data);
        return typeHasInfiniteSize(*array.element_type, scope, on_path);
    }

    if (type.kind != TypeKind::STRUCT) return false;

    const std::string& qname = std::get<StructTypeInfo>(type.data).struct_name;
    if (!on_path.insert(qname).second) return true;

    const StructSymbol* structure = findStructByQualifiedName(scope, qname);
    if (structure == nullptr) {
        on_path.erase(qname);
        return false;
    }

    for (const auto& field : structure->fields) {
        if (typeHasInfiniteSize(field.type, scope, on_path)) {
            on_path.erase(qname);
            return true;
        }
    }

    on_path.erase(qname);
    return false;
}

bool structHasInfiniteSize(const StructSymbol& structure, const Scope& scope) {
    std::unordered_set<std::string> on_path;
    on_path.insert(structure.qualified_name);

    for (const auto& field : structure.fields) {
        if (typeHasInfiniteSize(field.type, scope, on_path)) return true;
    }

    return false;
}

bool isEqualityComparableType(const Type& type, const Scope& scope,
                              std::unordered_set<std::string>& visiting) {
    if (type.kind == TypeKind::ERROR) return true;

    if (isArithmetic(type) || isBool(type) || type.kind == TypeKind::CHAR ||
        isString(type)) {
        return true;
    }

    if (type.kind == TypeKind::ARRAY) {
        const auto& array = std::get<ArrayTypeInfo>(type.data);
        return isEqualityComparableType(*array.element_type, scope, visiting);
    }

    if (type.kind == TypeKind::STRUCT) {
        const std::string& struct_name =
            std::get<StructTypeInfo>(type.data).struct_name;
        if (!visiting.insert(struct_name).second) return false;

        const StructSymbol* structure = findStructForType(scope, type);
        if (structure == nullptr) {
            visiting.erase(struct_name);
            return false;
        }

        for (const auto& field : structure->fields) {
            if (!isEqualityComparableType(field.type, scope, visiting)) {
                visiting.erase(struct_name);
                return false;
            }
        }

        visiting.erase(struct_name);
        return true;
    }

    return false;
}

bool canCompareEquality(const Type& left, const Type& right,
                        const Scope& scope) {
    if (left.kind == TypeKind::ERROR || right.kind == TypeKind::ERROR) {
        return true;
    }

    if (isArithmetic(left) && isArithmetic(right)) {
        return getCommonType(left, right).has_value();
    }

    if (!typeEquals(left, right)) return false;

    std::unordered_set<std::string> visiting;
    return isEqualityComparableType(left, scope, visiting);
}

bool isComparisonOp(TokenType op) {
    return op == TokenType::EQUAL || op == TokenType::NOT_EQUAL ||
           op == TokenType::LESS || op == TokenType::GREATER ||
           op == TokenType::LESS_EQUAL || op == TokenType::GREATER_EQUAL;
}

bool hasErrorType(const std::vector<Type>& types) {
    for (const auto& type : types) {
        if (type.kind == TypeKind::ERROR) return true;
    }
    return false;
}

}  // namespace

Semantic::Semantic(const Parser::Program& program, const std::string& filename)
    : m_program(program),
      m_filename(filename),
      m_global(std::make_unique<Scope>()) {}

std::expected<TypedProgram, std::vector<std::string>> Semantic::analyze() {
    collectTopLevel(m_program, *m_global);
    checkRecursiveValueStructs();

    auto mains = m_global->findFunctions("main");
    if (mains.empty()) {
        m_errors.push_back(m_filename +
                           ":1:1: error: function 'main' is not defined");
    } else if (mains.size() != 1) {
        m_errors.push_back(m_filename +
                           ":1:1: error: function 'main' cannot be overloaded");
    } else {
        const FunctionSignature* main = mains.front();
        if (!main->param_types.empty()) {
            error(main->loc, "function 'main' must not have parameters");
        }
        if (!typeEquals(main->return_type, makePrimitive(TypeKind::INT64))) {
            error(main->loc, "function 'main' must return int");
        }
    }

    m_current_scope = m_global.get();
    analyzeProgram();

    if (!m_errors.empty()) return std::unexpected(m_errors);

    return TypedProgram{.program = &m_program,
                        .expr_types = std::move(m_expr_types),
                        .decl_types = std::move(m_decl_types),
                        .call_targets = std::move(m_call_targets),
                        .global_scope = std::move(m_global)};
}

std::expected<Type, std::string> Semantic::resolveTypeExpr(
    const Parser::TypeExpr& te, const Scope& scope) {
    return std::visit(
        overloaded{
            [&](const Parser::SimpleType& simple)
                -> std::expected<Type, std::string> {
                if (auto kind = builtinTypeKind(simple.name)) {
                    return makePrimitive(*kind);
                }

                if (const auto* alias = scope.findTypeAlias(simple.name)) {
                    return resolveTypeAlias(*alias, scope);
                }

                if (const auto* structure = scope.findStruct(simple.name)) {
                    return makeStruct(structure->qualified_name);
                }

                return std::unexpected("unknown type '" + simple.name + "'");
            },
            [&](const Parser::QualifiedType& qualified)
                -> std::expected<Type, std::string> {
                auto found = scope.findByPath(qualified.parts);
                if (!found) return std::unexpected(found.error());

                if (const auto* alias =
                        std::get_if<const TypeAliasSymbol*>(&found.value())) {
                    return resolveTypeAlias(**alias, scope);
                }

                if (const auto* structure =
                        std::get_if<const StructSymbol*>(&found.value())) {
                    return makeStruct((*structure)->qualified_name);
                }

                return std::unexpected("'" + joinName(qualified.parts) +
                                       "' is not a type");
            },
            [&](const std::unique_ptr<Parser::ArrayType>& array)
                -> std::expected<Type, std::string> {
                auto size = parseArraySizeLiteral(array->size);
                if (!size) return std::unexpected(size.error());

                auto element = resolveTypeExpr(array->element, scope);
                if (!element) return std::unexpected(element.error());
                if (element->kind == TypeKind::VOID) {
                    return std::unexpected("array element type cannot be void");
                }
                return makeArray(*size, *element);
            },
        },
        te.node);
}

std::expected<Type, std::string> Semantic::resolveTypeAlias(
    const TypeAliasSymbol& alias, const Scope& scope) {
    if (alias.decl == nullptr || alias.target_type.kind != TypeKind::ERROR) {
        return alias.target_type;
    }

    if (m_resolving_aliases.contains(&alias)) {
        return std::unexpected("cyclic type alias '" + alias.name + "'");
    }

    m_resolving_aliases.insert(&alias);
    auto target = resolveTypeExpr(alias.decl->type, scope);
    m_resolving_aliases.erase(&alias);
    return target;
}

void Semantic::collectTopLevel(const Parser::Program& p, Scope& scope) {
    std::function<void(const std::vector<Parser::Decl>&, Scope&,
                       std::vector<std::string>)>
        predeclare = [&](const std::vector<Parser::Decl>& declarations,
                         Scope& target, std::vector<std::string> path) {
            for (const auto& decl : declarations) {
                std::visit(
                    overloaded{
                        [&](const std::unique_ptr<Parser::FunctionDecl>&) {},
                        [&](const std::unique_ptr<Parser::StructDecl>& st) {
                            std::string qualified =
                                joinName(appendName(path, st->name));
                            if (!target.declareStruct(StructSymbol{
                                    .name = st->name,
                                    .qualified_name = std::move(qualified),
                                    .fields = {},
                                    .loc = decl.loc})) {
                                error(
                                    decl.loc,
                                    "symbol '" + st->name +
                                        "' is already declared in this scope");
                            }
                        },
                        [&](const std::unique_ptr<Parser::NamespaceDecl>& ns) {
                            auto ns_scope = std::make_unique<Scope>(&target);
                            Scope* raw_scope = ns_scope.get();
                            if (!target.declareNamespace(ns->name,
                                                         std::move(ns_scope))) {
                                error(
                                    decl.loc,
                                    "symbol '" + ns->name +
                                        "' is already declared in this scope");
                                return;
                            }
                            predeclare(ns->declarations, *raw_scope,
                                       appendName(path, ns->name));
                        },
                        [&](const std::unique_ptr<Parser::TypeAliasDecl>&
                                alias) {
                            if (!target.declareTypeAlias(
                                    TypeAliasSymbol{.name = alias->name,
                                                    .target_type = makeError(),
                                                    .decl = alias.get(),
                                                    .loc = decl.loc})) {
                                error(
                                    decl.loc,
                                    "symbol '" + alias->name +
                                        "' is already declared in this scope");
                            }
                        },
                        [&](const std::unique_ptr<Parser::ImplDecl>&) {},
                    },
                    decl.node);
            }
        };

    predeclare(p.declarations, scope, {});

    for (const auto& decl : p.declarations) collectDecl(decl, scope);
}

void Semantic::checkRecursiveValueStructs() {
    std::unordered_set<std::string> reported;

    std::function<void(const std::vector<Parser::Decl>&,
                       std::vector<std::string>)>
        walk = [&](const std::vector<Parser::Decl>& declarations,
                   std::vector<std::string> path) {
            for (const auto& decl : declarations) {
                std::visit(
                    overloaded{
                        [&](const std::unique_ptr<Parser::StructDecl>& st) {
                            std::string qname =
                                joinName(appendName(path, st->name));
                            if (!reported.insert(qname).second) return;

                            const StructSymbol* structure =
                                findStructByQualifiedName(*m_global, qname);
                            if (structure == nullptr) return;

                            if (structHasInfiniteSize(*structure, *m_global)) {
                                error(decl.loc, "recursive value struct '" +
                                                    qname +
                                                    "' has infinite size");
                            }
                        },
                        [&](const std::unique_ptr<Parser::NamespaceDecl>& ns) {
                            walk(ns->declarations, appendName(path, ns->name));
                        },
                        [&](const auto&) {},
                    },
                    decl.node);
            }
        };

    walk(m_program.declarations, {});
}

void Semantic::collectDecl(const Parser::Decl& d, Scope& scope) {
    std::visit(overloaded{
                   [&](const std::unique_ptr<Parser::FunctionDecl>& fn) {
                       collectFunction(*fn, scope, d.loc);
                   },
                   [&](const std::unique_ptr<Parser::StructDecl>& st) {
                       collectStruct(*st, scope, d.loc);
                   },
                   [&](const std::unique_ptr<Parser::NamespaceDecl>& ns) {
                       collectNamespace(*ns, scope, d.loc);
                   },
                   [&](const std::unique_ptr<Parser::TypeAliasDecl>& alias) {
                       collectTypeAlias(*alias, scope, d.loc);
                   },
                   [&](const std::unique_ptr<Parser::ImplDecl>& impl) {
                       collectImpl(*impl, scope, d.loc);
                   },
               },
               d.node);
}

void Semantic::collectStruct(const Parser::StructDecl& s, Scope& scope,
                             Parser::NodeLocation loc) {
    std::vector<StructFieldSymbol> fields;
    std::unordered_set<std::string> names;

    for (const auto& field : s.fields) {
        if (!names.insert(field.name).second) {
            error(field.loc, "duplicate field '" + field.name + "'");
            continue;
        }

        auto field_type = resolveTypeExpr(field.type, scope);
        if (!field_type) {
            error(field.type.loc, field_type.error());
            fields.push_back(StructFieldSymbol{.name = field.name,
                                               .type = makeError(),
                                               .is_public = field.is_public});
            continue;
        }
        if (field_type->kind == TypeKind::VOID) {
            error(field.type.loc,
                  "field '" + field.name + "' cannot have type void");
            fields.push_back(StructFieldSymbol{.name = field.name,
                                               .type = makeError(),
                                               .is_public = field.is_public});
            continue;
        }

        fields.push_back(StructFieldSymbol{.name = field.name,
                                           .type = *field_type,
                                           .is_public = field.is_public});
    }

    if (!scope.defineStruct(s.name, std::move(fields))) {
        error(loc,
              "internal error: struct '" + s.name + "' was not predeclared");
    }
}

void Semantic::collectTypeAlias(const Parser::TypeAliasDecl& ta, Scope& scope,
                                Parser::NodeLocation loc) {
    auto target = resolveTypeExpr(ta.type, scope);
    if (!target) {
        error(ta.type.loc, target.error());
        return;
    }
    if (target->kind == TypeKind::VOID) {
        error(ta.type.loc, "type alias '" + ta.name + "' cannot target void");
        return;
    }

    if (!scope.defineTypeAlias(ta.name, *target)) {
        error(loc, "internal error: type alias '" + ta.name +
                       "' was not predeclared");
    }
}

void Semantic::collectNamespace(const Parser::NamespaceDecl& ns, Scope& scope,
                                Parser::NodeLocation loc) {
    Scope* ns_scope = scope.findNamespaceMutable(ns.name);
    if (ns_scope == nullptr) {
        error(loc, "internal error: namespace '" + ns.name +
                       "' was not predeclared");
        return;
    }

    m_current_namespace.push_back(ns.name);
    for (const auto& decl : ns.declarations) collectDecl(decl, *ns_scope);
    m_current_namespace.pop_back();
}

void Semantic::collectFunction(const Parser::FunctionDecl& fn, Scope& scope,
                               Parser::NodeLocation loc) {
    if (isBuiltinFunctionName(fn.name)) {
        error(loc, "cannot redeclare builtin function '" + fn.name + "'");
        return;
    }

    std::vector<Type> param_types;
    std::vector<std::string> param_names;
    param_types.reserve(fn.params.size());
    param_names.reserve(fn.params.size());

    for (const auto& param : fn.params) {
        auto param_type = resolveTypeExpr(param.type, scope);
        if (!param_type) {
            error(param.type.loc, param_type.error());
            param_types.push_back(makeError());
        } else if (param_type->kind == TypeKind::VOID) {
            error(param.type.loc,
                  "parameter '" + param.name + "' cannot have type void");
            param_types.push_back(makeError());
        } else {
            param_types.push_back(*param_type);
        }
        param_names.push_back(param.name);
    }

    auto return_type = resolveTypeExpr(fn.return_type, scope);
    if (!return_type) {
        error(fn.return_type.loc, return_type.error());
        return_type = makeError();
    }

    FunctionSignature sig{.name = fn.name,
                          .namespace_qname = joinName(m_current_namespace),
                          .enclosing_struct_qname = "",
                          .is_public = fn.is_public,
                          .param_types = std::move(param_types),
                          .param_names = std::move(param_names),
                          .return_type = *return_type,
                          .loc = loc,
                          .decl = &fn};

    if (!scope.declareFunction(std::move(sig))) {
        error(loc, "duplicate function overload '" + fn.name + "'");
    }
}

void Semantic::collectImpl(const Parser::ImplDecl& impl, Scope& scope,
                           Parser::NodeLocation loc) {
    const auto* structure = scope.findLocalStruct(impl.struct_name);
    if (structure == nullptr) {
        if (scope.findStruct(impl.struct_name) != nullptr) {
            error(loc,
                  "impl for struct '" + impl.struct_name +
                      "' must be declared in the same scope as the struct");
        } else {
            error(loc, "impl target struct '" + impl.struct_name +
                           "' is not declared");
        }
        return;
    }

    if (!m_impl_structs.insert(structure->qualified_name).second) {
        error(loc,
              "duplicate impl for struct '" + structure->qualified_name + "'");
        return;
    }

    Type self_type = makeStruct(structure->qualified_name);
    for (const auto& method : impl.methods) {
        if (method->params.empty()) {
            error(loc,
                  "method '" + method->name + "' must have self parameter");
            continue;
        }

        const auto& self = method->params.front();
        auto resolved_self = resolveTypeExpr(self.type, scope);
        if (self.name != "self" || !resolved_self ||
            !typeEquals(*resolved_self, self_type)) {
            error(self.loc, "first method parameter must be 'self: " +
                                impl.struct_name + "'");
        }

        std::vector<Type> param_types;
        std::vector<std::string> param_names;
        param_types.reserve(method->params.size());
        param_names.reserve(method->params.size());

        for (const auto& param : method->params) {
            auto param_type = resolveTypeExpr(param.type, scope);
            if (!param_type) {
                error(param.type.loc, param_type.error());
                param_types.push_back(makeError());
            } else if (param_type->kind == TypeKind::VOID) {
                error(param.type.loc,
                      "parameter '" + param.name + "' cannot have type void");
                param_types.push_back(makeError());
            } else {
                param_types.push_back(*param_type);
            }
            param_names.push_back(param.name);
        }

        auto return_type = resolveTypeExpr(method->return_type, scope);
        if (!return_type) {
            error(method->return_type.loc, return_type.error());
            return_type = makeError();
        }

        FunctionSignature sig{
            .name = method->name,
            .namespace_qname = joinName(m_current_namespace),
            .enclosing_struct_qname = structure->qualified_name,
            .is_public = method->is_public,
            .param_types = std::move(param_types),
            .param_names = std::move(param_names),
            .return_type = *return_type,
            .loc = loc,
            .decl = method.get()};

        if (!scope.declareFunction(std::move(sig))) {
            error(loc, "duplicate method overload '" + structure->name +
                           "::" + method->name + "'");
        }
    }
}

void Semantic::analyzeProgram() {
    for (const auto& decl : m_program.declarations) {
        std::visit(overloaded{
                       [&](const std::unique_ptr<Parser::FunctionDecl>& fn) {
                           analyzeFunction(*fn, *m_global);
                       },
                       [&](const std::unique_ptr<Parser::StructDecl>&) {},
                       [&](const std::unique_ptr<Parser::NamespaceDecl>& ns) {
                           analyzeNamespace(*ns);
                       },
                       [&](const std::unique_ptr<Parser::TypeAliasDecl>&) {},
                       [&](const std::unique_ptr<Parser::ImplDecl>& impl) {
                           analyzeImpl(*impl);
                       },
                   },
                   decl.node);
    }
}

void Semantic::analyzeNamespace(const Parser::NamespaceDecl& ns) {
    const Scope* found = m_current_scope->findNamespace(ns.name);
    if (found == nullptr) return;

    Scope* ns_scope = const_cast<Scope*>(found);
    Scope* saved = m_current_scope;
    m_current_scope = ns_scope;

    for (const auto& decl : ns.declarations) {
        std::visit(
            overloaded{
                [&](const std::unique_ptr<Parser::FunctionDecl>& fn) {
                    analyzeFunction(*fn, *ns_scope);
                },
                [&](const std::unique_ptr<Parser::StructDecl>&) {},
                [&](const std::unique_ptr<Parser::NamespaceDecl>& inner) {
                    analyzeNamespace(*inner);
                },
                [&](const std::unique_ptr<Parser::TypeAliasDecl>&) {},
                [&](const std::unique_ptr<Parser::ImplDecl>& impl) {
                    analyzeImpl(*impl);
                },
            },
            decl.node);
    }

    m_current_scope = saved;
}

void Semantic::analyzeImpl(const Parser::ImplDecl& impl) {
    const StructSymbol* structure =
        m_current_scope->findStruct(impl.struct_name);
    const std::string saved = m_current_impl_struct;
    if (structure != nullptr) {
        m_current_impl_struct = structure->qualified_name;
    }

    for (const auto& method : impl.methods) {
        analyzeFunction(*method, *m_current_scope);
    }

    m_current_impl_struct = saved;
}

void Semantic::analyzeFunction(const Parser::FunctionDecl& fn, Scope& parent) {
    Scope fn_scope(&parent);

    for (const auto& param : fn.params) {
        auto param_type = resolveTypeExpr(param.type, parent);
        Type type = param_type ? *param_type : makeError();
        if (!param_type) error(param.type.loc, param_type.error());
        if (type.kind == TypeKind::VOID) {
            type = makeError();
        }

        if (!fn_scope.declareVariable(VariableSymbol{.name = param.name,
                                                     .type = type,
                                                     .is_mutable = false,
                                                     .loc = param.loc})) {
            error(param.loc, "duplicate parameter '" + param.name + "'");
        }
        recordDeclType(&param, type);
    }

    auto return_type = resolveTypeExpr(fn.return_type, parent);
    Type saved_return = m_current_return_type;
    Scope* saved_scope = m_current_scope;

    m_current_return_type = return_type ? *return_type : makeError();
    if (!return_type) error(fn.return_type.loc, return_type.error());
    m_current_scope = &fn_scope;

    analyzeBlock(fn.body);

    if (m_current_return_type.kind != TypeKind::VOID &&
        m_current_return_type.kind != TypeKind::ERROR &&
        !allPathsReturn(fn.body)) {
        error(fn.return_type.loc, "not all control paths return a value");
    }

    m_current_scope = saved_scope;
    m_current_return_type = saved_return;
}

void Semantic::analyzeBlock(const Parser::Block& b) {
    Scope block_scope(m_current_scope);
    Scope* saved = m_current_scope;
    m_current_scope = &block_scope;

    for (const auto& stmt : b.statements) analyzeStmt(stmt);

    m_current_scope = saved;
}

void Semantic::analyzeStmt(const Parser::Stmt& s) {
    std::visit(
        overloaded{
            [&](const std::unique_ptr<Parser::LetStmt>& l) {
                analyzeLet(*l, s.loc);
            },
            [&](const std::unique_ptr<Parser::AssignStmt>& a) {
                analyzeAssign(*a, s.loc);
            },
            [&](const std::unique_ptr<Parser::IfStmt>& i) {
                analyzeIf(*i, s.loc);
            },
            [&](const std::unique_ptr<Parser::WhileStmt>& w) {
                analyzeWhile(*w, s.loc);
            },
            [&](const std::unique_ptr<Parser::ReturnStmt>& r) {
                analyzeReturn(*r, s.loc);
            },
            [&](const std::unique_ptr<Parser::ExprStmt>& e) {
                analyzeExprStmt(*e, s.loc);
            },
            [&](const std::unique_ptr<Parser::Block>& b) { analyzeBlock(*b); },
            [&](const Parser::BreakStmt&) { analyzeBreak(s.loc); },
            [&](const Parser::ContinueStmt&) { analyzeContinue(s.loc); },
            [&](const Parser::NullStmt&) {},
        },
        s.node);
}

void Semantic::analyzeLet(const Parser::LetStmt& l, Parser::NodeLocation loc) {
    Type declared = makeError();
    Type init_type = makeError();

    if (l.type) {
        auto resolved = resolveTypeExpr(*l.type, *m_current_scope);
        if (!resolved) {
            error(l.type->loc, resolved.error());
            init_type = checkExpr(l.init);
        } else if (resolved->kind == TypeKind::VOID) {
            error(l.type->loc,
                  "variable '" + l.name + "' cannot have type void");
            init_type = checkExpr(l.init);
        } else {
            declared = *resolved;
            init_type = checkExpr(l.init, &declared);
            if (!isConvertibleTo(init_type, declared)) {
                error(loc, "cannot initialize variable '" + l.name +
                               "' of type " + typeToString(declared) +
                               " with value of type " +
                               typeToString(init_type));
            }
        }
    } else {
        init_type = checkExpr(l.init);
        declared = init_type;
        if (declared.kind == TypeKind::VOID) {
            error(loc, "variable '" + l.name + "' cannot have type void");
            declared = makeError();
        }
    }

    if (!m_current_scope->declareVariable(
            VariableSymbol{.name = l.name,
                           .type = declared,
                           .is_mutable = l.is_mutable,
                           .loc = loc})) {
        error(loc,
              "variable '" + l.name + "' is already declared in this scope");
    }

    recordDeclType(&l, declared);
}

void Semantic::analyzeAssign(const Parser::AssignStmt& a,
                             Parser::NodeLocation loc) {
    Type target_type = checkExpr(a.target);
    if (!isLvalue(a.target)) error(loc, "assignment target is not an lvalue");
    if (!isMutableLvalue(a.target)) error(loc, "assignment to immutable value");

    Type value_type = checkExpr(a.value, &target_type);
    if (!isConvertibleTo(value_type, target_type)) {
        error(loc, "cannot assign value of type " + typeToString(value_type) +
                       " to target of type " + typeToString(target_type));
    }
}

void Semantic::analyzeIf(const Parser::IfStmt& i, Parser::NodeLocation loc) {
    Type cond = checkExpr(i.condition);
    if (cond.kind != TypeKind::ERROR && !isBool(cond)) {
        error(loc, "if condition must have type bool");
    }

    analyzeBlock(i.then_block);
    if (i.else_branch) analyzeStmt(**i.else_branch);
}

void Semantic::analyzeWhile(const Parser::WhileStmt& w,
                            Parser::NodeLocation loc) {
    Type cond = checkExpr(w.condition);
    if (cond.kind != TypeKind::ERROR && !isBool(cond)) {
        error(loc, "while condition must have type bool");
    }

    ++m_loop_depth;
    analyzeBlock(w.body);
    --m_loop_depth;
}

void Semantic::analyzeReturn(const Parser::ReturnStmt& r,
                             Parser::NodeLocation loc) {
    if (r.value) {
        Type value_type = checkExpr(*r.value, &m_current_return_type);
        if (m_current_return_type.kind == TypeKind::VOID) {
            error(loc, "void function cannot return a value");
        } else if (!isConvertibleTo(value_type, m_current_return_type)) {
            error(loc, "cannot return value of type " +
                           typeToString(value_type) +
                           " from function returning " +
                           typeToString(m_current_return_type));
        }
        return;
    }

    if (m_current_return_type.kind != TypeKind::VOID &&
        m_current_return_type.kind != TypeKind::ERROR) {
        error(loc, "non-void function must return a value");
    }
}

void Semantic::analyzeBreak(Parser::NodeLocation loc) {
    if (m_loop_depth == 0) error(loc, "break outside loop");
}

void Semantic::analyzeContinue(Parser::NodeLocation loc) {
    if (m_loop_depth == 0) error(loc, "continue outside loop");
}

void Semantic::analyzeExprStmt(const Parser::ExprStmt& e,
                               Parser::NodeLocation) {
    checkExpr(e.expression);
}

Type Semantic::checkExpr(const Parser::Expr& e, const Type* expected) {
    Type result = std::visit(
        overloaded{
            [&](const Parser::IntLiteral& lit) {
                auto type = intLiteralType(lit.value, expected, false);
                if (!type) {
                    error(e.loc, type.error());
                    return makeError();
                }
                return *type;
            },
            [&](const Parser::FloatLiteral& lit) {
                return floatLiteralType(lit.value, expected);
            },
            [&](const Parser::CharLiteral&) {
                return makePrimitive(TypeKind::CHAR);
            },
            [&](const Parser::StringLiteral&) {
                return makePrimitive(TypeKind::STRING);
            },
            [&](const Parser::BoolLiteral&) {
                return makePrimitive(TypeKind::BOOL);
            },
            [&](const Parser::Identifier& id) {
                return checkIdentifier(id, e.loc);
            },
            [&](const std::unique_ptr<Parser::BinaryExpr>& b) {
                return checkBinary(*b, e.loc);
            },
            [&](const std::unique_ptr<Parser::UnaryExpr>& u) {
                return checkUnary(*u, e.loc, expected);
            },
            [&](const std::unique_ptr<Parser::CallExpr>& c) {
                return checkCall(*c, e.loc);
            },
            [&](const std::unique_ptr<Parser::MethodCall>& c) {
                return checkMethodCall(*c, e.loc);
            },
            [&](const std::unique_ptr<Parser::IndexExpr>& ix) {
                return checkIndex(*ix, e.loc);
            },
            [&](const std::unique_ptr<Parser::FieldAccess>& fa) {
                return checkFieldAccess(*fa, e.loc);
            },
            [&](const std::unique_ptr<Parser::CastExpr>& c) {
                return checkCast(*c, e.loc);
            },
            [&](const std::unique_ptr<Parser::ArrayLiteral>& al) {
                return checkArrayLit(*al, e.loc, expected);
            },
            [&](const std::unique_ptr<Parser::StructLiteral>& sl) {
                return checkStructLit(*sl, e.loc);
            },
        },
        e.node);

    recordExprType(e, result);
    return result;
}

Type Semantic::checkBinary(const Parser::BinaryExpr& b,
                           Parser::NodeLocation loc) {
    Type left = checkExpr(b.left);
    Type right = checkExpr(b.right);
    if (left.kind == TypeKind::ERROR || right.kind == TypeKind::ERROR) {
        return makeError();
    }

    if (b.op == TokenType::PLUS && isString(left) && isString(right)) {
        return makePrimitive(TypeKind::STRING);
    }

    if (b.op == TokenType::PLUS || b.op == TokenType::MINUS ||
        b.op == TokenType::STAR || b.op == TokenType::SLASH) {
        if (!isArithmetic(left) || !isArithmetic(right)) {
            error(loc, "arithmetic operands must be numeric");
            return makeError();
        }

        auto common = getCommonType(left, right);
        if (!common) {
            error(loc, "no common numeric type for " + typeToString(left) +
                           " and " + typeToString(right));
            return makeError();
        }
        return *common;
    }

    if (b.op == TokenType::PERCENT) {
        if (!isInteger(left) || !isInteger(right)) {
            error(loc, "operator '%' operands must be integer");
            return makeError();
        }

        auto common = getCommonType(left, right);
        if (!common || !isInteger(*common)) {
            error(loc, "no common integer type for " + typeToString(left) +
                           " and " + typeToString(right));
            return makeError();
        }
        return *common;
    }

    if (b.op == TokenType::EQUAL || b.op == TokenType::NOT_EQUAL) {
        if (!canCompareEquality(left, right, *m_current_scope)) {
            error(loc, "cannot compare " + typeToString(left) + " and " +
                           typeToString(right));
            return makeError();
        }
        return makePrimitive(TypeKind::BOOL);
    }

    if (isComparisonOp(b.op)) {
        if (!isArithmetic(left) || !isArithmetic(right)) {
            error(loc, "ordered comparison operands must be numeric");
            return makeError();
        }

        if (!getCommonType(left, right)) {
            error(loc, "no common numeric type for " + typeToString(left) +
                           " and " + typeToString(right));
            return makeError();
        }

        return makePrimitive(TypeKind::BOOL);
    }

    if (b.op == TokenType::AND_AND || b.op == TokenType::OR_OR) {
        if (!isBool(left) || !isBool(right)) {
            error(loc, "logical operands must have type bool");
            return makeError();
        }
        return makePrimitive(TypeKind::BOOL);
    }

    error(loc, "unsupported binary operator");
    return makeError();
}

Type Semantic::checkUnary(const Parser::UnaryExpr& u, Parser::NodeLocation loc,
                          const Type* expected) {
    if (u.op == TokenType::MINUS) {
        if (const auto* literal =
                std::get_if<Parser::IntLiteral>(&u.operand.node)) {
            auto type = intLiteralType(literal->value, expected, true);
            if (!type) {
                error(u.operand.loc, type.error());
                recordExprType(u.operand, makeError());
                return makeError();
            }
            recordExprType(u.operand, *type);
            return *type;
        }

        Type operand = checkExpr(u.operand, expected);
        if (operand.kind == TypeKind::ERROR) return makeError();
        if (!isArithmetic(operand)) {
            error(loc, "unary '-' operand must be numeric");
            return makeError();
        }
        return operand;
    }

    if (u.op == TokenType::NOT) {
        Type operand = checkExpr(u.operand);
        if (operand.kind == TypeKind::ERROR) return makeError();
        if (!isBool(operand)) {
            error(loc, "unary '!' operand must have type bool");
            return makeError();
        }
        return makePrimitive(TypeKind::BOOL);
    }

    error(loc, "unsupported unary operator");
    return makeError();
}

Type Semantic::checkCall(const Parser::CallExpr& c, Parser::NodeLocation loc) {
    if (auto builtin = tryBuiltinCall(c, loc)) return *builtin;

    std::vector<Type> arg_types;
    arg_types.reserve(c.args.size());
    for (const auto& arg : c.args) arg_types.push_back(checkExpr(arg));
    if (hasErrorType(arg_types)) return makeError();

    std::vector<const FunctionSignature*> overloads;
    if (c.name.parts.size() == 1) {
        overloads = m_current_scope->findFunctions(c.name.parts[0]);
    } else {
        auto found = m_current_scope->findByPath(c.name.parts);
        if (found) {
            if (auto functions =
                    std::get_if<std::vector<const FunctionSignature*>>(
                        &found.value())) {
                overloads = *functions;
            }
        }

        if (overloads.empty()) {
            overloads =
                findQualifiedMethodOverloads(*m_current_scope, c.name.parts);
        }
    }

    if (overloads.empty()) {
        error(loc, "unknown function '" + joinName(c.name.parts) + "'");
        return makeError();
    }

    OverloadResolution resolved = resolveOverload(overloads, arg_types);
    if (resolved.status == OverloadResult::OK) {
        if (!resolved.chosen->enclosing_struct_qname.empty() &&
            !resolved.chosen->is_public &&
            resolved.chosen->enclosing_struct_qname !=
                m_current_impl_struct) {
            error(loc, "method '" + joinName(c.name.parts) + "' is private");
            return makeError();
        }

        m_call_targets[&c] = resolved.chosen;
        return resolved.chosen->return_type;
    }

    if (resolved.status == OverloadResult::AMBIGUOUS) {
        error(loc, "ambiguous call to '" + joinName(c.name.parts) + "'");
    } else {
        error(loc, "no matching overload for '" + joinName(c.name.parts) + "'");
    }

    return makeError();
}

Type Semantic::checkMethodCall(const Parser::MethodCall& c,
                               Parser::NodeLocation loc) {
    Type object_type = checkExpr(c.object);

    std::vector<Type> arg_types;
    arg_types.reserve(c.args.size() + 1);
    arg_types.push_back(object_type);
    for (const auto& arg : c.args) arg_types.push_back(checkExpr(arg));

    if (hasErrorType(arg_types)) return makeError();

    if (object_type.kind != TypeKind::STRUCT) {
        error(loc, "method call requires struct type");
        return makeError();
    }

    const std::string& struct_name =
        std::get<StructTypeInfo>(object_type.data).struct_name;
    auto overloads =
        findMethodOverloads(*m_current_scope, struct_name, c.method);
    if (overloads.empty()) {
        error(loc, "unknown method '" + c.method + "' for type " +
                       typeToString(object_type));
        return makeError();
    }

    OverloadResolution resolved = resolveOverload(overloads, arg_types);
    if (resolved.status == OverloadResult::OK) {
        if (!resolved.chosen->is_public &&
            struct_name != m_current_impl_struct) {
            error(loc, "method '" + c.method + "' of struct '" +
                           struct_name + "' is private");
            return makeError();
        }

        m_call_targets[&c] = resolved.chosen;
        return resolved.chosen->return_type;
    }

    if (resolved.status == OverloadResult::AMBIGUOUS) {
        error(loc, "ambiguous method call '" + c.method + "' for type " +
                       typeToString(object_type));
    } else {
        error(loc, "no matching overload for method '" + c.method +
                       "' on type " + typeToString(object_type));
    }

    return makeError();
}

Type Semantic::checkIndex(const Parser::IndexExpr& ix,
                          Parser::NodeLocation loc) {
    Type object = checkExpr(ix.object);
    Type index = checkExpr(ix.index);
    if (object.kind == TypeKind::ERROR || index.kind == TypeKind::ERROR) {
        return makeError();
    }

    if (object.kind != TypeKind::ARRAY) {
        error(loc, "indexing requires array type");
        return makeError();
    }

    if (!isInteger(index)) {
        error(loc, "array index must be integer");
        return makeError();
    }

    return *std::get<ArrayTypeInfo>(object.data).element_type;
}

Type Semantic::checkFieldAccess(const Parser::FieldAccess& fa,
                                Parser::NodeLocation loc) {
    Type object = checkExpr(fa.object);
    if (object.kind == TypeKind::ERROR) return makeError();

    if (object.kind != TypeKind::STRUCT) {
        error(loc, "field access requires struct type");
        return makeError();
    }

    const std::string& struct_name =
        std::get<StructTypeInfo>(object.data).struct_name;

    const StructSymbol* structure = nullptr;
    if (struct_name.find("::") == std::string::npos) {
        structure = m_current_scope->findStruct(struct_name);
    } else {
        auto found =
            m_current_scope->findByPath(splitQualifiedName(struct_name));
        if (found) {
            if (auto symbol =
                    std::get_if<const StructSymbol*>(&found.value())) {
                structure = *symbol;
            }
        }
    }

    if (structure == nullptr) {
        error(loc, "unknown struct type '" + struct_name + "'");
        return makeError();
    }

    for (const auto& field : structure->fields) {
        if (field.name != fa.field) continue;

        if (!field.is_public &&
            structure->qualified_name != m_current_impl_struct) {
            error(loc, "field '" + fa.field + "' of struct '" +
                           struct_name + "' is private");
        }
        return field.type;
    }

    error(loc, "struct '" + struct_name + "' has no field '" + fa.field + "'");
    return makeError();
}

Type Semantic::checkCast(const Parser::CastExpr& c, Parser::NodeLocation loc) {
    auto target = resolveTypeExpr(c.target_type, *m_current_scope);
    if (!target) {
        error(c.target_type.loc, target.error());
        return makeError();
    }
    if (target->kind == TypeKind::VOID) {
        error(c.target_type.loc, "cast target type cannot be void");
        return makeError();
    }

    Type value = checkExpr(c.value);
    if (!isExplicitCastAllowed(value, *target)) {
        error(loc, "invalid cast from " + typeToString(value) + " to " +
                       typeToString(*target));
        return makeError();
    }

    return *target;
}

Type Semantic::checkArrayLit(const Parser::ArrayLiteral& al,
                             Parser::NodeLocation loc, const Type* expected) {
    if (expected != nullptr && expected->kind == TypeKind::ARRAY) {
        const auto& expected_array = std::get<ArrayTypeInfo>(expected->data);
        if (al.elements.size() != expected_array.size) {
            error(loc, "array literal has " +
                           std::to_string(al.elements.size()) +
                           " elements but expected " +
                           std::to_string(expected_array.size));
            return makeError();
        }

        for (const auto& element : al.elements) {
            Type actual = checkExpr(element, expected_array.element_type.get());
            if (!isConvertibleTo(actual, *expected_array.element_type)) {
                error(element.loc,
                      "array element of type " + typeToString(actual) +
                          " cannot initialize element of type " +
                          typeToString(*expected_array.element_type));
                return makeError();
            }
        }

        return *expected;
    }

    if (al.elements.empty()) {
        error(loc, "cannot infer type of empty array literal");
        return makeError();
    }

    Type element_type = checkExpr(al.elements.front());
    for (std::size_t i = 1; i < al.elements.size(); ++i) {
        Type current = checkExpr(al.elements[i]);
        auto common = getCommonType(element_type, current);
        if (!common) {
            error(al.elements[i].loc, "array elements have incompatible types");
            return makeError();
        }
        element_type = *common;
    }

    return makeArray(al.elements.size(), element_type);
}

Type Semantic::checkStructLit(const Parser::StructLiteral& sl,
                              Parser::NodeLocation loc) {
    auto found = m_current_scope->findByPath(sl.name.parts);
    if (!found) {
        error(loc, found.error());
        return makeError();
    }

    const StructSymbol* structure = nullptr;
    if (auto symbol = std::get_if<const StructSymbol*>(&found.value())) {
        structure = *symbol;
    } else {
        error(loc, "'" + joinName(sl.name.parts) + "' is not a struct type");
        return makeError();
    }

    std::unordered_map<std::string, const StructFieldSymbol*> field_types;
    for (const auto& field : structure->fields) {
        field_types[field.name] = &field;
    }

    std::unordered_set<std::string> initialized;
    for (const auto& field : sl.fields) {
        auto expected = field_types.find(field.name);
        if (expected == field_types.end()) {
            error(field.loc, "unknown field '" + field.name + "'");
            continue;
        }

        if (!initialized.insert(field.name).second) {
            error(field.loc,
                  "duplicate field initializer '" + field.name + "'");
            continue;
        }

        if (!expected->second->is_public &&
            structure->qualified_name != m_current_impl_struct) {
            error(field.loc, "cannot initialize private field '" + field.name +
                                 "' of struct '" +
                                 structure->qualified_name +
                                 "' outside its methods");
        }

        Type actual = checkExpr(field.value, &expected->second->type);
        if (!isConvertibleTo(actual, expected->second->type)) {
            error(field.loc, "cannot initialize field '" + field.name +
                                 "' of type " +
                                 typeToString(expected->second->type) +
                                 " with value of type " + typeToString(actual));
        }
    }

    for (const auto& field : structure->fields) {
        if (!initialized.contains(field.name)) {
            error(loc, "missing initializer for field '" + field.name + "'");
        }
    }

    return makeStruct(structure->qualified_name);
}

Type Semantic::checkIdentifier(const Parser::Identifier& id,
                               Parser::NodeLocation loc) {
    auto found = m_current_scope->findByPath(id.parts);
    if (!found) {
        error(loc, found.error());
        return makeError();
    }

    if (auto variable = std::get_if<const VariableSymbol*>(&found.value())) {
        return (*variable)->type;
    }

    if (std::holds_alternative<std::vector<const FunctionSignature*>>(
            found.value())) {
        error(loc, "function '" + joinName(id.parts) +
                       "' cannot be used as a value");
    } else if (std::holds_alternative<const StructSymbol*>(found.value())) {
        error(loc,
              "type '" + joinName(id.parts) + "' cannot be used as a value");
    } else if (std::holds_alternative<const TypeAliasSymbol*>(found.value())) {
        error(loc, "type alias '" + joinName(id.parts) +
                       "' cannot be used as a value");
    } else {
        error(loc, "namespace '" + joinName(id.parts) +
                       "' cannot be used as a value");
    }

    return makeError();
}

std::optional<Type> Semantic::tryBuiltinCall(const Parser::CallExpr& c,
                                             Parser::NodeLocation loc) {
    if (c.name.parts.size() != 1) return std::nullopt;

    const std::string& name = c.name.parts[0];
    if (!isBuiltinFunctionName(name)) return std::nullopt;

    if (name == "print") {
        if (c.args.size() != 1) {
            error(loc, "print expects 1 argument");
            return makeError();
        }
        Type arg = checkExpr(c.args[0]);
        if (arg.kind == TypeKind::VOID) {
            error(loc, "print argument cannot have type void");
            return makeError();
        }
        return makePrimitive(TypeKind::VOID);
    }

    if (name == "input") {
        if (!c.args.empty()) {
            error(loc, "input expects 0 arguments");
            return makeError();
        }
        return makePrimitive(TypeKind::STRING);
    }

    if (name == "len") {
        if (c.args.size() != 1) {
            error(loc, "len expects 1 argument");
            return makeError();
        }
        Type arg = checkExpr(c.args[0]);
        if (!isString(arg) && arg.kind != TypeKind::ARRAY &&
            arg.kind != TypeKind::ERROR) {
            error(loc, "len expects string or array");
            return makeError();
        }
        return makePrimitive(TypeKind::INT64);
    }

    if (name == "code") {
        if (c.args.size() != 1) {
            error(loc, "code expects 1 argument");
            return makeError();
        }

        Type arg = checkExpr(c.args[0]);
        if (arg.kind != TypeKind::CHAR && arg.kind != TypeKind::ERROR) {
            error(loc, "code argument must be char");
            return makeError();
        }

        return makePrimitive(TypeKind::INT64);
    }

    if (name == "char_from") {
        if (c.args.size() != 1) {
            error(loc, "char_from expects 1 argument");
            return makeError();
        }

        Type arg = checkExpr(c.args[0]);
        if (!isInteger(arg) && arg.kind != TypeKind::ERROR) {
            error(loc, "char_from argument must be integer");
            return makeError();
        }

        return makePrimitive(TypeKind::CHAR);
    }

    if (name == "assert") {
        if (c.args.size() != 1) {
            error(loc, "assert expects 1 argument");
            return makeError();
        }

        Type arg = checkExpr(c.args[0]);
        if (!isBool(arg) && arg.kind != TypeKind::ERROR) {
            error(loc, "assert argument must be bool");
            return makeError();
        }

        return makePrimitive(TypeKind::VOID);
    }

    if (name == "exit") {
        if (c.args.size() != 1) {
            error(loc, "exit expects 1 argument");
            return makeError();
        }
        Type arg = checkExpr(c.args[0]);
        if (!isInteger(arg) && arg.kind != TypeKind::ERROR) {
            error(loc, "exit argument must be integer");
            return makeError();
        }
        return makePrimitive(TypeKind::VOID);
    }

    if (name == "panic") {
        if (c.args.size() != 1) {
            error(loc, "panic expects 1 argument");
            return makeError();
        }
        Type arg = checkExpr(c.args[0]);
        if (!isString(arg) && arg.kind != TypeKind::ERROR) {
            error(loc, "panic argument must be string");
            return makeError();
        }
        return makePrimitive(TypeKind::VOID);
    }

    return std::nullopt;
}

bool Semantic::isLvalue(const Parser::Expr& e) {
    return std::visit(
        overloaded{
            [&](const Parser::Identifier&) { return true; },
            [&](const std::unique_ptr<Parser::IndexExpr>&) { return true; },
            [&](const std::unique_ptr<Parser::FieldAccess>&) { return true; },
            [&](const auto&) { return false; },
        },
        e.node);
}

bool Semantic::isMutableLvalue(const Parser::Expr& e) {
    return std::visit(overloaded{
                          [&](const Parser::Identifier& id) {
                              if (id.parts.size() != 1) return false;
                              const auto* variable =
                                  m_current_scope->findVariable(id.parts[0]);
                              return variable != nullptr &&
                                     variable->is_mutable;
                          },
                          [&](const std::unique_ptr<Parser::IndexExpr>& ix) {
                              return isMutableLvalue(ix->object);
                          },
                          [&](const std::unique_ptr<Parser::FieldAccess>& fa) {
                              return isMutableLvalue(fa->object);
                          },
                          [&](const auto&) { return false; },
                      },
                      e.node);
}

bool Semantic::allPathsReturn(const Parser::Block& b) {
    auto isNoReturnCall = [](const Parser::CallExpr& call) {
        return call.name.parts.size() == 1 &&
               (call.name.parts[0] == "exit" || call.name.parts[0] == "panic");
    };

    std::function<bool(const Parser::Expr&)> exprTerminates =
        [&](const Parser::Expr& expr) -> bool {
        return std::visit(
            overloaded{
                [&](const std::unique_ptr<Parser::CallExpr>& call) {
                    return isNoReturnCall(*call);
                },
                [&](const auto&) { return false; },
            },
            expr.node);
    };

    std::function<bool(const Parser::Stmt&)> stmtReturns =
        [&](const Parser::Stmt& stmt) -> bool {
        return std::visit(
            overloaded{
                [&](const std::unique_ptr<Parser::ReturnStmt>&) {
                    return true;
                },
                [&](const std::unique_ptr<Parser::ExprStmt>& expr_stmt) {
                    return exprTerminates(expr_stmt->expression);
                },
                [&](const std::unique_ptr<Parser::Block>& block) {
                    return allPathsReturn(*block);
                },
                [&](const std::unique_ptr<Parser::IfStmt>& if_stmt) {
                    return if_stmt->else_branch.has_value() &&
                           allPathsReturn(if_stmt->then_block) &&
                           stmtReturns(**if_stmt->else_branch);
                },
                [&](const auto&) { return false; },
            },
            stmt.node);
    };

    for (const auto& stmt : b.statements) {
        if (stmtReturns(stmt)) return true;
    }
    return false;
}

void Semantic::error(Parser::NodeLocation loc, const std::string& msg) {
    std::ostringstream out;
    out << m_filename << ':' << loc.line << ':' << loc.col
        << ": error: " << msg;
    m_errors.push_back(out.str());
}

void Semantic::recordExprType(const Parser::Expr& e, Type t) {
    m_expr_types[&e] = std::move(t);
}

void Semantic::recordDeclType(const void* node, Type t) {
    m_decl_types[node] = std::move(t);
}

}  // namespace Semantic

/*
struct Point {
    x: int32;
    y: int32;
}

struct Size {
    width: int32;
    height: int32;
}

struct Rect {
    position: Point;
    size: Size;
}

struct Window {
    bounds: Rect;
    title: string;
}

fn main() -> int {
    return 0;
}
*/
