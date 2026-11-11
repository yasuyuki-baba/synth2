#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include "DspUtils.h"
#include "NoiseGenerator.h"
#include "ModulationEnvelope.h"
#include "PulseWidthRead.h"
#include "WaveTableBank.h"

namespace synth2::dsp {

    static constexpr uint32_t kPhaseBits27 = 27;
    static constexpr uint32_t kPhaseMask27 = 0x07ffffff;
    static constexpr uint32_t kPhaseScale27 = (1u << kPhaseBits27);

    // Native frequency and phase words are discrete compatibility boundaries.
    // Audio samples and interpolation use Float32 throughout the graph.
    inline uint32_t compatiblePhaseIncrement(float frequency,float sampleRate) noexcept
    {
        const float increment=(frequency/sampleRate)*static_cast<float>(kPhaseScale27);
        return static_cast<uint32_t>(static_cast<float>(increment))&kPhaseMask27;
    }

    class Oscillator {
    public:
        Oscillator() { reset(); }
        void setSampleRate(float sr) {
            if(sampleRate!=sr)incrementReady=false;
            sampleRate=sr;
        }

        uint32_t getPhaseFixed27() const noexcept { return phaseAcc32; }
        void setPhaseFixed27(uint32_t p) noexcept { phaseAcc32 = p & kPhaseMask27; }
        uint32_t getPhaseIncFixed27() const noexcept { return phaseIncFixed27; }
        void synchronizeNoise(uint32_t master,uint32_t nextMaster) noexcept { noise.synchronize(master,nextMaster); }

        float renderSample(int waveIndex,float pulseWidth,uint32_t phaseOffset27=0,
            const WaveTableBank* bank=nullptr,float nativePulseWidthBins=-1) noexcept
        {return readSample(waveIndex,pulseWidth,phaseOffset27,bank,nativePulseWidthBins);}

        float readSample(int waveIndex,float pulseWidth,uint32_t phaseOffset27=0,
            const WaveTableBank* bank=nullptr,float nativePulseWidthBins=-1) noexcept
        {
            if(waveIndex==4) {
                return noise.sample();
            }
            if(bank==nullptr)return {};
            const uint32_t phase=(phaseAcc32+phaseOffset27)&kPhaseMask27;
            if(waveIndex==1 && bank->hasShape(5))
            {
                const float bins=std::clamp(nativePulseWidthBins>=0 ? nativePulseWidthBins
                    : ModulationEnvelope::pulseWidthForDial(fractionalDial(pulseWidth),0.0f),10.0f,2037.0f);
                const auto table=bank->getNativeTableFor(5,static_cast<float>(frequencyHz));
                if(table.first==nullptr)return {};
                const auto origin=readTable(table.first,phase);
                const auto shifted=[&](uint32_t offset) {
                    const auto next=readTable(table.first,(phase+(offset<<16))&kPhaseMask27);
                    return .5f*(origin-next);
                };
                const auto low=static_cast<uint32_t>(bins);
                return interpolate(shifted(low),shifted(low+1),bins-low);
            }
            const float frequency=phaseIncFixed27*sampleRate/kPhaseScale27;
            const auto table=waveIndex==0 || waveIndex==2
                ? bank->getNativeTableFor(waveIndex,static_cast<float>(frequencyHz)) : bank->getTableFor(waveIndex,frequency);
            return table.first!=nullptr ? readTable(table.first,phase) : 0.0f;
        }

        float renderPhaseDiffSawNoAdvance(uint32_t offsetFixed27, const WaveTableBank* bank,
                                         uint32_t phaseOffset27 = 0, int sawShape = 0) noexcept {
            if (bank == nullptr) return 0.0f;
            float freqHz = (float)((float)phaseIncFixed27 * sampleRate / (float)kPhaseScale27);
            auto saw = (sawShape == 0 || sawShape == 5) ? bank->getNativeTableFor (sawShape,static_cast<float>(frequencyHz)) : bank->getTableFor(sawShape, freqHz);
            if (saw.first == nullptr) return 0.0f;

            const float* sd = saw.first;
            uint32_t p1 = (phaseAcc32 + phaseOffset27) & kPhaseMask27;
            uint32_t p2 = (p1 + offsetFixed27) & kPhaseMask27;

            auto read = [&](uint32_t p) {
                uint32_t i0 = (p >> 16) & 0x7ffu;
                float frac = (float)(p & 0xffffu) / 65536.0f;
                return sd[i0] + frac * (sd[(i0 + 1) % 2048] - sd[i0]);
            };
            return 0.5f * (read(p1) - read(p2));
        }

        void advancePhase() noexcept {
            uint32_t next = phaseAcc32 + phaseIncFixed27;
            phaseAcc32 = next & kPhaseMask27;
        }

        void advancePhaseFixedWithOffset(uint32_t offset) noexcept {
            uint32_t next = (phaseAcc32 + phaseIncFixed27 + offset);
            phaseAcc32 = next & kPhaseMask27;
        }

        void updatePhaseIncrement(float freqHz) noexcept {
            if(incrementReady && frequencyHz==freqHz)return;
            frequencyHz = freqHz;
            phaseIncFixed27=compatiblePhaseIncrement(freqHz,sampleRate);
            if (phaseIncFixed27 == 0) phaseIncFixed27 = 1;
            incrementReady=true;
        }

        void reset() noexcept { phaseAcc32 = 0; noise.reset(); }
        void resetPhase(float phase01, int waveIndex) noexcept {
            const float v = (float)phase01 * (float)kPhaseScale27;
            phaseAcc32 = (uint32_t)std::floor(v) & kPhaseMask27;
        }

    private:
        static float interpolate(float first,float second,float fraction) noexcept
        {return first+(second-first)*fraction;}
        static float readTable(const float* data,uint32_t phase) noexcept
        {
            const uint32_t index=(phase>>16)&2047u;
            return interpolate(data[index],data[(index+1)&2047u],
                static_cast<float>(phase&65535u)*(1.0f/65536.0f));
        }
        uint32_t phaseAcc32{0}, phaseIncFixed27{1};
        NoiseGenerator noise;
        float sampleRate{44100.0f};
        float frequencyHz{440.0f};
        bool incrementReady{};
    };

}
