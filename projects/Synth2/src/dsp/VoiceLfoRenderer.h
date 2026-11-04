#pragma once
#include <array>
#include "ReceiverControls.h"

namespace synth2::dsp
{
    struct VoiceLfoSettings
    {
        bool nativePitch {};
    };
    struct VoiceLfoView
    {
        std::array<const float*,2> shared {};
        ReceiverLfoView controls;
    };
    struct VoiceLfoSample
    {
        std::array<float,2> values {};
        double firstPitch {},secondPitch {};
    };
    struct VoiceLfoRenderer
    {
        static VoiceLfoSample sample(const VoiceLfoSettings& config,const VoiceLfoView& view,
            int position)
        {
            VoiceLfoSample result;
            // Voices borrow prepared signals; only Engine advances LFO state.
            for(std::size_t slot=0;slot<2;++slot)
                if(view.shared[slot]!=nullptr)result.values[slot]=view.shared[slot][position];
            if(config.nativePitch && view.controls.available)
                for(std::size_t slot=0;slot<2;++slot)if(view.controls.receivers[slot].enabled)
                {
                    const double value=view.controls.values[slot][position];
                    const int destination=view.controls.receivers[slot].destination;
                    if(destination==1)result.firstPitch+=value;
                    if(destination==0 || destination==1)result.secondPitch+=value;
                }
            return result;
        }
    };
}
