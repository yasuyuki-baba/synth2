#pragma once
#include <algorithm>
#include <cmath>

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
            dial=std::clamp(dial,0,127);
            if(dial==64)return 0;
            const float norm=std::min(1.0f,static_cast<float>(std::abs((dial-64)*(1.0f/63.0f))));
            const float seconds=static_cast<float>(std::pow(1000.0f,static_cast<float>(norm))*.0001f);
            return dial<64 ? -seconds : seconds;
        }
    };
}
