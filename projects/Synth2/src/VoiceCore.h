#pragma once
#include "dsp/OscillatorSection.h"
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
        dsp::FilterEnvelope filterEnvelope;
        dsp::CutoffControl cutoff;
        dsp::OscillatorPhaseState phase;
        dsp::AmplitudeEnvelope amplitude {true};
        dsp::FilterEnvelope modulationEnvelope;
        dsp::Portamento portamento;
        dsp::FrequencyModulation fm;
        dsp::OscillatorCoarsePitch coarse;
        float amplitudeReferenceVelocity {1.0f};
        double keyboardOffset {},unisonOffset {};
        int physicalSlot {-1};
        std::array<uint32_t,22> readFloatWords() const noexcept
        {
            const auto gains=amplitude.readGainState();const auto memory=filter.readNativeMemory();
            std::array<float,21> values{gains[0],gains[1],gains[2],amplitudeReferenceVelocity};
            std::copy(memory.begin(),memory.end(),values.begin()+4);
            std::array<uint32_t,22> words{};std::memcpy(words.data(),values.data(),sizeof(values));
            words[21]=static_cast<uint32_t>(static_cast<int64_t>(amplitude.getCachedLevel()));return words;
        }
        void prepare (const ::synth2::audio::ProcessSpec& spec,bool preservePhase=false)
        {
            oscillator.prepare (spec);
            filter.prepare (spec);
            filterEnvelope.reset();
            cutoff.prepare (static_cast<int>(spec.sampleRate));
            coarse.elapsed=static_cast<int>(spec.sampleRate)/20;
            if(!preservePhase) phase.pending = true;
            amplitude.reset();
            modulationEnvelope.reset();
            if(!preservePhase) portamento = {};
        }
    };

}
