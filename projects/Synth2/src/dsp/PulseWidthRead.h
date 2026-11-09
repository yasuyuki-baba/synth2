#pragma once
#include <algorithm>
#include <cstdint>

namespace synth2::dsp
{
    // Read the same pulse-offset coordinate for integral and fractional widths.
    // The caller retains its phase lookup, normalization and lane summation rules.
    struct PulseWidthRead
    {
        template<class Sample> static double interpolate(double bins,Sample sample) noexcept
        {
            bins=std::clamp(bins,10.0,2037.0);
            const auto low=static_cast<uint32_t>(bins);
            const double first=sample(low),second=sample(low+1);
            return first+(second-first)*static_cast<double>(bins-low);
        }
    };
}
