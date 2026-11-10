#pragma once
#include <algorithm>
#include <cmath>
#include "FloatControlCurve.h"

namespace synth2::dsp
{
    struct EffectControls
    {
        static double delaySeconds(int dial,double tempo) noexcept
        {
            constexpr int beats[]={0,1,2,3,4,6,8,9,12,16,18,21,24,32,36,42,48,72,84,96};
            return compatibleFloatCurve(tempo,[&](float word) {
                return static_cast<float>(60*beats[std::clamp(dial,0,19)])/(word*24.0f);
            });
        }
        static float spreadSeconds(int dial) noexcept
        {
            dial=std::clamp(dial,0,127);
            if(dial==64)return 0;
            const float norm=std::min(1.0f,static_cast<float>(std::abs((dial-64)*(1.0/63.0))));
            const float seconds=static_cast<float>(std::pow(1000.0,static_cast<double>(norm))*.0001);
            return dial<64 ? -seconds : seconds;
        }
    };
}
