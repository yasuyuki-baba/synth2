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
        double current {},target {},anchor {},cached {};
        int elapsed {};
        bool pending {};
        void set(double value,bool sounding) noexcept
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
                const double distance=target-anchor;
                current=anchor+(distance*static_cast<double>(elapsed))/static_cast<double>(horizon);
                cached=current;
                const int resolution=std::max(1,std::abs(static_cast<int>(distance*128.0)));
                return std::min(available,std::max(1,horizon/resolution));
            }
            current=target;return available;
        }
    };
}
