#pragma once

#include "audio/AudioTypes.h"
#include "EffectSettings.h"
#include "OutputTone.h"
#include "PeakEq.h"
#include "FractionalDial.h"
#include "SilenceGate.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace synth2::dsp
{
    // One stereo PeakEq -> OutputTone graph for normalized fractional dials.
    // Explicit context bypass and semantic silence retain their separate roles.
    class Equalizer
    {
    public:
        void prepare (double newSampleRate) noexcept
        {
            sampleRate = newSampleRate;
            cached = false;
            reset();
        }

        void prepare (const ::synth2::audio::ProcessSpec& spec) noexcept
        {
            sampleRate = spec.sampleRate;
            cached = false;
            reset();
        }

        void updateSampleRate (double rate) noexcept
        {
            sampleRate = rate;
            cached = false;
            nativeTone.prepare(rate,true);
            nativePeak.prepare(rate,true);
            peakSilence.prepare(rate,.2,true);toneSilence.prepare(rate,.2,true);
        }

        void setParams (const EqualizerSettings& p) noexcept
        {
            if (cached && p.level == previous.level && p.q == previous.q
                && p.tone == previous.tone && p.freq == previous.freq) return;
            previous = p;
            cached = true;
            const double levelDial=fractionalDial(p.level);
            peakEnabled = levelDial != 64;
            nativePeak.setDials(fractionalDial(p.freq),levelDial,fractionalDial(p.q));
            nativeTone.setFractionalDial(fractionalDial(p.tone));
        }

        void reset() noexcept
        {
            nativeTone.prepare(sampleRate);
            nativePeak.prepare(sampleRate);
            peakSilence.prepare(sampleRate,.2);toneSilence.prepare(sampleRate,.2);
        }

        inline double processSample (double in, int channel = 0) noexcept
        {
            channel = std::clamp (channel, 0, 1);
            const double out = peakEnabled ? nativePeak.process(in,channel) : in;
            return nativeTone.process(out,channel);
        }

        std::array<int,4> nativeSilenceSnapshot() const noexcept
        {
            const auto peak=peakSilence.snapshot(),tone=toneSilence.snapshot();
            return {peak[0],peak[1],tone[0],tone[1]};
        }

        // Canonical stereo graph, CEq wrapper 0xe8b0. Inactive/bypassed
        // stages preserve the upstream flag and their retained histories.
        void processNative(double* left,double* right,int frames,bool& silent,double fillScale=1.0) noexcept
        {
            if(peakEnabled && peakSilence.process(left,right,frames,silent,1e-6*fillScale))
                for(int i=0;i<frames;++i)
                {
                    left[i]=nativePeak.process(left[i],0);
                    if(right)right[i]=nativePeak.process(right[i],1);
                }
            const bool bypass=nativeTone.isBypassed();
            if(!bypass && toneSilence.process(left,right,frames,silent,1e-6*fillScale))
                for(int i=0;i<frames;++i)
                {
                    left[i]=nativeTone.process(left[i],0);
                    if(right)right[i]=nativeTone.process(right[i],1);
                }
        }

        template <typename ProcessContext>
        void process (const ProcessContext& context) noexcept
        {
            auto& inputBlock  = context.getInputBlock();
            auto& outputBlock = context.getOutputBlock();
            if (context.isBypassed)
            {
                outputBlock.copyFrom (inputBlock);
                return;
            }
            auto numChannels  = inputBlock.getNumChannels();
            auto numSamples   = inputBlock.getNumSamples();

            for (size_t chan = 0; chan < numChannels; ++chan)
            {
                auto* in  = inputBlock.getChannelPointer (chan);
                auto* out = outputBlock.getChannelPointer (chan);

                for (size_t i = 0; i < numSamples; ++i)
                    out[i] = processSample (in[i], static_cast<int> (chan));
            }
        }

    private:
        OutputTone nativeTone;
        SilenceGate peakSilence,toneSilence;
        double sampleRate { 44100.0 };
        EqualizerSettings previous {};
        PeakEq nativePeak;
        bool cached = false, peakEnabled = false;
    };
}
