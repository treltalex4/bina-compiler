module bina.semantic.type;

import std;

namespace Semantic {
namespace {
constexpr std::pair<TypeKind, TypeKind> kWideningEdges[] = {
    {TypeKind::INT8, TypeKind::INT16},
    {TypeKind::INT16, TypeKind::INT32},
    {TypeKind::INT32, TypeKind::INT64},
    {TypeKind::UINT8, TypeKind::UINT16},
    {TypeKind::UINT16, TypeKind::UINT32},
    {TypeKind::UINT32, TypeKind::UINT64},
    {TypeKind::UINT8, TypeKind::INT16},
    {TypeKind::UINT16, TypeKind::INT32},
    {TypeKind::UINT32, TypeKind::INT64},
    {TypeKind::INT32, TypeKind::FLOAT32},
    {TypeKind::INT64, TypeKind::FLOAT64},
    {TypeKind::FLOAT32, TypeKind::FLOAT64},
};

int numericRank(TypeKind kind) {
    switch (kind) {
        case TypeKind::INT8:
        case TypeKind::UINT8:
        case TypeKind::FLOAT32:
            return 0;
        case TypeKind::INT16:
        case TypeKind::UINT16:
        case TypeKind::FLOAT64:
            return 1;
        case TypeKind::INT32:
        case TypeKind::UINT32:
            return 2;
        case TypeKind::INT64:
        case TypeKind::UINT64:
            return 3;
        default:
            return -1;
    }
}

int numericFamily(TypeKind kind) {
    switch (kind) {
        case TypeKind::INT8:
        case TypeKind::INT16:
        case TypeKind::INT32:
        case TypeKind::INT64:
            return 0;
        case TypeKind::UINT8:
        case TypeKind::UINT16:
        case TypeKind::UINT32:
        case TypeKind::UINT64:
            return 1;
        case TypeKind::FLOAT32:
        case TypeKind::FLOAT64:
            return 2;
        default:
            return -1;
    }
}
}  // namespace

// создание типов
Type makePrimitive(TypeKind kind) {
    Type t;
    t.kind = kind;
    return t;
}

Type makeArray(std::size_t n, Type element) {
    Type array_type;
    array_type.kind = TypeKind::ARRAY;
    array_type.data = ArrayTypeInfo{
        .size = n, .element_type = std::make_shared<Type>(std::move(element))};

    return array_type;
}

Type makeStruct(std::string struct_name) {
    Type struct_type;
    struct_type.kind = TypeKind::STRUCT;
    struct_type.data = StructTypeInfo{.struct_name = std::move(struct_name)};

    return struct_type;
}

Type makeError() {
    Type t;
    t.kind = TypeKind::ERROR;
    return t;
}

// служебные функции
bool typeEquals(const Type& a, const Type& b) {
    if (a.kind != b.kind) return false;
    switch (a.kind) {
        case TypeKind::ARRAY: {
            const auto& ai = std::get<ArrayTypeInfo>(a.data);
            const auto& bi = std::get<ArrayTypeInfo>(b.data);
            return ai.size == bi.size &&
                   typeEquals(*ai.element_type, *bi.element_type);
        }
        case TypeKind::STRUCT:
            return std::get<StructTypeInfo>(a.data).struct_name ==
                   std::get<StructTypeInfo>(b.data).struct_name;
        case TypeKind::ERROR:
            return false;
        default:
            return true;
    }
}

bool operator==(const Type& a, const Type& b) { return typeEquals(a, b); }

std::string typeToString(const Type& type) {
    switch (type.kind) {
        case TypeKind::INT8:
            return "int8";
        case TypeKind::INT16:
            return "int16";
        case TypeKind::INT32:
            return "int32";
        case TypeKind::INT64:
            return "int64";
        case TypeKind::UINT8:
            return "uint8";
        case TypeKind::UINT16:
            return "uint16";
        case TypeKind::UINT32:
            return "uint32";
        case TypeKind::UINT64:
            return "uint64";
        case TypeKind::FLOAT32:
            return "float32";
        case TypeKind::FLOAT64:
            return "float64";
        case TypeKind::BOOL:
            return "bool";
        case TypeKind::CHAR:
            return "char";
        case TypeKind::STRING:
            return "string";
        case TypeKind::VOID:
            return "void";
        case TypeKind::ARRAY: {
            const auto& ai = std::get<ArrayTypeInfo>(type.data);
            return "[" + std::to_string(ai.size) + "]" +
                   typeToString(*ai.element_type);
        }
        case TypeKind::STRUCT:
            return std::get<StructTypeInfo>(type.data).struct_name;
        case TypeKind::ERROR:
            return "<error>";
    }
}

bool isConvertibleTo(const Type& from, const Type& to) {
    if (typeEquals(from, to)) return true;
    if (from.kind == TypeKind::ERROR || to.kind == TypeKind::ERROR) return true;

    return wideningDistance(from, to) >= 0;
}

bool isPromotionOf(const Type& from, const Type& to) {
    const int from_family = numericFamily(from.kind);
    const int to_family = numericFamily(to.kind);
    return from_family >= 0 && from_family == to_family &&
           numericRank(from.kind) < numericRank(to.kind);
}

int wideningDistance(const Type& from, const Type& to) {
    if (!isArithmetic(from) || !isArithmetic(to)) return -1;
    if (from.kind == to.kind) return 0;

    constexpr std::size_t type_count =
        static_cast<std::size_t>(TypeKind::ERROR) + 1;
    std::array<bool, type_count> visited{};
    std::deque<std::pair<TypeKind, int>> queue;

    visited[static_cast<std::size_t>(from.kind)] = true;
    queue.push_back({from.kind, 0});

    while (!queue.empty()) {
        const auto [kind, distance] = queue.front();
        queue.pop_front();

        for (const auto& [edge_from, edge_to] : kWideningEdges) {
            if (edge_from != kind) continue;

            const auto index = static_cast<std::size_t>(edge_to);
            if (visited[index]) continue;

            if (edge_to == to.kind) return distance + 1;

            visited[index] = true;
            queue.push_back({edge_to, distance + 1});
        }
    }

    return -1;
}

std::optional<Type> getCommonType(const Type& a, const Type& b) {
    if (a.kind == TypeKind::ERROR) return a;
    if (b.kind == TypeKind::ERROR) return b;

    if (typeEquals(a, b)) return a;

    if (isConvertibleTo(a, b)) return b;

    if (isConvertibleTo(b, a)) return a;

    if ((a.kind == TypeKind::INT64 && b.kind == TypeKind::FLOAT32) ||
        (a.kind == TypeKind::FLOAT32 && b.kind == TypeKind::INT64)) {
        return makePrimitive(TypeKind::FLOAT64);
    }

    return std::nullopt;
}

bool isInteger(const Type& type) {
    switch (type.kind) {
        case TypeKind::INT8:
        case TypeKind::INT16:
        case TypeKind::INT32:
        case TypeKind::INT64:
        case TypeKind::UINT8:
        case TypeKind::UINT16:
        case TypeKind::UINT32:
        case TypeKind::UINT64:
            return true;
        default:
            return false;
    }
}

bool isFloat(const Type& type) {
    return type.kind == TypeKind::FLOAT32 || type.kind == TypeKind::FLOAT64;
}

bool isArithmetic(const Type& type) { return isInteger(type) || isFloat(type); }

bool isBool(const Type& type) { return type.kind == TypeKind::BOOL; }

bool isString(const Type& type) { return type.kind == TypeKind::STRING; }
}  // namespace Semantic
