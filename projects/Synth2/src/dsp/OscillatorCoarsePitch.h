#pragma once
#include <algorithm>
#include <cmath>

namespace synth2::dsp
{
    // CVCO coarse setter3e220, wrapper3bdc0 and note trigger3db00.
    // The wrapper interpolates octave exponents; its final assignment does not
    // refresh the cached frequency. Only an actual receiver call does that.
    struct OscillatorCoarsePitch
    {
        float current {},target {},anchor {},cached {};
        int elapsed {};
        bool pending {};
        void set(float value,bool sounding) noexcept
        {
            target=value;pending=true;
            if(sounding){anchor=current;elapsed=0;}else current=value;
        }
        void refresh() noexcept {cached=current;}
        void noteOn() noexcept
        {if(pending){current=target;cached=current;pending=false;}}
        int span(int available,int rate) noexcept
        {
            const int horizon=std::max(1,rate/20);
            if(current!=target && elapsed<horizon)
            {
                const float distance=target-anchor;
                current=anchor+(distance*static_cast<float>(elapsed))/static_cast<float>(horizon);
                cached=current;
                const int resolution=std::max(1,std::abs(static_cast<int>(distance*128.0f)));
                return std::min(available,std::max(1,horizon/resolution));
            }
            current=target;return available;
        }
    };
}
