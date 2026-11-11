#pragma once
#include <algorithm>
#include <cmath>

namespace synth2::dsp
{
    // Recover integer source words exactly after either native normalization.
    // Other representable dial positions keep their fractional part.
    inline float fractionalDial(float normalized) noexcept
    {
        const float value=std::clamp(normalized,0.0f,1.0f);
        const float raw=value*127.0f,nearest=std::round(raw);
        // Loading may publish either native float normalization. Recognize
        // those exact source words before extending the continuous coordinate.
        const float anchor=static_cast<float>(nearest);
        return value==nearest/127.0f || value==static_cast<float>(anchor/127.0f)
            || value==static_cast<float>(anchor*(1.0f/127.0f)) ? nearest : raw;
    }
    template<class Mapping>
    float interpolateDial(float dial,Mapping mapping) noexcept
    {
        dial=std::clamp(dial,0.0f,127.0f);
        const int low=static_cast<int>(dial);
        const float first=mapping(low);
        if(dial==low || low==127)return first;
        return first+(mapping(low+1)-first)*(dial-low);
    }
}
