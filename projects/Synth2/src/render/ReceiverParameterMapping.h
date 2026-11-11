#pragma once
#include "FilterParameterMapping.h"
#include "../patch/Synth1Tuning.h"

namespace synth2::render
{
    inline bool receiverParameter(int index) noexcept
    {return filterParameter(index) || index==2 || index==3 || index==4 || index==45 || index==72;}

    inline void applyReceiverParameter(RenderControls& target,int index,float normalized,bool compatibleCutoff) noexcept
    {
        const float dial=dsp::fractionalDial(normalized);
        switch(index)
        {
            case 2:target.osc2PitchSemis=static_cast<float>(coarseTuneSemitones(static_cast<int>(std::lround(dial))));break;
            case 3:target.osc2FineCents=fineTuneCents(dial);break;
            case 4:target.osc2KbdTrack=dial>0 ? 1.0f : 0.0f;break;
            case 45:target.osc1FmAmount=dial/127.0f;break;
            case 72:target.globalFineTune=fineTuneCents(dial);break;
            default:applyFilterParameter(target,index,normalized,compatibleCutoff);break;
        }
    }
}
