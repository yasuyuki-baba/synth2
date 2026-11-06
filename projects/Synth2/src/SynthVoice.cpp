#include "dsp/VelocityResponse.h"
#include "dsp/Unison.h"
#include "SynthVoice.h"
#include "VoiceCorePool.h"
#include "dsp/ModulationRouter.h"
#include "render/VoiceConfiguration.h"
#include "render/VoiceAudioConfiguration.h"
#include "render/VoiceTuningConfiguration.h"
#include "render/VoiceLfoConfiguration.h"
#include "render/VoiceLifecycleConfiguration.h"
#include "dsp/AmplitudeBlockRenderer.h"
#include "dsp/OscillatorTuning.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "dsp/DspUtils.h"

namespace synth2
{
    namespace
    {
        static int mapWave (float norm)
        {
            if (norm > 1.0f) return 4; // Noise
            if (norm <= 0.25f) return 0; // Saw
            if (norm <= 0.50f) return 1; // Square
            if (norm <= 0.75f) return 2; // Tri
            return 3;                    // Sine
        }
    }

    SynthVoice::SynthVoice (RenderControls& sharedParams, dsp::OscillatorPhaseRandom* sharedPhaseRandom,
                            VoiceCorePool* sharedPool)
        : params (sharedParams), phaseRandom (sharedPhaseRandom != nullptr ? sharedPhaseRandom : &localPhaseRandom), pool (sharedPool)
    {
        for (size_t i = 0; i < cores.size(); ++i) cores[i] = &localCores[i];
    }

    void SynthVoice::releaseCores() noexcept
    {
        if (pooled)
            for (auto* core : cores) if (core != nullptr) pool->release(*this,*core);
        for (size_t i = 0; i < cores.size(); ++i) cores[i] = &localCores[i];
        pooled = false; assignedCount = 0;
    }

    void SynthVoice::retireNote() noexcept
    {
        // Relinquish ownership but retain read-only views of the last physical
        // group. The next assignment replaces these views in releaseCores().
        if(pooled)for(auto* core:cores)if(core!=nullptr)pool->release(*this,*core);
        lifecycle.retired();
        clearCurrentNote();
    }

    void SynthVoice::detachCore (VoiceCore& stolen) noexcept
    {
        for (auto& core : cores) if (core == &stolen) core = nullptr;
        if (!startingNote && std::all_of(cores.begin(),cores.end(),[](auto* core) { return core == nullptr; }))
            stopNote(0,false);
    }

    bool SynthVoice::canPlaySound (::synth2::audio::Sound* sound)
    {
        return true;
    }

    void SynthVoice::prepare (double newSampleRate)
    {
        retireNote();
        lifecycle.consumeTransition();
        sampleRate = newSampleRate;
        ::synth2::audio::ProcessSpec spec { newSampleRate, 512, 2 };

        if (sampleRate > 0) {
            for (auto& core : localCores)
                core.prepare (spec);

            amplifier.prepare (spec);
            mixer.prepare (spec);
            dsp::ModulationEnvelope::prepare();
            dsp::FrequencyModulation::prepare();
            dsp::OscillatorTuning::prepare();
        }

        requestPhaseReset();
    }

    void SynthVoice::updateSampleRate (double newSampleRate)
    {
        sampleRate = newSampleRate;
        for(auto& core:localCores)core.coarse.elapsed=static_cast<int>(newSampleRate)/20;
        // Imported native envelopes reconfigure their durations without note-on.
        // Logical ownership and physical oscillator phases remain untouched.
        if (isVoiceActive()) updateEnvelopeConfiguration();
    }

