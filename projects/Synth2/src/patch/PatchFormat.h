#pragma once
#include <array>
#include <string>
#include <string_view>

namespace synth2
{
inline constexpr int currentPatchFormatVersion = 200;
// Values use host dial units, not normalized values. p126 belongs to the
// legacy host ABI; the file version is carried by the header instead.
struct ContinuousPatch
{
    std::string name;
    std::array<float,129> values{};
};
bool isContinuousPatch(std::string_view text);
ContinuousPatch parseContinuousPatch(std::string_view text);
std::string formatContinuousPatch(const ContinuousPatch& patch);
}
