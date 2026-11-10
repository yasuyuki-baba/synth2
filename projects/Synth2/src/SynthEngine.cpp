#include "offline/RenderDiagnostics.h"
#include "render/ReceiverParameterMapping.h"
#if defined(SYNTH2_HOST_PROFILING)
#include "HostCallbackProfile.h"
#endif
#include "dsp/OscillatorTables.h"
#include "dsp/OscillatorTuning.h"
#include "SynthEngine.h"
#include "patch/Synth1ControlPromotion.h"
#include "render/ControllerMapping.h"
#include "render/OutputConfiguration.h"
#include "render/SharedModulationConfiguration.h"
#include <cmath>

namespace synth2
{
    SynthEngine::SynthEngine() : synth(sharedModulation.arpeggioRandom())
    {
        synth.params = &params;
        synth.corePool = &corePool;
        synth.modulation=&sharedModulation;
        synth.controllerSettings=&renderHistory.controllerRoutes;
        waveTableBank = std::make_unique<synth2::WaveTableBank>();
        
        for (int i = 0; i < 32; ++i)
        {
            auto* voice = new SynthVoice (params,phaseRandom,corePool);
            voice->setWaveTableBank (waveTableBank.get());
            synth.addVoice (voice);
        }
            
        synth.addSound (new SynthSound());
    }

    void SynthEngine::applyReceiverParameter(int index,float normalized) noexcept
    {
        if(render::filterParameter(index)){applyFilterParameter(index,normalized);return;}
        const float previousFine=params.globalFineTune;
        render::applyReceiverParameter(params,index,promoteNormalizedSource(normalized),renderHistory.nativeMidiRoutingLoadHistory);
        if(index==2)corePool.setOsc2Coarse(dsp::OscillatorTuning::coarseExponent(params.osc2PitchSemis),false);
        if(index==45)corePool.setControllerDial(45,static_cast<int>(currentSampleRate));
        // OSC2 Fine/tracking republishes the current coarse frequency; master
        // Fine does so only when its resolved fine value actually changes.
        if(index==3 || index==4 || (index==72 && params.globalFineTune!=previousFine))corePool.refreshOsc2Frequency();
    }

    void SynthEngine::applyFilterParameter(int index,float normalized) noexcept
    {
        render::applyFilterParameter(params,index,promoteNormalizedSource(normalized),renderHistory.nativeMidiRoutingLoadHistory);
        updateFilterEnvelopeConfiguration();
        restartFilterCutoff();
    }

    void SynthEngine::updateFilterEnvelopeConfiguration() noexcept
    {
        for(int i=0;i<synth.getNumVoices();++i)
            if(auto* voice=dynamic_cast<SynthVoice*>(synth.getVoice(i));voice && voice->isVoiceActive())
                voice->updateFilterEnvelopeConfiguration();
    }

    void SynthEngine::restartFilterCutoff() noexcept
    {
        corePool.restartFilterCutoff();
    }

    void SynthEngine::requestPhaseReset()
    {
        synth.controllers.reset(renderHistory.controllerResetOnLoad);
        const float exponent=dsp::OscillatorTuning::coarseExponent(params.osc2PitchSemis);
        corePool.setOsc2Coarse(exponent,true);
        // Full loads restart the native cutoff/FM receiver clocks even when
        // dials are equal; sounding physical cores retain their current values.
        for(int destination:{19,45})
        {
            corePool.setControllerDial(destination,static_cast<int>(currentSampleRate));
        }
        sharedModulation.requestReload();
        corePool.requestPhaseReset();
    }

    void SynthEngine::setOversamplingFactor(int factor) noexcept
    {
        if(!dsp::OversamplingSettings::valid(factor) || (factor==1 && audioFactor==1))return;
        audioFactor=factor;
        corePool.setAudioFactor(currentSampleRate,factor);
        for(auto& channel:audioDownsamplers)channel.setFactor(factor);
        outputPipeline.setOversamplingFactor(factor);
        for(int i=0;i<synth.getNumVoices();++i)
            if(auto* voice=dynamic_cast<SynthVoice*>(synth.getVoice(i)))
                voice->setAudioOutput(factor,oversampledAudio[0].data(),oversampledAudio[1].data());
    }

