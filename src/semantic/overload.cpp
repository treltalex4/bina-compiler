module bina.semantic.overload;

import std;
import bina.semantic.type;
import bina.semantic.symbol;

namespace Semantic {
namespace {
// все аргументы точно совпадают с типами параметров
bool exactMatch(const FunctionSignature& sig, const std::vector<Type>& args) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (!typeEquals(args[i], sig.param_types[i])) return false;
    }
    return true;
}

// каждый аргумент неявно приводится к типу параметра
bool convertibleMatch(const FunctionSignature& sig,
                      const std::vector<Type>& args) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (!isConvertibleTo(args[i], sig.param_types[i])) return false;
    }
    return true;
}
}  // namespace

OverloadResolution resolveOverload(
    const std::vector<const FunctionSignature*>& overloads,
    const std::vector<Type>& arg_types) {
    // 1. фильтр по числу аргументов
    std::vector<const FunctionSignature*> candidates;
    for (const auto* sig : overloads) {
        if (sig->param_types.size() == arg_types.size())
            candidates.push_back(sig);
    }

    if (candidates.empty()) return {.status = OverloadResult::NO_MATCH};

    for (const auto* sig : candidates) {
        if (exactMatch(*sig, arg_types))
            return {.status = OverloadResult::OK, .chosen = sig};
    }

    std::vector<const FunctionSignature*> convertible;
    for (const auto* sig : candidates) {
        if (convertibleMatch(*sig, arg_types)) convertible.push_back(sig);
    }

    if (convertible.size() == 1)
        return {.status = OverloadResult::OK, .chosen = convertible[0]};

    if (convertible.empty())
        return {.status = OverloadResult::NO_MATCH,
                .candidates = std::move(candidates)};

    return {.status = OverloadResult::AMBIGUOUS,
            .candidates = std::move(convertible)};
}
}  // namespace Semantic
