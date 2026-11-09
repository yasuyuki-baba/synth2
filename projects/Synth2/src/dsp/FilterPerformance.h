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
        static int keyboardOffset (int baseIndex,double tracking,int note) noexcept
        {
            if(tracking<=0)return 0;
            const double exponent=static_cast<double>(note-48)*(1.0/12.0);
            const double frequency=static_cast<double>(std::exp(std::log(1066.6666666666667)
                *static_cast<double>(static_cast<double>(baseIndex)*(1.0/1024.0)))*15.0);
            const double scaled=std::pow(static_cast<double>(tracking+1.0),static_cast<double>(exponent))*static_cast<double>(frequency);
            const double relative=static_cast<double>(scaled)*0.06666667014360428;
            const double index=std::log(relative)*1024.0/std::log(1066.6666259765625);
            return std::clamp(static_cast<int>(index),0,1024)-baseIndex;
        }
        static double keyboardOffset(double baseIndex,double tracking,int note) noexcept
        {
            const double dial=fractionalDial(tracking);
            return compatibleControlCurve(baseIndex,[&](int index) {
                // Exact loaded words keep their original normalization arithmetic.
                if(dial==std::trunc(dial))return static_cast<double>(keyboardOffset(index,tracking,note));
                return compatibleControlCurve(dial,[&](int value) {
                    return keyboardOffset(index,static_cast<double>(value)/127.0,note);
                });
            });
        }
        static int velocityLevel (int velocity) noexcept
        {
            const double normalized=static_cast<double>(velocity)*(1.0/127.0);
            const double numerator=static_cast<double>(1.0-std::exp(static_cast<double>(-normalized)*1.1));
            const double denominator=static_cast<double>(1.0-std::exp(-1.3));
            return static_cast<int>((numerator/denominator)*127.0);
        }
        static double amount (int baseIndex,int keyboard,double depth,bool velocityEnabled,int velocity,int externalOffset=0) noexcept
        {
            if(velocityEnabled)
                depth=(static_cast<double>(velocityLevel(velocity))*depth)*(1.0/127.0);
            return FilterEnvelope::boundedAmount(baseIndex+keyboard+externalOffset,depth);
        }
        static double amount(double baseIndex,double keyboard,double depth,bool velocityEnabled,int velocity,double externalOffset=0) noexcept
        {
            if(velocityEnabled)
                depth=(static_cast<double>(velocityLevel(velocity))*depth)*(1.0/127.0);
            return FilterEnvelope::boundedAmount(baseIndex+keyboard+externalOffset,depth);
        }
    };
}