    void SynthEngine::prepareToPlay (float sampleRate, int samplesPerBlock)
    {
        // The native host's mains/sample-rate/block-size cycle retains held
        // voices and physical allocation. Keep that history for the audited
        // sine-tremolo route, updating component rates without resetting state.
        const bool rateChanged = currentSampleRate != sampleRate;
        const bool preserveVoices = !sharedModulation.isInitial()
            && params.playMode == 0
            && (params.lfo1.on || params.lfo2.on);
        if(!preserveVoices || rateChanged)for(auto& channel:audioDownsamplers)channel.reset();
        currentSampleRate = sampleRate;
        outputPipeline.prepareGain(sampleRate,preserveVoices);
        if (!preserveVoices)
        {
            synth.allNotesOff (0, false);
            synth.resetAllocation();
        }
        synth.setPlaybackRate (sampleRate, preserveVoices);
        synth.arpeggiator.prepare (sampleRate);
        // The native sample-rate setter preserves running clocks and applies
        // one sample of slew before rescaling their period/counter.
        sharedModulation.retimeClocks(sampleRate);
        globalSampleCount = 0;
        params.globalSampleCount = 0;
        previousPlayMode = params.playMode;
        outputPipeline.prepareProcessors(sampleRate,!sharedModulation.isInitial());
        sharedModulation.prepareBlock(samplesPerBlock);

        initWaveTables (sampleRate);
        
        ::synth2::audio::ProcessSpec spec { sampleRate, (uint32_t) samplesPerBlock, 2 };
        // Native idle reprepare retains physical oscillator phase requests.
        // Keep this behavior within the audited imported sine-tremolo route.
        const bool preservePhase=!sharedModulation.isInitial() && params.playMode==0
            && params.unisonVoices==1 && params.polyphony==1
            && (params.lfo1.on || params.lfo2.on);
        if (!preserveVoices) corePool.prepare(spec,preservePhase);
        else if (rateChanged) corePool.updateSampleRate(spec);
        outputPipeline.prepareEffects(spec,preserveVoices,rateChanged);
        
        for (int i = 0; i < synth.getNumVoices(); ++i)
        {
            if (auto* voice = dynamic_cast<SynthVoice*> (synth.getVoice(i))) {
                if (!preserveVoices) voice->prepare (sampleRate);
                else if (rateChanged) voice->updateSampleRate (sampleRate);
                voice->setWaveTableBank (waveTableBank.get());
                voice->setGlobalLfoBlock (&sharedModulation.block());
            }
        }
        if(audioFactor>1)setOversamplingFactor(audioFactor);
    }

    void SynthEngine::initWaveTables (float sampleRateForTables)
    {
        const float nyquist = sampleRateForTables * 0.5f;

        waveTableBank->setAliasBorderHz ((float) nyquist);
        
        constexpr int N = 2048;
        constexpr int halfN = N / 2;

        const std::vector<float> sinTable(dsp::oscillatorSine.begin(),dsp::oscillatorSine.end());

        const auto nativeSaw = dsp::buildNativeSaw (sinTable);
        auto square = nativeSaw;
        for (const auto& info : square.infos)
            for (int n = 0; n < N; ++n)
                square.data[info.offset+n] = .5f * (static_cast<float>(nativeSaw.data[info.offset+n])
                    - static_cast<float>(nativeSaw.data[info.offset+((n+halfN)&(N-1))]));
        waveTableBank->setShapeTables (1,std::move (square));
        waveTableBank->setShapeTables (5,nativeSaw);
        waveTableBank->setShapeTables (0,nativeSaw);
        waveTableBank->setShapeTables (2,dsp::buildNativeTriangle (sinTable));
        synth2::ShapeTables sineShape;
        sineShape.data.assign(sinTable.begin(),sinTable.end());
        sineShape.infos.push_back ({ 0u, (std::size_t) N, (float) nyquist });
        waveTableBank->setShapeTables (3,std::move (sineShape));
    }

    void SynthEngine::releaseResources()
    {
        // Keep native tone delay samples until the next prepare can decide
        // whether this is a rate change. Other prepares reset EQ as before.
        const bool retainTone = !sharedModulation.isInitial() && params.playMode == 0
            && (params.lfo1.on || params.lfo2.on);
        outputPipeline.release(retainTone);
    }

    void SynthEngine::applyNativeLfoHistory(const dsp::LfoHistoryEvent& event) noexcept
    { sharedModulation.applyHistory(event,params.hostBpm); }

