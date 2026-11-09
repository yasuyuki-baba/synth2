#pragma once
#include <algorithm>
#include <cmath>
#include "FractionalDial.h"

namespace synth2::dsp
{
    // Source23 dispatch0x298fb..0x29956 followed by CVcf0x3a920.
    // The source dial selects the compatible drive curve. Keep its normalization,
    // exponential and receiver scaling in double precision.
    inline double saturationForDial (int dial) noexcept
    {
        dial=std::clamp(dial,0,127);
        const double exponent=static_cast<double>(dial)*(1.0/127.0)-1.0;
        const double curve=(std::exp(static_cast<double>(exponent)*5.8)*static_cast<double>(dial))*(1.0/127.0);
        return static_cast<double>(curve)*100.0;
    }

    inline double saturationStrength(double normalized) noexcept
    {return interpolateDial(fractionalDial(normalized),[](int value){return saturationForDial(value);});}

    inline double saturate (double input,double strength) noexcept
    {
        return input*(strength+1.0)/(std::abs(input)*strength+1.0);
    }
}
