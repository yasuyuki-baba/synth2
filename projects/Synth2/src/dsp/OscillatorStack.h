#pragma once
#include "CompatibleControlCurve.h"
#include "FloatControlCurve.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace synth2::dsp
{
    struct OscillatorStack
    {
        static double depth(double dial) noexcept
        {return compatibleControlCurve(std::clamp(dial,0.0,127.0),[](int value){return depthKnot(value);});}
        static std::array<double,8> frequencies(double base,double dial) noexcept
        {
            dial=std::clamp(dial,0.0,127.0);
            const int low=static_cast<int>(dial);
            auto result=frequencyKnot(base,low);
            if(dial==low)return result;
            const auto next=frequencyKnot(base,low+1);
            for(size_t i=0;i<result.size();++i)result[i]+=(next[i]-result[i])*(dial-low);
            return result;
        }
    private:
        // Compatible tuning knots are source coordinates, not audio storage.
        static float depthKnot(int dial) noexcept
        {return static_cast<float>(dial)*(1.0f/127.0f)*20.0f;}
        static std::array<double,8> frequencyKnot(double base,int dial) noexcept
        {
            std::array<double,8> result{};
            const float spacing=depthKnot(dial),logTwo=std::log(2.0f);
            for(int pair=0;pair<4;++pair)
            {
                const float cents=static_cast<float>(pair*2+1)*spacing;
                const float up=std::exp((cents*(1.0f/1200.0f))*logTwo);
                const float down=std::exp((cents*(-1.0f/1200.0f))*logTwo);
                result[pair*2]=compatibleFloatCurve(base,[&](float word){return up*word;});
                result[pair*2+1]=compatibleFloatCurve(base,[&](float word){return down*word;});
            }
            return result;
        }
    };
}