    void SynthVoice::startNote (int midiNoteNumber, float velocity,
                                 ::synth2::audio::Sound*, int currentPitchWheelPosition)
    {
        const auto transition=lifecycle.next();
        lifecycle.noteStarted();
        midiNote = midiNoteNumber;
        if(!transition.legato())filterMidiNote=midiNoteNumber;
        level = velocity;
        if(currentPitchWheelPosition!=8192 || localPitchBend.current!=0 || localPitchBend.moving())
            pitchWheelMoved(currentPitchWheelPosition);

        const bool nativeGlide = !params.portamentoAuto || transition.glide;

        if (transition.legato())
        {
            for(auto* core:cores) if(core != nullptr)
            {
                if(params.nativeVoicePerformance)core->coarse.refresh();
                core->portamento.setDuration(dsp::Portamento::timeSeconds(params.sourcePortamentoDial),static_cast<int>(sampleRate));
                core->portamento.setNote(midiNoteNumber,nativeGlide,
                    static_cast<int>(params.octaveBase*12+params.keyShift));
            }
            updateEnvelopeConfiguration();
            lifecycle.consumeTransition();
            return;
        }
        lifecycle.consumeTransition();

        releaseCores();
        const int activeCores = std::clamp (params.unisonVoices, 1, 8);
        const auto reuseSettings=render::coreReuse(params);
        auto startCore = [&](int i)
        {
            auto& core = *cores[i];
            const bool wasActive=core.amplitude.isActive();
            const auto reuse=dsp::CoreReusePolicy::select(reuseSettings,wasActive);
            core.portamento.setDuration(dsp::Portamento::timeSeconds(params.sourcePortamentoDial),static_cast<int>(sampleRate));
            core.portamento.setNote(midiNoteNumber,nativeGlide,
                static_cast<int>(params.octaveBase*12+params.keyShift));
            core.oscillator.params.osc1Wave = mapWave(params.osc1WaveIndex);
            core.oscillator.params.osc2Wave = mapWave(params.osc2WaveIndex);
            const float coarse=dsp::OscillatorTuning::coarseExponent(params.osc2PitchSemis);
            if(core.coarse.target!=coarse)core.coarse.set(coarse,false);
            core.coarse.noteOn();
            core.phase.setDial(params.oscillatorPhaseDial);
            if (i < activeCores && core.phase.start(*phaseRandom,activeCores,i,params.unisonPhaseDial))
            {
                core.oscillator.resetFmPhase();
                core.oscillator.osc1.setPhaseFixed27(core.phase.osc1);
                core.oscillator.osc2.setPhaseFixed27(core.phase.osc2);
                core.oscillator.stackPhases=core.phase.lanes;
                core.oscillator.subSharedPhase=core.phase.osc1;
            }
            // Physical core reuse is separate from logical note state.
            if(reuse.resetFilter)core.filter.reset();
            if(reuse.resetFilterEnvelope)core.filterEnvelope.reset();
            if (i < activeCores)
            {
                configureAmplitude(core,true);
                core.amplitude.noteOn();
                configureModulationEnvelope(core);
                core.modulationEnvelope.noteOn();
                if(reuse.settleFm)core.fm.settle=true;
                core.fm.noteOn();
            }
        };
        if (pool != nullptr && (params.playMode == 0 || params.nativeVoicePerformance))
        {
            pooled = startingNote = true; assignedCount = activeCores;
            cores.fill(nullptr);
            for (int i = 0; i < activeCores; ++i)
            {
                cores[i] = pool->claim(*this,params.playMode!=0 ? 32 : params.polyphony,params.playMode!=0 ? i : -1);
                startCore(i);
            }
            startingNote = false;
        }
        else
            for (int i = 0; i < 8; ++i) startCore(i);
        updateEnvelopeConfiguration();


        for (auto* core : cores) if (core != nullptr) { core->filterEnvelope.noteOn(); core->cutoff.settle(); }

        mixer.reset();

    }

    void SynthVoice::stopNote (float, bool allowTailOff)
    {
        const auto action=lifecycle.stop(allowTailOff);
        if(action==dsp::VoiceLifecycle::StopAction::Preserve)return;
        if(action==dsp::VoiceLifecycle::StopAction::Release)
        {
            for (auto* core : cores) { if (core != nullptr) core->amplitude.noteOff(); }
            for (auto* core : cores) if (core != nullptr) core->filterEnvelope.noteOff();
        }
        else
        {
            // An immediate stop relinquishes the logical voice. During
            // a legato reassignment startVoice replaces its note bookkeeping.
            // Native mono retriggers from the sounding envelope/filter state;
            // Logical bookkeeping replacement must not reset that physical core.
            for (auto* core : cores) if (core != nullptr) { core->filterEnvelope.reset(); core->amplitude.reset(); core->modulationEnvelope.reset(); }
            retireNote();
        }
    }

