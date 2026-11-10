#pragma once
#include "audio/VoiceBank.h"
#include "RenderControls.h"
#include "VoiceCore.h"
#include "dsp/VoiceLifecycle.h"
#include "dsp/Amplifier.h"
#include "dsp/LfoBlock.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace synth2
{
    class VoiceCorePool;

    class SynthVoice : public ::synth2::audio::Voice
    {
    public:
        explicit SynthVoice (RenderControls& sharedParams, dsp::OscillatorPhaseRandom& sharedPhaseRandom,
                              VoiceCorePool& sharedPool);


        bool canPlaySound (::synth2::audio::Sound* sound) override;

        void prepare (float newSampleRate);
        void setAudioChunkStart(int start) noexcept {audioChunkStart=start;}
        void setAudioOutput(int factor,float* left,float* right) noexcept
        {audioFactor=factor;audioLeft=left;audioRight=right; }
        void updateSampleRate (float newSampleRate);

        void startNote (int midiNoteNumber, float velocity,
                        ::synth2::audio::Sound*, int) override;
        void stopNote (float velocity, bool allowTailOff) override;
        void pitchWheelMoved (int newPitchWheelValue) override;
        void updateFilterEnvelopeConfiguration();
        void controllerMoved (int controllerNumber, int newControllerValue) override;

        void renderNextBlock (::synth2::audio::AudioBuffer<float>& outputBuffer,
                              int startSample,
                              int numSamples) override;

        void setWaveTableBank (const synth2::WaveTableBank* bank);
        void setGlobalLfoBlock (const dsp::LfoBlock* block) { globalLfoBlock = block; }
        void setAmplitudeGainHorizon(int horizon,int boundary) noexcept
        { amplitudeGainHorizon=horizon;amplitudeGainBoundary=boundary; }
        void setRenderWorkspaceOrigin(int origin) noexcept { nativeWorkspaceOrigin=origin; }
        void beginOutputObservation() noexcept { renderedSignal=false; }
        bool didRenderSignal() const noexcept { return renderedSignal; }
        void setNextTransition(dsp::NoteTransition transition) noexcept { lifecycle.schedule(transition); }
        dsp::VoiceLifecycle::Phase getLifecyclePhase() const noexcept { return lifecycle.phase(); }
        float getCurrentFrequency() const noexcept
        {return cores[0]!=nullptr ? static_cast<float>(cores[0]->oscillator.osc1.getPhaseIncFixed27())*sampleRate/dsp::kPhaseScale27 : 0;}
        void setPitchBendOffset(int offset) noexcept {pitchBendOffset=offset;}
        // Offline read-only inspection of the core actually used by this
        // logical voice, using the same physical pool in every play mode.
        bool readRenderingCoreWords(size_t index,std::array<uint32_t,16>& words) const noexcept
        {
            if(index>=static_cast<size_t>(activeCoreCount()) || cores[index]==nullptr)return false;
            const auto& core=*cores[index];const auto phase=core.oscillator.readPhaseWords();
            words={{static_cast<uint32_t>(isVoiceActive()),static_cast<uint32_t>(midiNote),
                phase[0],phase[1],phase[2],phase[3],static_cast<uint32_t>(core.amplitude.getPhase()),
                static_cast<uint32_t>(core.amplitude.getLevel()),static_cast<uint32_t>(core.amplitude.getCounter()),
                static_cast<uint32_t>(core.filterEnvelope.getPhase()),static_cast<uint32_t>(core.filterEnvelope.getLevel()),
                static_cast<uint32_t>(core.filterEnvelope.getCounter()),static_cast<uint32_t>(core.cutoff.getCurrent()),
                static_cast<uint32_t>(core.cutoff.getElapsed()),static_cast<uint32_t>(core.cutoff.getAnchor()),
                static_cast<uint32_t>(core.physicalSlot)}};
            return true;
        }

        bool readRenderingCoreFloatWords(size_t index,std::array<uint32_t,22>& words) const noexcept
        {
            if(index>=static_cast<size_t>(activeCoreCount()) || cores[index]==nullptr)return false;
            words=cores[index]->readFloatWords();return true;
        }

        bool readRenderingFmWords(size_t index,std::array<uint32_t,7>& words) const noexcept
        {
            if(index>=static_cast<size_t>(activeCoreCount()) || cores[index]==nullptr)return false;
            const auto& fm=cores[index]->fm;
            words={static_cast<uint32_t>(static_cast<int64_t>(fm.current)),static_cast<uint32_t>(static_cast<int64_t>(fm.anchor)),static_cast<uint32_t>(fm.remaining),
                static_cast<uint32_t>(static_cast<int64_t>(fm.target)),static_cast<uint32_t>(fm.settle),static_cast<uint32_t>(fm.duration),static_cast<uint32_t>(fm.elapsed)};
            return true;
        }

        bool readRenderingStackWords(size_t index,std::array<uint32_t,17>& words) const noexcept
        {
            if(index>=static_cast<size_t>(activeCoreCount()) || cores[index]==nullptr)return false;
            const auto& oscillator=cores[index]->oscillator;
            const float depth=dsp::OscillatorStack::depth(oscillator.stackDial);
            std::memcpy(words.data(),&depth,4);
            std::array<float,8> frequencies{};
            std::copy(oscillator.stackFrequencies.begin(),oscillator.stackFrequencies.end(),frequencies.begin());
            std::memcpy(words.data()+1,frequencies.data(),32);
            std::copy(oscillator.stackPhases.begin(),oscillator.stackPhases.end(),words.begin()+9);
            return true;
        }

        int midiNote = 60;

    private:
        friend class VoiceCorePool;
        void releaseCores() noexcept;
        void detachCore (VoiceCore&) noexcept;
        void retireNote() noexcept;
        int activeCoreCount() const noexcept { return assignedCount; }
        int layoutSlot (int voice) const noexcept
        {
            const int index = cores[static_cast<size_t>(voice)]->physicalSlot;
            return (index >= 0 ? index : voice) % std::clamp(params.unisonVoices,1,8);
        }
        void updateEnvelopeConfiguration();
        void configureAmplitude (VoiceCore&,bool noteAssignment=false) const noexcept;
        void configureModulationEnvelope (VoiceCore&) const noexcept;

        RenderControls& params;
        dsp::OscillatorPhaseRandom& phaseRandom;
        float sampleRate { 0.0f };
        int nativeWorkspaceOrigin {};
        int amplitudeGainHorizon {},amplitudeGainBoundary {};
        int audioFactor=1,audioChunkStart=0;float* audioLeft=nullptr;float* audioRight=nullptr;
        bool renderedSignal {};
        int filterMidiNote {60};
        int pitchBendOffset {};
        dsp::VoiceLifecycle lifecycle;
        float  level  { 0.0f };

        std::array<VoiceCore*,8> cores {};
        VoiceCorePool& pool;
        int assignedCount {};
        bool startingNote {};

        dsp::Amplifier  amplifier;
        const dsp::LfoBlock* globalLfoBlock { nullptr };

        const synth2::WaveTableBank* waveTableBankPtr { nullptr };
        
    };
}
