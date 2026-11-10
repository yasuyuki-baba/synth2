#pragma once
#include <algorithm>

namespace synth2::dsp
{
    // Static balance keeps unity on the nearer channel. Callers retain their
    // placement and multiplication order; this does not move gain across DSP.
    struct StereoBalance
    {
        float left {},right {};
        static StereoBalance gains(float pan) noexcept
        { return {std::clamp(1.0f-pan,0.0f,1.0f),std::clamp(1.0f+pan,0.0f,1.0f)}; }
    };
}
