#pragma once

#include "FilterEnvelope.h"
#include "FractionalDial.h"
#include <algorithm>
#include <cmath>

namespace synth2::dsp
{
    // CVcf note control, Win64 RVAs 0x38070 and 0x3a780.
    struct FilterPerformance
    {
        static int keyboardOffset (int baseIndex,float coordinate,int note) noexcept
        {
            const float tracking=static_cast<float>(coordinate);
            if(tracking<=0)return 0;
            const float exponent=static_cast<float>(note-48)*(1.0f/12.0f);
            const float frequency=static_cast<float>(std::exp(std::log(1066.6666666666667f)
                *(static_cast<float>(baseIndex)*(1.0f/1024.0f)))*15.0f);
            const float scaled=std::pow(static_cast<float>(tracking+1.0f),static_cast<float>(exponent))*static_cast<float>(frequency);
            const float relative=static_cast<float>(scaled)*0.06666667014360428f;
            const float index=std::log(relative)*1024.0f/std::log(1066.6666259765625f);
            return std::clamp(static_cast<int>(index),0,1024)-baseIndex;
        }
        static float keyboardOffset(float baseIndex,float tracking,int note) noexcept
        {
            const float dial=fractionalDial(tracking);
            return compatibleControlCurve(baseIndex,[&](int index) {
                // Exact loaded words keep their original normalization arithmetic.
                if(dial==std::trunc(dial))return static_cast<float>(keyboardOffset(index,tracking,note));
                return compatibleControlCurve(dial,[&](int value) {
                    return keyboardOffset(index,static_cast<float>(value)/127.0f,note);
                });
            });
        }
        static int velocityLevel (int velocity) noexcept
        {
            const float normalized=static_cast<float>(velocity)*(1.0f/127.0f);
            const float numerator=static_cast<float>(1.0f-std::exp(static_cast<float>(-normalized)*1.1f));
            const float denominator=static_cast<float>(1.0f-std::exp(-1.3f));
            return static_cast<int>((numerator/denominator)*127.0f);
        }
        static float amount (int baseIndex,int keyboard,float coordinate,bool velocityEnabled,int velocity,int externalOffset=0) noexcept
        {
            float depth=static_cast<float>(coordinate);
            if(velocityEnabled)
                depth=(static_cast<float>(velocityLevel(velocity))*depth)*(1.0f/127.0f);
            return FilterEnvelope::boundedAmount(baseIndex+keyboard+externalOffset,depth);
        }
        static float amount(float baseIndex,float keyboard,float depth,bool velocityEnabled,int velocity,float externalOffset=0) noexcept
        {
            if(velocityEnabled)
                depth=(static_cast<float>(velocityLevel(velocity))*depth)*(1.0f/127.0f);
            return FilterEnvelope::boundedAmount(baseIndex+keyboard+externalOffset,depth);
        }
    };
}
