#pragma once
#include <algorithm>
#include <cstdint>

namespace synth2::dsp
{
    // Read the same pulse-offset coordinate for integral and fractional widths.
    // The caller retains its phase lookup, normalization and lane summation rules.
    struct PulseWidthRead
    {
        template<class Sample> static float interpolate(float bins,Sample sample) noexcept
        {
            bins=std::clamp(bins,10.0f,2037.0f);
            const auto low=static_cast<uint32_t>(bins);
            const float first=sample(low),second=sample(low+1);
            return first+(second-first)*static_cast<float>(bins-low);
        }
    };
}
