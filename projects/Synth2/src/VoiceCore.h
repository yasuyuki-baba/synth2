#pragma once
#include "dsp/OscillatorSection.h"
#include "dsp/OversamplingSettings.h"
#include "dsp/Filter.h"
#include "dsp/FilterEnvelope.h"
#include "dsp/CutoffControl.h"
#include "dsp/OscillatorPhase.h"
#include "dsp/AmplitudeEnvelope.h"
#include "dsp/Portamento.h"
#include "dsp/FrequencyModulation.h"
#include "dsp/OscillatorCoarsePitch.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace synth2
{
    struct VoiceCore
    {
        dsp::OscillatorSection oscillator;
        dsp::Filter filter;
        int audioFactor=1;
        void setAudioFactor(float rate,int factor) noexcept
        {
            if(!dsp::OversamplingSettings::valid(factor))return;
            audioFactor=factor;
            oscillator.prepare({rate,512,2});
            filter.setSampleRate(rate*factor);
        }
        dsp::FilterEnvelope filterEnvelope;
        dsp::CutoffControl cutoff;
        dsp::OscillatorPhaseState phase;
        dsp::AmplitudeEnvelope amplitude {true};
        dsp::FilterEnvelope modulationEnvelope;
        dsp::Portamento portamento;
        dsp::FrequencyModulation fm;
        dsp::OscillatorCoarsePitch coarse;
        float amplitudeReferenceVelocity {1.0f};
        float keyboardOffset {},unisonOffset {};
        bool noteHeld {false};
        void releaseKey() noexcept
        {
            if(!noteHeld)return;
            noteHeld=false;amplitude.noteOff();filterEnvelope.noteOff();
        }
        int filterMidiNote {60};
        int physicalSlot {-1};
        std::array<uint32_t,22> readFloatWords() const noexcept
        {
            const auto gains=amplitude.readGainState();const auto memory=filter.readNativeMemory();
            std::array<float,21> values{static_cast<float>(gains[0]),static_cast<float>(gains[1]),static_cast<float>(gains[2]),static_cast<float>(amplitudeReferenceVelocity)};
            std::copy(memory.begin(),memory.end(),values.begin()+4);
            std::array<uint32_t,22> words{};std::memcpy(words.data(),values.data(),sizeof(values));
            words[21]=static_cast<uint32_t>(static_cast<int64_t>(amplitude.getCachedLevel()));return words;
        }
        void prepare (const ::synth2::audio::ProcessSpec& spec,bool preservePhase=false)
        {
            auto audioSpec=spec;audioSpec.sampleRate*=audioFactor;
            oscillator.prepare (spec);
            filter.prepare (audioSpec);
            filterEnvelope.reset();
            cutoff.prepare (static_cast<int>(spec.sampleRate));
            coarse.elapsed=static_cast<int>(spec.sampleRate)/20;
            if(!preservePhase) phase.pending = true;
            amplitude.reset();noteHeld=false;
            modulationEnvelope.reset();
            if(!preservePhase) portamento = {};
        }
    };

}
