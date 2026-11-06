#pragma once
#include "FractionalDial.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace synth2::dsp
{
    // Isolated non-tempo CLFO clock, Win64 RVA0x14f60/0x14ff0/0x15210.
    // Integration must preserve native setter/advance call boundaries.
    class LfoClock
    {
    public:
        void setRate(int dial) noexcept
        {
            rate = std::clamp(dial,0,127);
            rateDial=static_cast<float>(rate);
            const float unit = static_cast<float>(rate) * (1.0f/127.0f);
            targetHz = static_cast<float>(std::pow(1500.0,static_cast<double>(unit)) * (1.0/12.0));
            slew(1);
            step = targetHz > 10.0f ? 32 : 64;
        }

        void setRate(float dial) noexcept
        {
            dial=std::clamp(dial,0.0f,127.0f);
            if(dial==std::trunc(dial)){setRate(static_cast<int>(dial));return;}
            rateDial=dial;
            rate=static_cast<int>(std::lround(dial));
            targetHz=interpolateDial(dial,[](int value) {
                const float unit=static_cast<float>(value)*(1.0f/127.0f);
                return static_cast<float>(std::pow(1500.0,static_cast<double>(unit))*(1.0/12.0));
            });
            slew(1);
            step=targetHz>10.0f ? 32 : 64;
        }

        // Native CLFO RVA0x15255. Kept separate from the integrated free clock:
        // a temporary sync setter also changes history after sync is disabled.
        void setSynchronizedRate(float dial, float bpm) noexcept
        {
            constexpr int divisions[] {1536,1152,864,768,576,432,384,288,216,192,
                144,96,84,72,48,42,36,32,24,21,18,16,12,9,8,6,4,3,2,1};
            rateDial=std::clamp(dial,0.0f,127.0f);
            // Tempo divisions are discrete choices, separate from free-rate Hz.
            rate=static_cast<int>(std::lround(rateDial));
            const int division=divisions[(rate*30)>>7];
            const float pulses=bpm*48.0f;
            const float divided=pulses/static_cast<float>(division);
            targetHz=divided*(1.0f/120.0f);
            slew(1);
            step=targetHz > 10.0f ? 32 : 64;
        }

        void setSampleRate(int value) noexcept
        {
            sampleRate = std::max(1,value);
            slew(1);
            step = targetHz > 10.0f ? 32 : 64;
            // The sample-rate setter recalculates even when no slew is pending.
            rescale(static_cast<int>(static_cast<float>(sampleRate)/currentHz));
        }

        void resetPhase() noexcept { counter = 0; }

        void advance(int samples) noexcept
        {
            counter += samples;
            if(counter >= period) counter %= period;
            slew(samples);
        }

        int phaseIndex() const noexcept
        {
            return static_cast<int>((static_cast<uint32_t>(counter) << 11) / static_cast<uint32_t>(period));
        }

        int sampleRate = 44100;
        int step = 0;
        float currentHz = 1.0f;
        float targetHz = 1.0f;
        int counter = 0;
        int period = 44100;
        int rate = 0;
        float rateDial = 0;

    private:
        void rescale(int nextPeriod) noexcept
        {
            const float scaled = static_cast<float>(nextPeriod) * static_cast<float>(counter);
            counter = static_cast<int>(scaled / static_cast<float>(period));
            period = nextPeriod;
        }

        void slew(int samples) noexcept
        {
            if(currentHz == targetHz) return;
            const float denominator = static_cast<float>(sampleRate) * 0.001f;
            const float delta = static_cast<float>(samples) / denominator;
            if(currentHz < targetHz) currentHz = std::min(currentHz + delta,targetHz);
            else currentHz = std::max(currentHz - delta,targetHz);
            rescale(static_cast<int>(static_cast<float>(sampleRate) / currentHz));
        }
    };
}