    void SynthEngine::render (::synth2::audio::AudioBuffer<float>& buffer,
                             ::synth2::audio::MidiBuffer& midiMessages,
                             int startSample,
                             int numSamples)
    {
        // Running MIDI offsets must not enter mapper caches or saved presets.
        struct RestoreMappedControls
        {
            RenderControls& target;RenderControls base;const uint64_t& sampleCount;
            ~RestoreMappedControls(){target=base;target.globalSampleCount=sampleCount;}
        } restore{params,params,globalSampleCount};
        synth.nativeOmniInput=renderHistory.nativeMidiRoutingLoadHistory;
        synth.controllers.applyActive(renderHistory.controllerRoutes,[&](int destination,int dial) {
            render::applyControllerDial(params,destination,dial);
        });
        const auto sharedSettings=render::sharedModulation(params,renderHistory);
        sharedModulation.synchronize(sharedSettings,currentSampleRate);
        corePool.setPhaseDial(params.oscillatorPhaseDial);
        corePool.setGroupCount(params.unisonVoices);
        // Prepare each shared sample once, including silence and MIDI subregions.
        // Hosts may send more samples than their advertised block size. Chunking
        // reuses prepared storage without allocating on the audio thread.
        const int capacity = sharedModulation.capacity();
        int audioChunkStart=startSample;
        auto convertAudio=[&](int start,int count) {
            bool nonzero=false;
            for(int frame=0;frame<count;++frame)
            {
                std::array<float,2> output{};
                for(int channel=0;channel<2;++channel)
                {
                    std::array<float,16> values{};
                    for(int step=0;step<audioFactor;++step)
                        values[step]=oversampledAudio[channel][(start+frame-audioChunkStart)*audioFactor+step];
                    output[channel]=audioDownsamplers[channel].process(values);
                    nonzero=nonzero || output[channel]!=0;
                }
                if(buffer.getNumChannels()==1)buffer.setSample(0,start+frame,(output[0]+output[1])*.5f);
                else for(int channel=0;channel<2;++channel)buffer.setSample(channel,start+frame,output[channel]);
            }
            return !nonzero;
        };
        auto processOutput=[&](int outputStart,int outputFrames,bool inputSilent) {
       #if defined(SYNTH2_HOST_PROFILING)
        hostprofile::Scope phase(hostprofile::current.effects);
       #endif
        if(audioFactor>1)inputSilent=convertAudio(outputStart,outputFrames);
        const auto settings=render::outputSettings(params);
        const auto panControls=settings.panLfo
            ? render::outputPanControls(params,&sharedModulation.block(),outputStart) : dsp::ReceiverLfoView{};
        outputPipeline.process(buffer,outputStart,outputFrames,settings,
            {currentSampleRate,capacity,offline::bypassEffects(),inputSilent},panControls);
        };
        if (previousPlayMode != params.playMode)
        {
            if(!renderHistory.nativeMidiRoutingLoadHistory)synth.allNotesOff (0, false);
            previousPlayMode = params.playMode;
        }
        assert (capacity > 0);
        if (capacity <= 0) return;
        const auto outputSink=[](void* context,int start,int count,bool silent) {
            (*static_cast<decltype(processOutput)*>(context))(start,count,silent);
        };
        for (int offset = 0; offset < numSamples;)
        {
            // Win64 processing bridge RVA 0x19e00 limits each synth call
            // to 256 frames, including hosts that supply larger blocks.
            const int size = std::min ({capacity,256,numSamples - offset});
            audioChunkStart=startSample+offset;
            if(audioFactor>1)
            {
                for(auto& channel:oversampledAudio)std::fill_n(channel.data(),size*audioFactor,0.f);
                for(int i=0;i<synth.getNumVoices();++i)
                    if(auto* voice=dynamic_cast<SynthVoice*>(synth.getVoice(i)))voice->setAudioChunkStart(audioChunkStart);
            }
            sharedModulation.beginChunk(startSample+offset);
            {
               #if defined(SYNTH2_HOST_PROFILING)
                hostprofile::Scope phase(hostprofile::current.synthesis);
               #endif
                synth.renderRegion (buffer, midiMessages, startSample + offset, size,
                    &processOutput,+outputSink);
            }
            globalSampleCount += static_cast<uint64_t> (size);
            params.globalSampleCount = globalSampleCount;
            offset += size;
        }

    }
}
