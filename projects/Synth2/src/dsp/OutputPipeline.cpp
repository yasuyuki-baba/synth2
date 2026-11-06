#include "StereoBalance.h"
#include "OutputPipeline.h"

namespace synth2::dsp
{
    void OutputPipeline::prepareGain(double rate,bool preserve)
    { outputGain.prepare(rate,preserve); }
    void OutputPipeline::prepareProcessors(double rate,bool preserve)
    {
        if(!preserve)audioOversampler.reset();
        nativePan.prepare(rate,preserve);
        nativePhaser.prepare(rate,preserve);
        nativeMultiEffect.prepare(rate,preserve);
    }
    void OutputPipeline::prepareEffects(const ::synth2::audio::ProcessSpec& spec,bool preserveVoices,bool rateChanged)
    {
        if(preserveVoices && rateChanged)equalizer.updateSampleRate(spec.sampleRate);
        else equalizer.prepare(spec);
        if(spec.sampleRate>=10000.0 && spec.sampleRate<=384000.0)wetEffects.prepare(spec.sampleRate);
        else wetEffects.reset();
    }
    void OutputPipeline::release(bool retainTone)
    {
        if(!retainTone)equalizer.reset();
        wetEffects.reset();
    }
    void OutputPipeline::process(::synth2::audio::AudioBuffer<float>& buffer,int outputStart,int outputFrames,
        const OutputPipelineSettings& settings,const OutputProcessContext& context,const ReceiverLfoView& panControls)
    {
        const bool useWetEffects=!context.bypassEffects
            && context.sampleRate>=10000.0 && context.sampleRate<=384000.0;
        bool graphSilent=settings.nativeSilenceGraph && context.inputSilent;
        if(!context.bypassEffects)
            graphSilent=processToneAndEffects(buffer,outputStart,outputFrames,settings,
                context.inputSilent,graphSilent);
        processBalance(buffer,outputStart,outputFrames,settings,graphSilent,panControls);
        if(useWetEffects)processWetEffects(buffer,outputStart,outputFrames,settings,graphSilent);
        processGain(buffer,outputStart,outputFrames,context.gainBlockCapacity);
    }

    bool OutputPipeline::processToneAndEffects(::synth2::audio::AudioBuffer<float>& buffer,int outputStart,int outputFrames,
        const OutputPipelineSettings& settings,bool inputSilent,bool graphSilent)
    {
        const bool nativeSilenceGraph=settings.nativeSilenceGraph;
        const bool nativeGainSilence=settings.nativeGainSilence;
        nativeMultiEffect.setDials(settings.nativeMultiEffectDials[0],settings.nativeMultiEffectDials[1],settings.nativeMultiEffectDials[2]);
        const bool highRateDrive=audioOversampler.getFactor()>1 && settings.nativeMultiEffectType>=0 && settings.nativeMultiEffectType<=2;
        if(audioOversampler.getFactor()>1)
        {
            auto* left=buffer.getWritePointer(0,outputStart);
            auto* right=buffer.getNumChannels()>1 ? buffer.getWritePointer(1,outputStart) : nullptr;
            bool nonzero=false;
            for(int i=0;i<outputFrames;++i)
            {
                const auto output=audioOversampler.process(left[i],right ? right[i] : left[i],
                    [&](float* l,float* r,int count) {
                        if(highRateDrive)
                        {
                            for(int k=0;k<count;++k){l[k]/=OutputGain::defaultTarget;r[k]/=OutputGain::defaultTarget;}
                            nativeMultiEffect.process(settings.nativeMultiEffectType,l,r,count,false);
                            for(int k=0;k<count;++k){l[k]*=OutputGain::defaultTarget;r[k]*=OutputGain::defaultTarget;}
                        }
                    });
                left[i]=output[0];if(right)right[i]=output[1];
                nonzero=nonzero || output[0]!=0 || output[1]!=0;
            }
            inputSilent=!nonzero;
            graphSilent=false; // Prepared FIR/drive tails must drain during idle input.
        }
        if(!highRateDrive && settings.nativeMultiEffectType>=0 && settings.nativeMultiEffectType<=5)
        {
            // Nonlinear effects consume the native VCA level. The existing
            // voice trim represents the master stage after these devices.
            buffer.applyGain(outputStart,outputFrames,1.0f/OutputGain::defaultTarget);
            const bool effectSilent=nativeMultiEffect.process(settings.nativeMultiEffectType,buffer.getWritePointer(0,outputStart),
                buffer.getNumChannels()>1 ? buffer.getWritePointer(1,outputStart) : nullptr,outputFrames,inputSilent);
            graphSilent=nativeSilenceGraph && effectSilent;
            buffer.applyGain(outputStart,outputFrames,OutputGain::defaultTarget);
        }
        nativePhaser.setDials(settings.nativePhaserDepth,settings.nativePhaserRate,settings.nativePhaserFeedback,settings.nativePhaserPairs);
        if(settings.nativePhaser)
        {
            graphSilent=nativeSilenceGraph && inputSilent;
            nativePhaser.process(buffer.getWritePointer(0,outputStart),
                buffer.getNumChannels()>1 ? buffer.getWritePointer(1,outputStart) : nullptr,outputFrames,graphSilent);
        }
        if(audioOversampler.getFactor()>1)graphSilent=false;
        equalizer.setParams(settings.eq);
        if(nativeSilenceGraph)equalizer.processNative(buffer.getWritePointer(0,outputStart),
            buffer.getNumChannels()>1 ? buffer.getWritePointer(1,outputStart) : nullptr,outputFrames,graphSilent,
            nativeGainSilence ? OutputGain::defaultTarget*settings.masterVolume : 1.0f);
        else
        {
            ::synth2::audio::AudioBlock<float> block(buffer);
            auto subBlock=block.getSubBlock(static_cast<std::size_t>(outputStart),static_cast<std::size_t>(outputFrames));
            ::synth2::audio::ProcessContextReplacing<float> context(subBlock);
            equalizer.process(context);
        }
        return graphSilent;
    }

