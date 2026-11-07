#pragma once
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace synth2::host
{
// Bump when inference rules or resolved loading semantics change.
inline constexpr int patchTagCacheVersion=1;
// Inputs are resolved host dials, never raw version-dependent source entries.
// Heuristic suggestions describe names/settings, not measured audio semantics.
std::vector<std::string> inferPatchTags(std::string_view name,const std::array<float,129>& dials);
}
