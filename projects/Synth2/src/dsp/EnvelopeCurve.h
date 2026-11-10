#pragma once
#include <algorithm>
#include <cmath>

namespace synth2::dsp
{
    // These are quantized control knots, not audio sample storage. Evaluate the
    // compatible word before interpolating fractional levels in double.
    inline double exponentialEnvelopeKnot(int range,int remainingFrames,int duration) noexcept
    {
        const float remaining=static_cast<float>(remainingFrames)*(1.0f/static_cast<float>(std::max(1,duration)));
        const float curve=std::exp((remaining-1.0f)*8.0f)*remaining;
        return std::trunc(curve*static_cast<float>(range));
    }
    inline double linearEnvelopeKnot(int range,int elapsed,int duration) noexcept
    {
        const float slope=static_cast<float>(range)*(1.0f/static_cast<float>(std::max(1,duration)));
        return std::trunc(static_cast<float>(elapsed)*slope);
    }
}