    void OutputPipeline::processBalance(::synth2::audio::AudioBuffer<float>& buffer,int outputStart,int outputFrames,
        const OutputPipelineSettings& settings,bool graphSilent,const ReceiverLfoView& panControls)
    {
        if(settings.panLfo && !graphSilent && buffer.getNumChannels() >= 2) {
            for(int i=0;i<outputFrames;++i) {
                const double target=(panControls.receivers[0].enabled && panControls.receivers[0].destination == 6 ? panControls.values[0][i] : 0)
                    +(panControls.receivers[1].enabled && panControls.receivers[1].destination == 6 ? panControls.values[1][i] : 0);
                float l=buffer.getSample(0,outputStart+i),r=buffer.getSample(1,outputStart+i);
                nativePan.process(l,r,target,settings.pan);
                buffer.setSample(0,outputStart+i,l);buffer.setSample(1,outputStart+i,r);
            }
        }
        else if(!settings.panLfo)
        {
            // Ordinary balance follows nonlinear MultiFX and Tone too.
            // Applying it per voice changes quantizer/drive input levels.
            const auto balance=StereoBalance::gains(settings.pan);
            const float left=balance.left,right=balance.right;
            if(buffer.getNumChannels()>=2)
            {
                buffer.applyGain(0,outputStart,outputFrames,left);
                buffer.applyGain(1,outputStart,outputFrames,right);
            }
            else buffer.applyGain(0,outputStart,outputFrames,(left+right)*.5f);
        }
    }

    void OutputPipeline::processWetEffects(::synth2::audio::AudioBuffer<float>& buffer,int outputStart,int outputFrames,
        const OutputPipelineSettings& settings,bool graphSilent)
    {
        const bool nativeSilenceGraph=settings.nativeSilenceGraph;
        wetEffects.setParams(settings.delay,settings.chorus,settings.hostBpm);
        if(nativeSilenceGraph)wetEffects.process(buffer.getWritePointer(0,outputStart),
            buffer.getNumChannels()>1 ? buffer.getWritePointer(1,outputStart) : nullptr,outputFrames,graphSilent);
        else wetEffects.process(buffer.getWritePointer(0,outputStart),
            buffer.getNumChannels()>1 ? buffer.getWritePointer(1,outputStart) : nullptr,outputFrames);
    }

    void OutputPipeline::processGain(::synth2::audio::AudioBuffer<float>& buffer,int outputStart,int outputFrames,int capacity)
    {
        // The existing voice trim represents the default native master
        // target. Apply only its startup ratio here, retaining the current
        // calibrated effect input levels and public master-volume control.
        constexpr float target = OutputGain::defaultTarget;
        for (int offset = 0; offset < outputFrames;)
        {
            const int size = std::min ({capacity, 256, outputFrames - offset});
            for (int i = 0; i < size; ++i)
            {
                const float left = outputGain.next (target) / target;
                const float right = outputGain.next (target) / target;
                const int frame = outputStart + offset + i;
                if (buffer.getNumChannels() >= 2)
                {
                    buffer.setSample (0, frame, buffer.getSample (0, frame) * left);
                    buffer.setSample (1, frame, buffer.getSample (1, frame) * right);
                }
                else if (buffer.getNumChannels() == 1)
                    buffer.setSample (0, frame, buffer.getSample (0, frame) * (left + right) * 0.5f);
            }
            outputGain.finish (target);
            offset += size;
        }
    }

}
