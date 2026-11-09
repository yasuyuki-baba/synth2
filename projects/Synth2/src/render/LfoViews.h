#pragma once
#include "../dsp/LfoBlock.h"
#include "../dsp/ReceiverControls.h"

namespace synth2::render
{
    inline dsp::ReceiverLfoView receiverLfos(const dsp::LfoBlock* block,int start,
        const LfoSettings& first,const LfoSettings& second)
    {
        dsp::ReceiverLfoView view;
        view.receivers={{{first.on,first.dest},{second.on,second.dest}}};
        if(block!=nullptr)
        {
            const int offset=start-block->startSample;
            for(size_t slot=0;slot<2;++slot)
            {
                view.values[slot]=block->controls[slot].data()+offset;
                view.remaining[slot]=block->controlRemaining[slot].data()+offset;
            }
            view.available=true;
        }
        return view;
    }
}
