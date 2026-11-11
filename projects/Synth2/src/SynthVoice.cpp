#include "offline/RenderDiagnostics.h"
#include "dsp/Unison.h"
#include "SynthVoice.h"
#include "VoiceCorePool.h"
#include "render/VoiceConfiguration.h"
#include "render/VoiceAudioConfiguration.h"
#include "render/VoiceTuningConfiguration.h"
#include "render/VoiceLifecycleConfiguration.h"
#include "dsp/AmplitudeBlockRenderer.h"
#include "dsp/OscillatorTuning.h"
#include <cmath>
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

    SynthVoice::SynthVoice (RenderControls& sharedParams, dsp::OscillatorPhaseRandom& sharedPhaseRandom,
                            VoiceCorePool& sharedPool)
        : params (sharedParams), phaseRandom (sharedPhaseRandom), pool (sharedPool)
    {
    }

    void SynthVoice::releaseCores() noexcept
    {
        for (auto* core : cores) if (core != nullptr) pool.release(*this,*core);
        cores.fill(nullptr);
        assignedCount = 0;
    }

    void SynthVoice::retireNote() noexcept
    {
        // Relinquish ownership but retain read-only views of the last physical
        // group. The next assignment replaces these views in releaseCores().
        for(auto* core:cores)if(core!=nullptr)pool.release(*this,*core);
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

    void SynthVoice::prepare (float newSampleRate)
    {
        retireNote();
        lifecycle.consumeTransition();
        sampleRate = newSampleRate;
        ::synth2::audio::ProcessSpec spec { newSampleRate, 512, 2 };

        if (sampleRate > 0) {
            amplifier.prepare (spec);
            dsp::ModulationEnvelope::prepare();
            dsp::FrequencyModulation::prepare();
        }

    }

    void SynthVoice::updateSampleRate (float newSampleRate)
    {
        sampleRate = newSampleRate;
        // Imported native envelopes reconfigure their durations without note-on.
        // Logical ownership and physical oscillator phases remain untouched.
        if (isVoiceActive()) updateEnvelopeConfiguration();
    }

    void SynthVoice::startNote (int midiNoteNumber, float velocity,
                                 ::synth2::audio::Sound*, int)
    {
        const auto transition=lifecycle.next();
        lifecycle.noteStarted();
        midiNote = midiNoteNumber;
        level = velocity;

        const bool nativeGlide = !params.portamentoAuto || transition.glide;

        const int activeCores = std::clamp (params.unisonVoices, 1, 8);
        auto startCore = [&](int i)
        {
            auto& core = *cores[i];
            core.filterMidiNote=midiNoteNumber;core.noteHeld=true;
            const bool wasActive=core.amplitude.isActive();
            const auto reuse=dsp::CoreReusePolicy::select(wasActive);
            core.portamento.setDuration(dsp::Portamento::timeSeconds(params.sourcePortamentoDial),static_cast<int>(sampleRate));
            core.portamento.setNote(midiNoteNumber,nativeGlide,
                static_cast<int>(params.octaveBase*12+params.keyShift));
            core.oscillator.params.osc1Wave = mapWave(params.osc1WaveIndex);
            core.oscillator.params.osc2Wave = mapWave(params.osc2WaveIndex);
            const float coarse=dsp::OscillatorTuning::coarseExponent(params.osc2PitchSemis);
            if(core.coarse.target!=coarse)core.coarse.set(coarse,false);
            core.coarse.noteOn();
            core.phase.setDial(params.oscillatorPhaseDial);
            if (core.phase.start(phaseRandom,activeCores,i,params.unisonPhaseDial))
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
            configureAmplitude(core,true);
            core.amplitude.noteOn();
            configureModulationEnvelope(core);
            core.modulationEnvelope.noteOn();
            if(reuse.settleFm)core.fm.settle=true;
            core.fm.noteOn();
        };
        if (transition.legato())
        {
            // Mono binds the first physical group. A larger group retains the
            // continuing cores and admits its additional cores as new notes.
            const auto previous=cores;
            std::array<bool,8> started {};
            startingNote=true;
            releaseCores();
            assignedCount=activeCores;
            for(int i=0;i<activeCores;++i)
            {
                cores[i]=pool.claim(*this,32,i);
                const bool continuing=cores[i]->noteHeld && std::find(previous.begin(),previous.end(),cores[i])!=previous.end();
                if(!continuing){startCore(i);started[i]=true;}
                else
                {
                    auto& core=*cores[i];
                    core.coarse.refresh();
                    core.portamento.setDuration(dsp::Portamento::timeSeconds(params.sourcePortamentoDial),static_cast<int>(sampleRate));
                    core.portamento.setNote(midiNoteNumber,nativeGlide,
                        static_cast<int>(params.octaveBase*12+params.keyShift));
                }
            }
            startingNote=false;
            updateEnvelopeConfiguration();
            for(int i=0;i<activeCores;++i)
                if(started[i])
                {cores[i]->filterEnvelope.noteOn();cores[i]->cutoff.settle();}
            lifecycle.consumeTransition();
            return;
        }
        lifecycle.consumeTransition();
        releaseCores();
        startingNote = true;
        assignedCount = activeCores;
        for (int i = 0; i < activeCores; ++i)
        {
            cores[i] = pool.claim(*this,params.playMode!=0 ? 32 : params.polyphony,params.playMode!=0 ? i : -1);
            startCore(i);
        }
        startingNote = false;
        updateEnvelopeConfiguration();

        for (auto* core : cores) if (core != nullptr) { core->filterEnvelope.noteOn(); core->cutoff.settle(); }

    }

    void SynthVoice::stopNote (float, bool allowTailOff)
    {
        const auto action=lifecycle.stop(allowTailOff);
        if(action==dsp::VoiceLifecycle::StopAction::Preserve)return;
        if(action==dsp::VoiceLifecycle::StopAction::Release)
        {
            for(auto* core:cores)if(core!=nullptr)core->releaseKey();
        }
        else
        {
            // An immediate stop relinquishes the logical voice. During
            // a legato reassignment startVoice replaces its note bookkeeping.
            // Native mono retriggers from the sounding envelope/filter state;
            // Logical bookkeeping replacement must not reset that physical core.
            for (auto* core : cores) if (core != nullptr) { core->filterEnvelope.reset(); core->amplitude.reset(); core->modulationEnvelope.reset(); core->noteHeld=false; }
            retireNote();
        }
    }

    void SynthVoice::pitchWheelMoved (int) {}

    void SynthVoice::controllerMoved (int, int) {}

    void SynthVoice::renderNextBlock (::synth2::audio::AudioBuffer<float>& outputBuffer,
                                       int startSample,
                                       int numSamples)
    {
        if (sampleRate <= 0.0f || lifecycle.phase()==dsp::VoiceLifecycle::Phase::Idle) return;
        if (numSamples > 256)
        {
            for (int offset = 0; offset < numSamples; offset += 256)
                renderNextBlock (outputBuffer,startSample+offset,std::min (256,numSamples-offset));
            return;
        }
        // Snapshot physical activity before VCA advancement. A core finishing
        // its release still renders this complete span; an already idle core
        // keeps its oscillator, filter and receiver history until reuse.
        // Logical note activity and requested Unison layout remain separate.
        const bool bypassEnv = offline::bypassEnvelopes();
        bool amplitudeActive = false;
        std::array<bool,8> renderCore {};
        for(int v=0;v<activeCoreCount();++v)if(cores[v]!=nullptr)
        {
            const bool active=cores[v]->amplitude.isActive();
            amplitudeActive=amplitudeActive || active;
            renderCore[v]=bypassEnv || active;
        }
        if (!bypassEnv && !amplitudeActive)
        {
            retireNote();
            return;
        }

        updateEnvelopeConfiguration();

        if(bypassEnv)renderedSignal=true;

        const auto lfoView=render::receiverLfos(globalLfoBlock,startSample,params.lfo1,params.lfo2);
        const auto oscillatorControl=render::oscillatorControls(params,sampleRate,bypassEnv);
        const auto filterControl=render::filterControls(params,bypassEnv);
        const int bend=pitchBendOffset;
        const auto oscillatorAudio=render::oscillatorAudio(params,bypassEnv,bend!=0);
        const auto filterAudio=render::filterAudio(params,bypassEnv);
        std::array<std::array<float,256>,8> nativeCoarsePitch;
        std::array<dsp::VoiceControlBuffer,8> nativeGlidePitch,nativeModPitch,nativeFmDepth,nativeCutoffIndices;
        std::array<dsp::VoiceSpanBuffer,8> nativeModRemaining;
        // Preserve the native stage order across all cores. Child controllers
        // retain separate oscillator-envelope/glide/FM and filter partitions.
        for(int v=0;v<activeCoreCount();++v)if(renderCore[v])
            dsp::OscillatorControlRenderer::envelope(oscillatorControl,lfoView,cores[v]->modulationEnvelope,
                numSamples,nativeModPitch[v],nativeModRemaining[v]);
        for(int v=0;v<activeCoreCount();++v)if(renderCore[v])
            dsp::OscillatorControlRenderer::glide(oscillatorControl,lfoView,cores[v]->portamento,
                numSamples,nativeModRemaining[v],nativeGlidePitch[v]);
        for(int v=0;v<activeCoreCount();++v)if(renderCore[v])
            dsp::OscillatorControlRenderer::fm(oscillatorControl,lfoView,cores[v]->fm,
                numSamples,nativeModPitch[v],nativeModRemaining[v],nativeFmDepth[v],
                &cores[v]->coarse,&nativeCoarsePitch[v]);
        for(int v=0;v<activeCoreCount();++v)if(renderCore[v])
            dsp::FilterControlRenderer::render(filterControl,lfoView,cores[v]->filterEnvelope,cores[v]->cutoff,
                cores[v]->keyboardOffset,cores[v]->unisonOffset,numSamples,nativeCutoffIndices[v]);

        // Native oscillator input is rendered for the entire callback before
        // the integer amplitude stage retires the voice. Keep those phase ticks.
        // The native workspace advances by two words per frame. Retain this
        // scheduling coordinate independently of Float32 gain/audio storage.
        const int gainOffset=!bypassEnv ? (((startSample-nativeWorkspaceOrigin)*2)&3) : 0;
        alignas(16) std::array<std::array<float,260>,8> amplitudeGains;
        if (!bypassEnv)
            for (int v = 0; v < activeCoreCount(); ++v) if (renderCore[v])
            {
                // Projected signals and generated ARP events retain gain progress.
                // Ordinary receiver spans keep their actual dispatch length.
                const bool coreRendered=dsp::AmplitudeBlockRenderer::render(
                    render::amplitudeBlockSettings(gainOffset,amplitudeGainHorizon,amplitudeGainBoundary),lfoView,
                    cores[v]->amplitude,numSamples,amplitudeGains[v].data());
                renderedSignal=renderedSignal || coreRendered;
            }

        for (auto* core : cores) if (core != nullptr) core->phase.setDial (params.oscillatorPhaseDial);

        const dsp::VoiceTuning tuning(render::voiceTuning(params,bypassEnv),midiNote);
        const int numUnison=tuning.unisonCount();
        const float velocityGain = dsp::AmplitudeEnvelope::velocityForDials(
            static_cast<int>(std::lround(level*127)),dsp::fractionalDial(params.ampVelocity));
        const float sy1AmpGain = dsp::AmplitudeEnvelope::gainForDial(dsp::fractionalDial(params.ampGainMultiplier));

        const auto& unison=tuning.unison;
        std::array<dsp::VoiceTuning::Core,8> coreTuning;
        std::array<float,8> coarseFrequencies;
        std::array<float,8> corePans;
        for(int v=0;v<activeCoreCount();++v)if(renderCore[v])
        {
            coreTuning[v]=tuning.prepareCore(unison.pitchCents[layoutSlot(v)]);
            corePans[v]=dsp::Unison::panOffset(numUnison,layoutSlot(v),params.unisonPan);
            dsp::FilterAudioRenderer::configure(cores[v]->filter,filterAudio);
        }

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
            float firstPitch=0,secondPitch=0;
            if(lfoView.available)for(size_t slot=0;slot<2;++slot)
                if(lfoView.receivers[slot].enabled)
                {
                    const auto destination=lfoView.receivers[slot].destination;
                    const float value=lfoView.values[slot][i];
                    if(destination==1)firstPitch+=value;
                    if(destination==0 || destination==1)secondPitch+=value;
                }

            float combinedL = 0.0f;
            float combinedR = 0.0f;
            std::array<float,16> highLeft,highRight;
            if(audioFactor>1)
            {
                std::fill_n(highLeft.data(),audioFactor,0.0f);
                std::fill_n(highRight.data(),audioFactor,0.0f);
            }

            for (int v = 0; v < activeCoreCount(); ++v)
            {
                if (!renderCore[v]) continue;
                const float panAmt = corePans[v];

                auto coreFrequency=coreTuning[v];
                if(nativeCoarsePitch[v][i]!=cores[v]->coarse.target)
                {
                    // Base/fine inputs are fixed for this render span. Only a
                    // changed receiver exponent needs another frequency map.
                    // First use always assigns; no cache crosses span boundaries.
                    if(i==0 || nativeCoarsePitch[v][i]!=nativeCoarsePitch[v][i-1])
                        coarseFrequencies[v]=tuning.coarseSecond(coreTuning[v],nativeCoarsePitch[v][i],unison.pitchCents[layoutSlot(v)]);
                    coreFrequency.second=coarseFrequencies[v];
                }
                const dsp::OscillatorAudioInput oscillatorInput {
                    coreFrequency.first,coreFrequency.second,params.pulseWidth,params.oscMix,params.subOscGain,syncEnabled,ringEnabled,
                    firstPitch+bend,
                    secondPitch+bend,
                    nativeGlidePitch[v][i],nativeModPitch[v][i],
                    nativeFmDepth[v][i],
                    0};
                float vEnveloped;
                if(audioFactor==1)
                {
                    const float oscSignal=dsp::OscillatorAudioRenderer::process(cores[v]->oscillator,
                        oscillatorAudio,oscillatorInput,lfoView,i,waveTableBankPtr);

                    const float vFiltered=dsp::FilterAudioRenderer::process(cores[v]->filter,filterAudio,
                        {oscSignal,nativeCutoffIndices[v][i]});

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
                        oscillatorAudio,oscillatorInput,lfoView,i,waveTableBankPtr,oscillatorFrame.data(),factor);
                    const float gain=bypassEnv ? 1.f : amplitudeGains[v][static_cast<size_t>(i+gainOffset)];
                    for(int step=0;step<factor;++step)
                    {
                        const float signal=oscillatorFrame[step];
                        const float filtered=dsp::FilterAudioRenderer::process(cores[v]->filter,filterAudio,
                            {signal,nativeCutoffIndices[v][i]})*gain;
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
        for(int v=0;v<activeCoreCount();++v)if(renderCore[v])cores[v]->filter.finishNativeBlock();
        if (!bypassEnv)
        {
            const int count = activeCoreCount();
            const bool finished=std::all_of(cores.begin(),cores.begin()+count,
                [](auto* core) {return core==nullptr || !core->amplitude.isActive();});
            for(int v=0;v<count;++v)if(renderCore[v] && !cores[v]->amplitude.isActive())
            {
                // Native VCO envelope history survives stealing while sounding,
                // but the VCA's completed tail resets it before idle reuse.
                cores[v]->fm.settle=true; // CVca end37f00 invokes CVco idle marker3dfd0.
                cores[v]->modulationEnvelope.reset();
                if(params.resetIdleFilter)
                {
                    cores[v]->filterEnvelope.reset();
                    cores[v]->cutoff.settle();
                }
                pool.release(*this,*cores[v]);
                // A surviving logical note must not keep a writable view of a
                // freed lane that another note can claim. Retired whole notes
                // retain their read-only inspection views as before.
                if(!finished)cores[v]=nullptr;
            }
            if(finished)retireNote();
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
            const int filterNote=core.filterMidiNote;
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
