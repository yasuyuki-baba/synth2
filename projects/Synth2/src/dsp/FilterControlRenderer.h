#pragma once
#include "ReceiverControls.h"
#include "FilterEnvelope.h"
#include "CutoffControl.h"

namespace synth2::dsp
{
    struct FilterControlSettings
    {
        bool enabled {},filterLfo {};
        double cutoffIndex {};
    };

    struct FilterControlRenderer
    {
        static void render(const FilterControlSettings& config,const ReceiverLfoView& lfos,
            FilterEnvelope& envelope,CutoffControl& cutoff,double keyboardOffset,double unisonOffset,
            int count,VoiceControlBuffer& indices)
        {
            std::fill_n(indices.data(),count,0);
            if(!config.enabled)return;
            int position=0;
            while(position<count)
            {
                int frames=count-position;double modulation=0;
                if(config.filterLfo && lfos.available)
                    for(size_t slot=0;slot<2;++slot)if(lfos.receivers[slot].enabled && lfos.receivers[slot].destination==2)
                    {
                        modulation+=lfos.values[slot][position];
                        frames=std::min(frames,lfos.remaining[slot][position]);
                    }
                envelope.render(frames,[&](double target,int size) {
                    const double desired=CutoffControl::target(config.cutoffIndex,keyboardOffset,unisonOffset,target,modulation,0);
                    cutoff.render(desired,size,[&](double index,int samples) {
                        std::fill_n(indices.data()+position,samples,index);position+=samples;
                    });
                });
            }
        }
    };
}
