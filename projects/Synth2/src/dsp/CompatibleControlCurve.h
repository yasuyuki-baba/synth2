#pragma once
#include <cmath>
#include <algorithm>
#include <utility>

namespace synth2::dsp
{
    // Integer coordinates identify measured control-curve knots, not the
    // storage type of a running signal. Fractional coordinates stay real.
    template<class Mapping>
    float compatibleControlCurve(float coordinate,Mapping mapping) noexcept
    {
        const int low=static_cast<int>(std::floor(coordinate));
        const float first=mapping(low);
        if(coordinate==low)return first;
        return first+(static_cast<float>(mapping(low+1))-first)*(coordinate-low);
    }

    template<class Mapping>
    std::pair<float,int> compatibleControlQuery(float coordinate,Mapping mapping) noexcept
    {
        const int low=static_cast<int>(std::floor(coordinate));
        const auto first=mapping(low);
        if(coordinate==low)return {first.first,first.second};
        const auto second=mapping(low+1);
        const int frames=first.second<0 ? second.second : second.second<0 ? first.second
            : std::min(first.second,second.second);
        return {first.first+(static_cast<float>(second.first)-first.first)*(coordinate-low),frames};
    }
}
