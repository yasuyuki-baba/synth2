#pragma once
#include "dsp/AudioOversampler.h"
#include "audio/VoiceBank.h"
#include "dsp/SharedModulationRuntime.h"

#include "RenderControls.h"
#include "dsp/OutputPipeline.h"
#include "render/RenderHistory.h"
#include "SynthVoice.h"
#include "VoiceCorePool.h"
#include "VoiceDispatcher.h"

namespace synth2
{
    class SynthSound final : public ::synth2::audio::Sound
    {
    public:
        bool appliesToNote (int) override { return true; }
        bool appliesToChannel (int) override { return true; }
    };

    class SynthEngine
    {
    public:
        SynthEngine();
        ~SynthEngine() = default;

        void prepareToPlay (float sampleRate, int samplesPerBlock);
        void releaseResources();
        void setOversamplingFactor(int factor) noexcept;
        int getOversamplingFactor() const noexcept {return audioFactor;}
        void requestPhaseReset();
        void restartFilterCutoff() noexcept;
        void applyReceiverParameter(int index,float normalized) noexcept;
        void applyFilterParameter(int index,float normalized) noexcept;
        void updateFilterEnvelopeConfiguration() noexcept;
        void requestNativeLfoRateUpdate(size_t index) noexcept { sharedModulation.touchRate(index); }
        void applyNativeLfoHistory(const dsp::LfoHistoryEvent&) noexcept;

        void render (::synth2::audio::AudioBuffer<float>& buffer,
                     ::synth2::audio::MidiBuffer& midiMessages,
                     int startSample,
                     int numSamples);

        // Called once after the synthetic embedded Init, before external loads/audio.
        void initializeNativeRandomHistory() noexcept { sharedModulation.initializeRandomHistory(); }
        void invalidateNativeRandomHistory() noexcept { sharedModulation.invalidateRandomHistory(); }
        bool nativeRandomHistoryValid() const noexcept { return sharedModulation.randomState().valid; }
        std::array<int,4> nativeBendWords() const noexcept {return synth.nativeBendWords();}
        std::array<int,4> pitchBendWords(int channel) const noexcept {return synth.pitchBendWords(channel); }
        const dsp::RandomLfoRuntime& getNativeRandomLfos() const noexcept { return sharedModulation.randomState(); }

        void setRenderHistory(const render::RenderHistory& value) noexcept { renderHistory=value; }
        const render::RenderHistory& getRenderHistory() const noexcept { return renderHistory; }

        RenderControls& getParams() { return params; }
        const RenderControls& getParams() const { return params; }
        const dsp::LfoClock& getNativeLfoClock(size_t index) const noexcept { return sharedModulation.clock(index); }
        std::array<float,7> getNativeMultiEffectState() const noexcept { return outputPipeline.multiEffectState(); }
        bool readCoreWords(size_t index,std::array<uint32_t,9>& words) const noexcept { return corePool.readCoreWords(index,words); }
        bool readCoreEnvelopeWords(size_t index,std::array<int,13>& words) const noexcept { return corePool.readCoreEnvelopeWords(index,words); }
        bool readCoreFloatWords(size_t index,std::array<uint32_t,22>& words) const noexcept { return corePool.readCoreFloatWords(index,words); }
        bool readRenderingCoreWords(size_t voice,size_t core,std::array<uint32_t,16>& words) const noexcept
        {
            if(voice>=static_cast<size_t>(synth.getNumVoices()))return false;
            const auto* logical=dynamic_cast<const SynthVoice*>(synth.getVoice(static_cast<int>(voice)));
            return logical!=nullptr && logical->readRenderingCoreWords(core,words);
        }

        bool readPhysicalCoarseWords(size_t core,std::array<uint32_t,5>& words) const noexcept
        {return corePool.readCoreCoarseWords(core,words);}
        bool readPhysicalFmWords(size_t core,std::array<uint32_t,7>& words) const noexcept
        {return corePool.readCoreFmWords(core,words);}
        bool readRenderingCoreFloatWords(size_t voice,size_t core,std::array<uint32_t,22>& words) const noexcept
        {
            if(voice>=static_cast<size_t>(synth.getNumVoices()))return false;
            const auto* logical=dynamic_cast<const SynthVoice*>(synth.getVoice(static_cast<int>(voice)));
            return logical!=nullptr && logical->readRenderingCoreFloatWords(core,words);
        }

        bool readRenderingFmWords(size_t voice,size_t core,std::array<uint32_t,7>& words) const noexcept
        {
            if(voice>=static_cast<size_t>(synth.getNumVoices()))return false;
            const auto* logical=dynamic_cast<const SynthVoice*>(synth.getVoice(static_cast<int>(voice)));
            return logical!=nullptr && logical->readRenderingFmWords(core,words);
        }

        bool readRenderingStackWords(size_t voice,size_t core,std::array<uint32_t,17>& words) const noexcept
        {
            if(voice>=static_cast<size_t>(synth.getNumVoices()))return false;
            const auto* logical=dynamic_cast<const SynthVoice*>(synth.getVoice(static_cast<int>(voice)));
            return logical!=nullptr && logical->readRenderingStackWords(core,words);
        }

    private:
        void initWaveTables (float sampleRate);
        int audioFactor=1;
        std::array<std::array<float,256*16>,2> oversampledAudio{};
        std::array<dsp::AudioDownsampler,2> audioDownsamplers;
        RenderControls params;
        render::RenderHistory renderHistory;
        dsp::SharedModulationRuntime sharedModulation;
        dsp::OscillatorPhaseRandom phaseRandom;
        VoiceCorePool corePool;
        VoiceDispatcher synth;
        
        dsp::OutputPipeline outputPipeline;

        std::unique_ptr<synth2::WaveTableBank> waveTableBank;
        float currentSampleRate { 44100.0f };
        uint64_t globalSampleCount { 0 };
        int previousPlayMode { 0 };
        

    };
}
