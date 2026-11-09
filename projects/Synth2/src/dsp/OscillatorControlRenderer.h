#pragma once
#include "ReceiverControls.h"
#include "FractionalDial.h"
#include "FilterEnvelope.h"
#include "Portamento.h"
#include "FrequencyModulation.h"
#include "OscillatorCoarsePitch.h"
#include <cmath>

namespace synth2::dsp
{
    struct OscillatorControlSettings
    {
        bool envelopeEnabled {},oscillatorLfo {},applyPortamento {},envelopeFm {},fmLfo {};
        float portamentoDial {-1};
        double sampleRate {44100.0};
        float fmAmount {};
        bool ringMod {};
    };
    // Caller-owned buffers and state; no global clocks, RNG or voice lifecycle.
    struct OscillatorControlRenderer
    {
        static void envelope(const OscillatorControlSettings& config,const ReceiverLfoView& lfos,
            FilterEnvelope& state,int count,VoiceControlBuffer& pitch,VoiceSpanBuffer& remaining)
        {
            std::fill_n(pitch.data(),count,0);
            for(int i=0;i<count;++i)remaining[i]=count-i;
            if(!config.envelopeEnabled)return;
            int position=0;
            while(position<count)
            {
                int frames=count-position;
                if(config.oscillatorLfo)frames=lfos.oscillatorFrames(position,frames);
                state.render(frames,[&](double target,int size) {
                    std::fill_n(pitch.data()+position,size,target);
                    for(int i=0;i<size;++i)remaining[position+i]=size-i;
                    position+=size;
                });
            }
        }
        static void glide(const OscillatorControlSettings& config,const ReceiverLfoView& lfos,
            Portamento& state,int count,VoiceSpanBuffer& remaining,VoiceControlBuffer& pitch)
        {
            std::fill_n(pitch.data(),count,0);
            if(config.portamentoDial<0)return;
            const float seconds=Portamento::timeSeconds(config.portamentoDial);
            if(state.seconds!=seconds || state.duration!=static_cast<int>(static_cast<float>(config.sampleRate)*seconds))
                state.setDuration(seconds,static_cast<int>(config.sampleRate));
            int position=0;
            while(position<count)
            {
                int frames=remaining[position];
                if(config.oscillatorLfo)frames=lfos.oscillatorFrames(position,frames);
                state.render(frames,[&](double control,int size) {
                    if(config.applyPortamento)std::fill_n(pitch.data()+position,size,control);
                    // FM dispatch is nested inside each glide region, even
                    // when consecutive pitch values happen to be equal.
                    for(int i=0;i<size;++i)remaining[position+i]=size-i;
                    position+=size;
                });
            }
        }
        static void fm(const OscillatorControlSettings& config,const ReceiverLfoView& lfos,
            FrequencyModulation& state,int count,const VoiceControlBuffer& envelopePitch,
            const VoiceSpanBuffer& remaining,VoiceControlBuffer& depth,
            OscillatorCoarsePitch* coarse=nullptr,std::array<float,256>* exponents=nullptr)
        {
            int position=0;
            while(position<count)
            {
                double control=config.envelopeFm ? envelopePitch[position] : 0;
                int frames=remaining[position];
                if(config.fmLfo && lfos.available)
                    for(size_t slot=0;slot<2;++slot)if(ReceiverLfoView::reachesOscillator(lfos.receivers[slot]))
                    {
                        frames=std::min(frames,lfos.remaining[slot][position]);
                        if(lfos.receivers[slot].destination==5)control+=lfos.values[slot][position];
                    }
                if(coarse!=nullptr)frames=coarse->span(frames,static_cast<int>(config.sampleRate));
                const int start=position;
                state.render(FrequencyModulation::depth(static_cast<double>(fractionalDial(config.fmAmount)),control),
                    frames,static_cast<int>(config.sampleRate),
                    [&](double value){depth[position++]=value;},coarse!=nullptr,!config.ringMod);
                if(coarse!=nullptr)
                {
                    std::fill(exponents->begin()+start,exponents->begin()+position,coarse->cached);
                    coarse->elapsed+=position-start;
                }
            }
        }
    };
}
