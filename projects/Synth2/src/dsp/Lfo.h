#pragma once

#include "audio/AudioTypes.h"
#include "ModulationSettings.h"
#include "WaveTableBank.h"
#include "LfoShape.h"
#include <array>

namespace synth2::dsp
{
    /**
        Lfo: Modular LFO Processor.
        Handles waveform generation and state management (TempoSync).
    */
    class Lfo
    {
    public:
        explicit Lfo (uint32_t seed = 0xa341316cu) noexcept
            : randomSeed (seed != 0 ? seed : 0xa341316cu), randomState (randomSeed) {}

        void prepare (double sr) noexcept
        {
            sampleRate = sr;
            LfoShape::prepare();
            reset();
        }

        void reset() noexcept
        {
            phaseAcc27 = 0;
            randomLevel = 0.0f;
            previousRandomLevel = 0.0f;
            smoothedRandomStarted = false;
            randomState = randomSeed;
        }

        void resetPhase() noexcept { phaseAcc27 = 0; }
        int nativePhaseIndex() const noexcept { return static_cast<int>(phaseAcc27 >> 16); }

        void setPhase (float phase01) noexcept
        {
            const uint32_t kPhaseScale27 = (1u << 27);
            const uint32_t kPhaseMask27 = 0x07ffffff;
            if (! std::isfinite (phase01)) phase01 = 0.0f;
            phase01 -= std::floor (phase01);
            phaseAcc27 = (uint32_t)(std::floor(phase01 * (float)kPhaseScale27)) & kPhaseMask27;
        }

        /**
            Processes a single sample and returns the normalized LFO waveform value (-1..1).
        */
        float processSample (const LfoSettings& p, float hostBpm, const WaveTableBank* bank = nullptr) noexcept
        {
            if (!p.on) return 0.0f;
            if (p.wave == 5 && ! smoothedRandomStarted)
            {
                previousRandomLevel = randomLevel;
                makeRandomLevel();
                smoothedRandomStarted = true;
            }

            double speedHz = 0.0;
            if (p.tempoSync) {
                static const int g_nLFOTime[] = {
                    1536, 1152, 864, 768, 576, 432, 384, 288, 216, 192, 144, 96, 84, 72, 48, 42, 36, 32, 24, 21, 18, 16, 12, 9, 8, 6, 4, 3, 2, 1
                };
                const int raw = std::clamp (static_cast<int> (std::lround (p.speed * 127.0f)), 0, 127);
                const int idx = raw * 30 / 128;
                int ticksPerCycle = g_nLFOTime[idx];
                // The native table uses 24 ticks per beat, not 96.
                speedHz = (hostBpm * 24.0) / (ticksPerCycle * 60.0);
            } else {
                float speed01 = std::clamp(p.speed, 0.0f, 1.0f);
                // Original-DLL tremolo sweeps confirm the 1/12..125 Hz curve.
                speedHz = std::pow (1500.0, static_cast<double> (speed01)) / 12.0;
                // Native 44.1/48 kHz captures follow an integer sample period.
                speedHz = sampleRate / std::max (1.0, std::floor (sampleRate / speedHz));
            }

            const uint32_t kPhaseScale27 = (1u << 27);
            const uint32_t kPhaseMask27 = 0x07ffffff;

            {
                double phaseIncDouble = (speedHz / sampleRate) * (double)kPhaseScale27;
                uint32_t phaseInc = (uint32_t)std::floor(phaseIncDouble) & kPhaseMask27;

                uint32_t oldPhase = phaseAcc27;
                phaseAcc27 = (phaseAcc27 + phaseInc) & kPhaseMask27;

                if (phaseAcc27 < oldPhase && (p.wave == 4 || p.wave == 5))
                {
                    previousRandomLevel = randomLevel;
                    makeRandomLevel();
                }
            }

            const double phase = static_cast<double> (phaseAcc27) / kPhaseScale27;
            float val = 0.0f;
            switch (p.wave)
            {
                case 0: val = static_cast<float> (std::sin (phase * ::synth2::audio::MathConstants<double>::twoPi)); break;
                case 1: val = static_cast<float> (phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase); break;
                case 2: val = static_cast<float> (1.0 - 2.0 * phase); break;
                case 3: val = phase < 0.5 ? 1.0f : -1.0f; break;
                case 4: val = randomLevel; break;
                case 5: val = previousRandomLevel + static_cast<float> (phase) * (randomLevel - previousRandomLevel); break;
            }
            if (bank != nullptr && p.wave >= 0 && p.wave < 4)
            {
                // LFO: sine/tri/saw/square; audio tables: saw/square/tri/sine.
                static constexpr int audioShape[] {3, 2, 0, 1};
                const auto table = bank->getTableFor (audioShape[p.wave], static_cast<float> (speedHz));
                if (table.first != nullptr && table.second > 0)
                {
                    const double position = phase * static_cast<double> (table.second);
                    const auto index = static_cast<size_t> (position);
                    const float fraction = static_cast<float> (position - static_cast<double> (index));
                    val = table.first[index] + fraction * (table.first[(index + 1) % table.second] - table.first[index]);
                }
            }

            return val * p.amount;
        }

    private:
        void makeRandomLevel() noexcept {
            // Keep S&H independent of other plugin instances and voice order.
            randomState ^= randomState << 13;
            randomState ^= randomState >> 17;
            randomState ^= randomState << 5;
            randomLevel = (static_cast<float> (randomState) / 4294967295.0f) * 2.0f - 1.0f;
        }

        double sampleRate { 44100.0 };
        uint32_t phaseAcc27 { 0 };
        uint32_t randomSeed;
        uint32_t randomState;
        float randomLevel { 0.0f };
        float previousRandomLevel { 0.0f };
        bool smoothedRandomStarted { false };
    };
}