    void SynthVoice::pitchWheelMoved (int newPitchWheelValue)
    {
        localPitchBend.set(newPitchWheelValue);
    }

    void SynthVoice::controllerMoved (int, int) {}

    void SynthVoice::renderNextBlock (::synth2::audio::AudioBuffer<float>& outputBuffer,
                                       int startSample,
                                       int numSamples)
    {
        if (sampleRate <= 0.0 || lifecycle.phase()==dsp::VoiceLifecycle::Phase::Idle) return;
        if(pitchBendSource==nullptr && localPitchBend.moving() && numSamples>32)
        {
            for(int offset=0;offset<numSamples;)
            {
                const int span=localPitchBend.moving() ? std::min(32,numSamples-offset) : numSamples-offset;
                renderNextBlock(outputBuffer,startSample+offset,span);
                offset+=span;
            }
            return;
        }
        if (numSamples > 256)
        {
            for (int offset = 0; offset < numSamples; offset += 256)
                renderNextBlock (outputBuffer,startSample+offset,std::min (256,numSamples-offset));
            return;
        }
        // Envelope retirement and logical note state keep separate lifetimes.
        static bool bypassEnv = (std::getenv("SYNTH2_BYPASS_ENV") != nullptr);
        const bool nativeVcaLfo = !bypassEnv && globalLfoBlock != nullptr
            && ((params.lfo1.nativeSharedClock && params.lfo1.dest == 3) || (params.lfo2.nativeSharedClock && params.lfo2.dest == 3));
        bool amplitudeActive = false;
        for (int v = 0; v < activeCoreCount(); ++v)
            if (cores[v] != nullptr) amplitudeActive = amplitudeActive || cores[v]->amplitude.isActive();
        if (!bypassEnv && !amplitudeActive)
        {
            retireNote();
            return;
        }

        updateEnvelopeConfiguration();

        if(bypassEnv)renderedSignal=true;

        const auto lfoView=render::receiverLfos(globalLfoBlock,startSample,params.lfo1,params.lfo2);
        const auto modulationRouting=render::modulationRouting(params,nativeVcaLfo,lfoView.available);
        auto audioLfos=lfoView;
        for(size_t slot=0;slot<2;++slot)audioLfos.receivers[slot].enabled=modulationRouting.consumedLfos[slot];
        const auto voiceLfoConfig=render::voiceLfoConfig(params);
        const auto voiceLfoView=render::voiceLfoInput(globalLfoBlock,startSample,audioLfos);
        const auto oscillatorControl=render::oscillatorControls(params,sampleRate,bypassEnv);
        const auto filterControl=render::filterControls(params,bypassEnv);
        std::array<dsp::ModulationRouter::ModResults,256> modulation;
        dsp::VoiceControlBuffer additionalFm{},additionalFilter{};
        dsp::AmplitudeControls additionalAmplitude{};
        bool hasFm=false,hasFilter=false,hasAmplitude=false;
        for(int i=0;i<numSamples;++i)
        {
            const auto input=dsp::VoiceLfoRenderer::sample(voiceLfoConfig,voiceLfoView,i);
            modulation[i]=dsp::ModulationRouter::calculate(modulationRouting,input.values[0],input.values[1]);
            additionalFm[i]=modulation[i].fmControl;additionalFilter[i]=modulation[i].filterControl;
            hasFm=hasFm || additionalFm[i]!=0;hasFilter=hasFilter || additionalFilter[i]!=0;
            for(size_t slot=0;slot<2;++slot)
            {additionalAmplitude[slot][i]=modulation[i].amplitudeControl[slot];hasAmplitude=hasAmplitude || additionalAmplitude[slot][i]!=0;}
        }
        if(pitchBendSource==nullptr && localPitchBend.moving())localPitchBend.advance(static_cast<int>(params.pitchBendRange));
        const int bend=pitchBendSource!=nullptr ? pitchBendSource->offset : localPitchBend.offset;
        const auto oscillatorAudio=render::oscillatorAudio(params,bypassEnv,bend!=0);
        const auto filterAudio=render::filterAudio(params,bypassEnv);
        std::array<std::array<float,256>,8> nativeCoarsePitch;
        std::array<dsp::VoiceControlBuffer,8> nativeGlidePitch,nativeModPitch,nativeFmDepth,nativeCutoffIndices;
        std::array<dsp::VoiceSpanBuffer,8> nativeModRemaining;
        // Preserve the native stage order across all cores. Child controllers
        // retain separate oscillator-envelope/glide/FM and filter partitions.
        for(int v=0;v<activeCoreCount();++v)if(cores[v])
            dsp::OscillatorControlRenderer::envelope(oscillatorControl,lfoView,cores[v]->modulationEnvelope,
                numSamples,nativeModPitch[v],nativeModRemaining[v]);
        for(int v=0;v<activeCoreCount();++v)if(cores[v])
            dsp::OscillatorControlRenderer::glide(oscillatorControl,lfoView,cores[v]->portamento,
                numSamples,nativeModRemaining[v],nativeGlidePitch[v]);
        for(int v=0;v<activeCoreCount();++v)if(cores[v])
            dsp::OscillatorControlRenderer::fm(oscillatorControl,lfoView,cores[v]->fm,
                numSamples,nativeModPitch[v],nativeModRemaining[v],nativeFmDepth[v],
                &cores[v]->coarse,&nativeCoarsePitch[v],
                hasFm ? &additionalFm : nullptr);
        for(int v=0;v<activeCoreCount();++v)if(cores[v])
            dsp::FilterControlRenderer::render(filterControl,lfoView,cores[v]->filterEnvelope,cores[v]->cutoff,
                cores[v]->keyboardOffset,cores[v]->unisonOffset,numSamples,nativeCutoffIndices[v],
                hasFilter ? &additionalFilter : nullptr);

        // Native oscillator input is rendered for the entire callback before
        // the integer amplitude stage retires the voice. Keep those phase ticks.
        // The native root advances its interleaved workspace by two floats
        // per frame before the mono VCA receiver processes each subregion.
        const int gainOffset=!bypassEnv ? (((startSample-nativeWorkspaceOrigin)*2)&3) : 0;
        alignas(16) std::array<std::array<float,260>,8> amplitudeGains;
        if (!bypassEnv)
            for (int v = 0; v < activeCoreCount(); ++v) if (cores[v] != nullptr)
            {
                // Projected signals and generated ARP events retain gain progress.
                // Ordinary receiver spans keep their actual dispatch length.
                const bool coreRendered=dsp::AmplitudeBlockRenderer::render(
                    render::amplitudeBlockSettings(modulationRouting,gainOffset,amplitudeGainHorizon,amplitudeGainBoundary),lfoView,
                    cores[v]->amplitude,numSamples,amplitudeGains[v].data(),
                    hasAmplitude ? &additionalAmplitude : nullptr);
                renderedSignal=renderedSignal || coreRendered;
            }

        for (auto* core : cores) if (core != nullptr) core->phase.setDial (params.oscillatorPhaseDial);

        const dsp::VoiceTuning tuning(render::voiceTuning(params,bypassEnv),midiNote);
        const int numUnison=tuning.unisonCount();
        const float velocityGain = dsp::ampVelocityGain (level, params.ampVelocity);
        const float sy1AmpGain = params.ampGainMultiplier <= 0.0f ? 0.0f
            : std::pow (2048.0f, std::pow (params.ampGainMultiplier, 0.3f) - 1.0f);

        const auto& unison=tuning.unison;
        std::array<dsp::VoiceTuning::Core,8> coreTuning;
        for(int v=0;v<activeCoreCount();++v)if(cores[v]!=nullptr)
            coreTuning[v]=tuning.prepareCore(unison.pitchCents[layoutSlot(v)]);

        for (int v = 0; v < activeCoreCount(); ++v) {
            if (cores[v] == nullptr) continue;
            // Per-voice parameter caching
            cores[v]->oscillator.params.osc1Wave = mapWave(params.osc1WaveIndex);
            cores[v]->oscillator.stackDial = params.osc1StackDial;
            cores[v]->oscillator.params.osc2Wave = mapWave(params.osc2WaveIndex);
        }

        const bool syncEnabled = params.osc2Sync > 0.0f;
        const bool ringEnabled = params.ringMod > 0.0f;

            for (int i = 0; i < numSamples; ++i)
            {
                float mixValue = params.oscMix;
                float subGainValue = params.subOscGain;

                const auto lfoInput=dsp::VoiceLfoRenderer::sample(voiceLfoConfig,voiceLfoView,i);

                const auto& mods=modulation[i];

                // Update frequencies, PW and Pan
                float curPw = params.pulseWidth;

                float combinedL = 0.0f;
                float combinedR = 0.0f;
                std::array<float,16> highLeft{},highRight{};

                for (int v = 0; v < activeCoreCount(); ++v)
                {
                    if (cores[v] == nullptr) continue;
                    const float panAmt = dsp::Unison::panOffset (numUnison,layoutSlot(v),params.unisonPan);

                    auto coreFrequency=coreTuning[v];
                    if(nativeCoarsePitch[v][i]!=cores[v]->coarse.target)
                        coreFrequency.second=tuning.coarseSecond(coreTuning[v],nativeCoarsePitch[v][i],unison.pitchCents[layoutSlot(v)]);
                    const dsp::OscillatorAudioInput oscillatorInput {
                        coreFrequency.first,coreFrequency.second,curPw,mixValue,subGainValue,syncEnabled,ringEnabled,
                        lfoInput.firstPitch+bend+mods.firstPitch,
                        lfoInput.secondPitch+bend+mods.secondPitch,
                        nativeGlidePitch[v][i],nativeModPitch[v][i],
                        nativeFmDepth[v][i],
                        mods.pulseControl};
                    float vEnveloped;
                    if(audioFactor==1)
                    {
                        const float oscSignal=dsp::OscillatorAudioRenderer::process(cores[v]->oscillator,
                            oscillatorAudio,oscillatorInput,audioLfos,i,waveTableBankPtr);

                        float vCombined = mixer.mix(oscSignal, (random.nextFloat() * 2.0f - 1.0f), params.noiseLevel);

                        const float vFiltered=dsp::FilterAudioRenderer::process(cores[v]->filter,filterAudio,
                            {vCombined,nativeCutoffIndices[v][i]});

                        // Central gain stage: apply envelope to the signal before panner/unison sum.
                        vEnveloped = vFiltered;
                        if (!bypassEnv)
                            vEnveloped *= amplitudeGains[v][static_cast<size_t>(i+gainOffset)];
                    }
                    else
                    {
                        const int factor=audioFactor;
                        std::array<float,16> oscillatorFrame{};
                        dsp::OscillatorAudioRenderer::process(cores[v]->oscillator,
                            oscillatorAudio,oscillatorInput,audioLfos,i,waveTableBankPtr,oscillatorFrame.data(),factor);
                        const float noise=random.nextFloat()*2.0f-1.0f;
                        const float gain=bypassEnv ? 1.f : amplitudeGains[v][static_cast<size_t>(i+gainOffset)];
                        for(int step=0;step<factor;++step)
                        {
                            const float signal=oscillatorFrame[step];
                            const float mixed=mixer.mix(signal,noise,params.noiseLevel);
                            const float filtered=dsp::FilterAudioRenderer::process(cores[v]->filter,filterAudio,
                                {mixed,nativeCutoffIndices[v][i]})*gain;
                            float left,right;amplifier.processSample(filtered,1.f,panAmt,left,right);
                            highLeft[step]+=left;highRight[step]+=right;
                        }
                        continue;
                    }

                    float vL, vR;
                    amplifier.processSample (vEnveloped, 1.0f, panAmt, vL, vR);
                    combinedL += vL;
                    combinedR += vR;
                }

            // Static gain/velocity are cached per segment; envelope and
            // modulation remain sample-varying and are applied exactly once.
            float overallGain = bypassEnv
                ? sy1AmpGain * params.masterVolume * velocityGain
                : params.masterVolume;

            if(audioFactor>1)
            {
                for(int step=0;step<audioFactor;++step)
                {
                    audioLeft[(startSample+i-audioChunkStart)*audioFactor+step]+=highLeft[step]*overallGain;
                    audioRight[(startSample+i-audioChunkStart)*audioFactor+step]+=highRight[step]*overallGain;
                }
                continue;
            }
            combinedL *= overallGain;
            combinedR *= overallGain;

            // Native resonant voices exceed unity. Preserve floating-point
            // headroom instead of introducing per-voice hard distortion.
            const float outL = combinedL;
            const float outR = combinedR;

            if (outputBuffer.getNumChannels() >= 2) {
                outputBuffer.addSample (0, startSample + i, outL);
                outputBuffer.addSample (1, startSample + i, outR);
            } else {
                outputBuffer.addSample (0, startSample + i, (outL + outR) * 0.5f);
            }

        }
        for (auto* core : cores) if (core != nullptr) core->filter.finishNativeBlock();
        if (!bypassEnv)
        {
            const int count = activeCoreCount();
            for (int v = 0; v < count; ++v) if (cores[v] != nullptr && !cores[v]->amplitude.isActive())
            {
                // Native VCO envelope history survives stealing while sounding,
                // but the VCA's completed tail resets it before idle reuse.
                cores[v]->fm.settle=true; // CVca end37f00 invokes CVco idle marker3dfd0.
                cores[v]->modulationEnvelope.reset();
                if(params.nativeIdleFilterReset)
                {
                    cores[v]->filterEnvelope.reset();
                    cores[v]->cutoff.settle();
                }
                if (pooled) pool->release(*this,*cores[v]);
            }
            if (std::all_of(cores.begin(),cores.begin()+count,[](auto* core) { return core == nullptr || !core->amplitude.isActive(); })) retireNote();
        }

    }

