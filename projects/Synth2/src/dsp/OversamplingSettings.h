#pragma once
#include <array>

namespace synth2::dsp
{
struct OversamplingSettings
{
    static constexpr std::array<int,5> factors {1,2,4,8,16};
    static constexpr bool valid(int factor) noexcept
    { for(int value:factors)if(value==factor)return true;return false; }
    // A 65-tap half-band decimator at each power-of-two stage.
    static constexpr int decimationLatency(int factor) noexcept
    { return factor==1 || !valid(factor) ? 0 : 32-32/factor; }
    // Voice decimation plus the output drive's interpolation/decimation pair.
    static constexpr int latency(int factor) noexcept
    { return 3*decimationLatency(factor); }
};
}
