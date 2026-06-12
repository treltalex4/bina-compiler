module bina.semantic.overload;

import std;
import bina.semantic.type;
import bina.semantic.symbol;

namespace Semantic {
namespace {
enum class ArgQuality { EXACT = 0, PROMOTION = 1, CONVERSION = 2, NONE = 3 };

ArgQuality argQuality(const Type& arg, const Type& param) {
    if (typeEquals(arg, param)) return ArgQuality::EXACT;
    if (isPromotionOf(arg, param)) return ArgQuality::PROMOTION;
    if (isConvertibleTo(arg, param)) return ArgQuality::CONVERSION;
    return ArgQuality::NONE;
}

ArgQuality worseQuality(ArgQuality lhs, ArgQuality rhs) {
    return static_cast<int>(lhs) > static_cast<int>(rhs) ? lhs : rhs;
}

struct RankedCandidate {
    const FunctionSignature* sig;
    ArgQuality worst;
    int total_distance;
};
}  // namespace

OverloadResolution resolveOverload(
    const std::vector<const FunctionSignature*>& overloads,
    const std::vector<Type>& arg_types) {
    std::vector<const FunctionSignature*> arity_matched;
    std::vector<RankedCandidate> viable;

    for (const auto* sig : overloads) {
        if (sig->param_types.size() != arg_types.size()) continue;
        arity_matched.push_back(sig);

        ArgQuality worst = ArgQuality::EXACT;
        int total_distance = 0;
        bool ok = true;

        for (std::size_t i = 0; i < arg_types.size(); ++i) {
            const Type& arg = arg_types[i];
            const Type& param = sig->param_types[i];
            const ArgQuality quality = argQuality(arg, param);

            if (quality == ArgQuality::NONE) {
                ok = false;
                break;
            }

            worst = worseQuality(worst, quality);
            total_distance += std::max(0, wideningDistance(arg, param));
        }

        if (ok) viable.push_back({sig, worst, total_distance});
    }

    if (viable.empty()) {
        return {.status = OverloadResult::NO_MATCH,
                .candidates = std::move(arity_matched)};
    }

    const ArgQuality best_quality =
        std::ranges::min(viable, {}, &RankedCandidate::worst).worst;
    std::erase_if(viable, [&](const RankedCandidate& candidate) {
        return candidate.worst != best_quality;
    });

    const int best_distance =
        std::ranges::min(viable, {}, &RankedCandidate::total_distance)
            .total_distance;
    std::erase_if(viable, [&](const RankedCandidate& candidate) {
        return candidate.total_distance != best_distance;
    });

    if (viable.size() == 1) {
        return {.status = OverloadResult::OK, .chosen = viable.front().sig};
    }

    std::vector<const FunctionSignature*> tied;
    tied.reserve(viable.size());
    for (const auto& candidate : viable) tied.push_back(candidate.sig);
    return {.status = OverloadResult::AMBIGUOUS, .candidates = std::move(tied)};
}
}  // namespace Semantic
