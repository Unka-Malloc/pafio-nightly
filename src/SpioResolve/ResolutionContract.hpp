#pragma once

#include "SpioResolve/Resolver.hpp"

#include <string>
#include <string_view>

namespace spio
{

std::string SerializeResolutionCanonical(
    const ResolvedGraphResult &graph,
    std::string_view manifest_sha256,
    std::string_view lock_sha256);

}  // namespace spio
