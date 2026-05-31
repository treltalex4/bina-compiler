export module bina.semantic.overload;

import std;
import bina.semantic.type;
import bina.semantic.symbol;

export namespace Semantic {
enum class OverloadResult { OK, NO_MATCH, AMBIGUOUS };

struct OverloadResolution {
    OverloadResult status;
    const FunctionSignature* chosen = nullptr;
    std::vector<const FunctionSignature*> candidates = {};
};

OverloadResolution resolveOverload(
    const std::vector<const FunctionSignature*>& overloads,
    const std::vector<Type>& arg_types);
}  // namespace Semantic