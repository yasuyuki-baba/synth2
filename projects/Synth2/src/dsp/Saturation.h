#pragma once
#include <algorithm>
#include <cmath>
#include "FractionalDial.h"

namespace synth2::dsp
{
    // Source23 dispatch0x298fb..0x29956 followed by CVcf0x3a920.
    // Preserve the float dial normalization, double exponential/products,
    // float receiver field and final float*100 in that order.
    inline float saturationForDial (int dial) noexcept
    {
        dial=std::clamp(dial,0,127);
        const float exponent=static_cast<float>(dial)*(1.0f/127.0f)-1.0f;
        const double curve=(std::exp(static_cast<double>(exponent)*5.8)*static_cast<double>(dial))*(1.0/127.0);
        return static_cast<float>(curve)*100.0f;
    }

    inline float saturationStrength(float normalized) noexcept
    {return interpolateDial(fractionalDial(normalized),[](int value){return saturationForDial(value);});}

    inline float saturate (float input,float strength) noexcept
    {
        return input*(strength+1.0f)/(std::abs(input)*strength+1.0f);
    }
}
