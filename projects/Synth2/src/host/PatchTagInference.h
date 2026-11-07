#pragma once
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace synth2::host
{
// Inputs are resolved host dials, never raw version-dependent source entries.
// Heuristic suggestions describe names/settings, not measured audio semantics.
std::vector<std::string> inferPatchTags(std::string_view name,const std::array<float,129>& dials);
}
