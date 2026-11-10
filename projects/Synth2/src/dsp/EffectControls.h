#pragma once
#include <algorithm>
#include <cmath>
#include "ControlTables.h"

namespace synth2::dsp
{
    struct EffectControls
    {
        static float delaySeconds(int dial,float tempo) noexcept
        {
            constexpr int beats[]={0,1,2,3,4,6,8,9,12,16,18,21,24,32,36,42,48,72,84,96};
            return static_cast<float>(60*beats[std::clamp(dial,0,19)])/(tempo*24.0f);
        }
        static float spreadSeconds(int dial) noexcept
        {
            return delaySpreadWords[static_cast<size_t>(std::clamp(dial,0,127))];
        }
    };
}
