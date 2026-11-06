#pragma once
#include "OscillatorSection.h"
#include "ReceiverControls.h"

namespace synth2::dsp
{
    struct OscillatorAudioSettings
    {
        bool secondTracksKeyboard {},pitchReceiver {},pitchEnvelope {},pulseReceiver {},pulseEnvelope {};
        float pulseWidth {};
        int subWave {},subOctave {-1};
        bool correctFmPhase {},pitchEnvelopeBoth {};
    };
    struct OscillatorAudioInput
    {
        float firstFrequency {},secondFrequency {},pulseWidth {},mix {},subGain {};
        bool sync {},ring {};
        double firstPitch {},secondPitch {},glidePitch {},envelope {},fmDepth {};
        double pulseControl {};
    };
    struct OscillatorAudioRenderer
    {
        static float process(OscillatorSection& oscillator,const OscillatorAudioSettings& config,
            const OscillatorAudioInput& input,const ReceiverLfoView& lfos,int position,
            const synth2::WaveTableBank* bank,float* frame=nullptr,int factor=1)
        {
            float firstFrequency=input.firstFrequency,secondFrequency=input.secondFrequency;
            double firstPitch=input.firstPitch+input.glidePitch;
            double secondPitch=input.secondPitch+(config.secondTracksKeyboard ? input.glidePitch : 0);
            if(config.pitchEnvelope)
            {
                // The reserved direct-call both-pitch destination spans one
                // octave; ordinary OSC2 pitch spans five at the same peak.
                const double control=config.pitchEnvelopeBoth ? input.envelope*(128.0/640.0) : input.envelope;
                if(config.pitchReceiver)
                {
                    secondPitch+=control;
                    if(config.pitchEnvelopeBoth)firstPitch+=control;
                }
                else
                {
                    secondFrequency=ModulationEnvelope::pitchFrequency(secondFrequency,control);
                    if(config.pitchEnvelopeBoth)firstFrequency=ModulationEnvelope::pitchFrequency(firstFrequency,control);
                }
            }
            const double fm=input.fmDepth;
            double pulse=input.pulseControl;
            if(config.pulseReceiver && lfos.available)
                for(size_t slot=0;slot<2;++slot)
                    if(lfos.receivers[slot].enabled && lfos.receivers[slot].destination==4)
                        pulse+=lfos.values[slot][position];
            if(config.pulseEnvelope)pulse+=input.envelope;
            const float width=config.pulseReceiver || config.pulseEnvelope
                ? config.pulseWidth : input.pulseWidth;
            const double bins=ModulationEnvelope::pulseWidthForDial(fractionalDial(width),pulse);
            if(frame) {
                oscillator.processFrame(frame,factor,firstFrequency,secondFrequency,input.pulseWidth,input.mix,
                    fm,input.sync,input.ring,input.subGain,config.subWave,config.subOctave,bank,
                    bins,firstPitch,secondPitch,config.pitchReceiver,config.correctFmPhase);
                return frame[0];
            }
            return oscillator.processSample(firstFrequency,secondFrequency,input.pulseWidth,input.mix,
                fm,input.sync,input.ring,input.subGain,config.subWave,config.subOctave,bank,
                bins,firstPitch,secondPitch,config.pitchReceiver,config.correctFmPhase);
        }
    };
}
