#pragma once
#include <cmath>
#include <limits>

namespace synth2::dsp
{
    // Preserve the transfer at compatible FP32 input words while retaining
    // a real coordinate between adjacent words. Output/interpolation is double.
    template<class Mapping>
    double compatibleFloatCurve(double coordinate,Mapping mapping) noexcept
    {
        const float word=static_cast<float>(coordinate);
        if(static_cast<double>(word)==coordinate || !std::isfinite(word))return mapping(word);
        constexpr float infinity=std::numeric_limits<float>::infinity();
        const float lower=static_cast<double>(word)>coordinate ? std::nextafter(word,-infinity) : word;
        const float upper=std::nextafter(lower,infinity);
        if(!std::isfinite(lower) || !std::isfinite(upper))return mapping(word);
        const double first=mapping(lower),second=mapping(upper);
        const double fraction=(coordinate-static_cast<double>(lower))
            /(static_cast<double>(upper)-static_cast<double>(lower));
        return first+(second-first)*fraction;
    }
}
