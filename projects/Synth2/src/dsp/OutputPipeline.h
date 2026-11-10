#pragma once
#include "OutputPipelineSettings.h"
#include "ReceiverControls.h"
#include "Equalizer.h"
#include "EffectPair.h"
#include "OutputGain.h"
#include "PanLfo.h"
#include "Phaser.h"
#include "MultiEffect.h"
#include "AudioOversampler.h"

namespace synth2::dsp
{
    class OutputPipeline
    {
    public:
        // The engine invokes preparation phases at their existing lifecycle
        // boundaries. Keeping them separate preserves rate-change retention.
        void setOversamplingFactor(int factor) noexcept
        {audioOversampler.setFactor(factor);nativeMultiEffect.setOversamplingFactor(factor);}
        void prepareGain(float rate,bool preserve);
        void prepareProcessors(float rate,bool preserve);
        void prepareEffects(const ::synth2::audio::ProcessSpec&,bool preserveVoices,bool rateChanged);
        void release(bool retainTone);
        std::array<float,7> multiEffectState() const noexcept { return nativeMultiEffect.snapshot(); }
        void process(::synth2::audio::AudioBuffer<float>&,int start,int frames,const OutputPipelineSettings&,
            const OutputProcessContext&,const ReceiverLfoView& panControls);
    private:
        bool processToneAndEffects(::synth2::audio::AudioBuffer<float>&,int,int,const OutputPipelineSettings&,
            bool inputSilent,bool graphSilent);
        void processBalance(::synth2::audio::AudioBuffer<float>&,int,int,const OutputPipelineSettings&,bool graphSilent,const ReceiverLfoView&);
        void processWetEffects(::synth2::audio::AudioBuffer<float>&,int,int,const OutputPipelineSettings&,bool graphSilent);
        void processGain(::synth2::audio::AudioBuffer<float>&,int,int,int capacity);
        StereoOversampler audioOversampler;
        Equalizer equalizer;
        EffectPair wetEffects;
        OutputGain outputGain;
        PanLfo nativePan;
        Phaser nativePhaser;
        MultiEffect nativeMultiEffect;
    };
}
