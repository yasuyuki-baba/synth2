#pragma once
#include <algorithm>

namespace synth2::dsp
{
    // Static balance keeps unity on the nearer channel. Callers retain their
    // placement and multiplication order; this does not move gain across DSP.
    struct StereoBalance
    {
        double left {},right {};
        static StereoBalance gains(double pan) noexcept
        { return {std::clamp(1.0-pan,0.0,1.0),std::clamp(1.0+pan,0.0,1.0)}; }
    };
}
