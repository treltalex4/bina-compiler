export module bina.semantic.type;

import std;

export namespace Semantic {
// имеющиеся типы
enum class TypeKind {
    INT8,
    INT16,
    INT32,
    INT64,
    UINT8,
    UINT16,
    UINT32,
    UINT64,
    FLOAT32,
    FLOAT64,
    BOOL,
    STRING,
    VOID,
    ARRAY,
    STRUCT,
    ERROR,
};

struct Type;

struct ArrayTypeInfo {
    std::size_t size;
    std::shared_ptr<Type> element_type;
};

struct StructTypeInfo {
    std::string struct_name;
};

using TypeData = std::variant<std::monostate, ArrayTypeInfo, StructTypeInfo>;

struct Type {
    TypeKind kind;
    TypeData data;
};

using TypeMap = std::unordered_map<const void*, Type>;

// создание типов
Type makePrimitive(TypeKind kind);
Type makeArray(std::size_t n, Type element);
Type makeStruct(std::string struct_name);
Type makeError();

// служебные функции
bool typeEquals(const Type& a, const Type& b);
bool operator==(const Type& a, const Type& b);
std::string typeToString(const Type& type);
bool isConvertibleTo(const Type& from, const Type& to);
std::optional<Type> getCommonType(const Type& a, const Type& b);

bool isInteger(const Type& type);
bool isFloat(const Type& type);
bool isArithmetic(const Type& type);
bool isBool(const Type& type);
bool isString(const Type& type);

}  // namespace Semantic