    void SynthVoice::configureAmplitude (VoiceCore& core,bool noteAssignment) const noexcept
    {
        dsp::VoiceEnvelopeConfigurator::amplitude(core.amplitude,core.amplitudeReferenceVelocity,
            render::amplitudeEnvelope(params),level,sampleRate,noteAssignment);
    }

    void SynthVoice::configureModulationEnvelope (VoiceCore& core) const noexcept
    {
        dsp::VoiceEnvelopeConfigurator::modulation(core.modulationEnvelope,render::modulationEnvelope(params),sampleRate);
    }

    void SynthVoice::updateEnvelopeConfiguration()
    {
        // Every caller configures the same caller-owned physical VCA state.
        for (int v = 0; v < activeCoreCount(); ++v) if (cores[v] != nullptr)
        {
            configureAmplitude(*cores[v]);configureModulationEnvelope(*cores[v]);
        }
        const float coarse=dsp::OscillatorTuning::coarseExponent(params.osc2PitchSemis);
        for(int v=0;v<activeCoreCount();++v)if(auto* core=cores[v];core!=nullptr && core->coarse.target!=coarse)
            core->coarse.set(coarse,core->modulationEnvelope.isActive());
        updateFilterEnvelopeConfiguration();
    }

    void SynthVoice::updateFilterEnvelopeConfiguration()
    {
        const auto filterSettings=render::filterEnvelope(params);
        const auto layout = dsp::Unison::layout(params.unisonVoices,
            dsp::fractionalDial(params.unisonDetune),params.unisonPitchDial);
        for (int v = 0; v < activeCoreCount(); ++v)
        {
            if (cores[v] == nullptr) continue;
            auto& core = *cores[v];
            core.unisonOffset = layout.filterIndex[layoutSlot(v)];
            // CVcf keeps its note while a legato assignment retunes only CVCO.
            const int filterNote=params.nativeFilterEnvelope ? filterMidiNote : midiNote;
            dsp::VoiceEnvelopeConfigurator::filter(core.filterEnvelope,core.keyboardOffset,core.unisonOffset,
                filterSettings,filterNote,level,sampleRate);
        }
        // VCO pitch decay continues after MIDI note-off, while VCA releases.
    }

    void SynthVoice::setWaveTableBank (const synth2::WaveTableBank* bank)
    {
        waveTableBankPtr = bank;
    }
}
