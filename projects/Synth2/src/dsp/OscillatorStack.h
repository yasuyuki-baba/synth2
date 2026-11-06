#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace synth2::dsp
{
    struct OscillatorStack
    {
        // Native 0x29160: serialized p76 -> cents between alternating lanes.
        static float depth(int dial) noexcept
        {
            return static_cast<float>(std::clamp(dial,0,127))*(1.0f/127.0f)*20.0f;
        }
        static float depth(float dial) noexcept
        {return std::clamp(dial,0.0f,127.0f)*(1.0f/127.0f)*20.0f;}

        // Native 0x3bc40: eight additional oscillators, +/-1,3,5,7 steps.
        static std::array<float,8> frequencies(float base,int dial) noexcept
        {return fromSpacing(base,depth(dial));}
        static std::array<float,8> frequencies(float base,float dial) noexcept
        {return fromSpacing(base,depth(dial));}
    private:
        static std::array<float,8> fromSpacing(float base,float spacing) noexcept
        {
            std::array<float,8> result {};
            const float logTwo=std::log(2.0f);
            for(int pair=0;pair<4;++pair)
            {
                const float cents=static_cast<float>(pair*2+1)*spacing;
                result[pair*2]=std::exp((cents*(1.0f/1200.0f))*logTwo)*base;
                result[pair*2+1]=std::exp((cents*(-1.0f/1200.0f))*logTwo)*base;
            }
            return result;
        }
    };
}
