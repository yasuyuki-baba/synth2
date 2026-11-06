#pragma once
#include "../RenderControls.h"
#include "../dsp/VoiceLfoRenderer.h"
#include "../dsp/LfoBlock.h"

namespace synth2::render
{
    inline dsp::VoiceLfoSettings voiceLfoConfig(const RenderControls& p)
    { return {p.nativePitchLfo}; }
    inline dsp::VoiceLfoView voiceLfoInput(const dsp::LfoBlock* block,int start,const dsp::ReceiverLfoView& controls)
    {
        dsp::VoiceLfoView view;view.controls=controls;
        if(block!=nullptr)
            for(int slot=0;slot<2;++slot)
                view.shared[slot]=block->samples.getReadPointer(slot)+(start-block->startSample);
        return view;
    }
}
