#pragma once
#include "../RenderControls.h"
#include "../dsp/FractionalDial.h"
#include "../dsp/CompatibleControlCurve.h"
#include <algorithm>
#include <cmath>
namespace synth2::render
{
    inline float filterSustainForDial(float dial) noexcept
    {
        // Preserve the receiver's reciprocal multiplication before its envelope
        // knots truncate the scaled sustain. Division can cross an integer knot.
        return std::clamp(dial,0.0f,127.0f)*(1.0f/127.0f);
    }
    inline bool filterParameter(int index) noexcept
    {return index==17 || index==19 || index==21 || index==22;}
    inline void applyFilterParameter(RenderControls& target,int index,float normalized,bool compatibleCutoff) noexcept
    {
        const float dial=std::clamp(dsp::fractionalDial(normalized),0.0f,127.0f);
        const float unit=dial/127.0f;
        switch(index)
        {
            case 17:target.filterEnv.sustain=filterSustainForDial(dial);break;
            case 19:
            {
                target.cutoffIndex=unit*1023.0f;target.filterCutoffAdjustment=0;
                const float position=dsp::fractionalDial(normalized);
                if(compatibleCutoff)
                    target.filterCutoffAdjustment=dsp::compatibleControlCurve(position,[](int value) {
                        return std::trunc((static_cast<float>(value)/127.0f)*1023.0f);
                    })-std::trunc(target.cutoffIndex);
                break;
            }
            case 21:target.filterEnvAmount=(dial-63.0f)/64.0f;break;
            case 22:target.filterKeyTrackNorm=unit;break;
        }
    }
}
