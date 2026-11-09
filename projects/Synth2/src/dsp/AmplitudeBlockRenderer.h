#pragma once
#include "ReceiverControls.h"
#include "AmplitudeEnvelope.h"

namespace synth2::dsp
{
    // Horizons and the optional known event end are relative to this call.
    // repeatHorizon continues the caller's gain grid after its initial span.
    struct AmplitudeBlockSettings { int gainOffset {};int gainHorizon {};int repeatHorizon {};int boundaryFrames {}; };

    struct AmplitudeBlockRenderer
    {
        // All inputs use physical VCA attenuation coordinates. Keep independent
        // slot products, caller-owned alignment and each source's held spans.
        static bool render(const AmplitudeBlockSettings& config,const ReceiverLfoView& lfos,
            AmplitudeEnvelope& amplitude,int frames,float* gains)
        {
            bool rendered=false;
            for(int offset=0;offset<frames;)
            {
                std::array<double,2> control{};int count=frames-offset;
                int horizon=config.gainHorizon;
                if(config.repeatHorizon>0)
                {
                    horizon=offset<config.gainHorizon ? config.gainHorizon-offset
                        : config.repeatHorizon-(offset-config.gainHorizon)%config.repeatHorizon;
                    if(config.boundaryFrames>offset)horizon=std::min(horizon,config.boundaryFrames-offset);
                    count=std::min(count,horizon);
                }
                for(size_t slot=0;slot<2;++slot)
                {
                    if(lfos.available && lfos.receivers[slot].enabled && lfos.receivers[slot].destination==3)
                    {
                        control[slot]=lfos.values[slot][offset];
                        count=std::min(count,lfos.remaining[slot][offset]);
                        if(horizon>0)horizon=std::min(horizon,lfos.remaining[slot][offset]);
                    }
                }
                amplitude.setModulation(control[0],control[1]);
                amplitude.render(gains+config.gainOffset+offset,count,horizon);
                rendered=rendered || !amplitude.wasSilent();
                offset+=count;
            }
            return rendered;
        }
    };
}
