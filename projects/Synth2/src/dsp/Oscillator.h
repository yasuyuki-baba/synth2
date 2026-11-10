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
    // Audio samples and interpolation remain double throughout the graph.
    inline uint32_t compatiblePhaseIncrement(double frequency,double sampleRate) noexcept
    {
        const double increment=static_cast<double>(static_cast<float>(frequency))*(kPhaseScale27/sampleRate);
        return static_cast<uint32_t>(static_cast<float>(increment))&kPhaseMask27;
    }

    struct OscillatorSample
    {
        double value{};
        float referenceWord{};
    };

    class Oscillator {
    public:
        Oscillator() { reset(); }
        void setSampleRate(double sr) {
            if(sampleRate!=sr)incrementReady=false;
            sampleRate=sr;
        }

        uint32_t getPhaseFixed27() const noexcept { return phaseAcc32; }
        void setPhaseFixed27(uint32_t p) noexcept { phaseAcc32 = p & kPhaseMask27; }
        uint32_t getPhaseIncFixed27() const noexcept { return phaseIncFixed27; }
        void synchronizeNoise(uint32_t master,uint32_t nextMaster) noexcept { noise.synchronize(master,nextMaster); }

        double renderSample(int waveIndex,double pulseWidth,uint32_t phaseOffset27=0,
            const WaveTableBank* bank=nullptr,double nativePulseWidthBins=-1) noexcept
        {return readSample(waveIndex,pulseWidth,phaseOffset27,bank,nativePulseWidthBins).value;}

        OscillatorSample readSample(int waveIndex,double pulseWidth,uint32_t phaseOffset27=0,
            const WaveTableBank* bank=nullptr,double nativePulseWidthBins=-1) noexcept
        {
            if(waveIndex==4) {
                const double value=noise.sample();return {value,static_cast<float>(value)};
            }
            if(bank==nullptr)return {};
            const uint32_t phase=(phaseAcc32+phaseOffset27)&kPhaseMask27;
            if(waveIndex==1 && bank->hasShape(5))
            {
                const double bins=std::clamp(nativePulseWidthBins>=0 ? nativePulseWidthBins
                    : ModulationEnvelope::pulseWidthForDial(fractionalDial(pulseWidth),0.0),10.0,2037.0);
                const auto table=bank->getNativeTableFor(5,static_cast<float>(frequencyHz));
                if(table.first==nullptr)return {};
                const auto origin=readTable(table.first,phase);
                const auto shifted=[&](uint32_t offset) {
                    const auto next=readTable(table.first,(phase+(offset<<16))&kPhaseMask27);
                    return OscillatorSample{.5*(origin.value-next.value),.5f*(origin.referenceWord-next.referenceWord)};
                };
                const auto low=static_cast<uint32_t>(bins);
                return interpolate(shifted(low),shifted(low+1),bins-low);
            }
            const double frequency=phaseIncFixed27*sampleRate/kPhaseScale27;
            const auto table=waveIndex==0 || waveIndex==2
                ? bank->getNativeTableFor(waveIndex,static_cast<float>(frequencyHz)) : bank->getTableFor(waveIndex,frequency);
            return table.first!=nullptr ? readTable(table.first,phase) : OscillatorSample{};
        }

        double renderPhaseDiffSawNoAdvance(uint32_t offsetFixed27, const WaveTableBank* bank,
                                         uint32_t phaseOffset27 = 0, int sawShape = 0) noexcept {
            if (bank == nullptr) return 0.0;
            double freqHz = (double)((double)phaseIncFixed27 * sampleRate / (double)kPhaseScale27);
            auto saw = (sawShape == 0 || sawShape == 5) ? bank->getNativeTableFor (sawShape,static_cast<float>(frequencyHz)) : bank->getTableFor(sawShape, freqHz);
            if (saw.first == nullptr) return 0.0;

            const double* sd = saw.first;
            uint32_t p1 = (phaseAcc32 + phaseOffset27) & kPhaseMask27;
            uint32_t p2 = (p1 + offsetFixed27) & kPhaseMask27;

            auto read = [&](uint32_t p) {
                uint32_t i0 = (p >> 16) & 0x7ffu;
                double frac = (double)(p & 0xffffu) / 65536.0;
                return sd[i0] + frac * (sd[(i0 + 1) % 2048] - sd[i0]);
            };
            return 0.5 * (read(p1) - read(p2));
        }

        void advancePhase() noexcept {
            uint32_t next = phaseAcc32 + phaseIncFixed27;
            phaseAcc32 = next & kPhaseMask27;
        }

        void advancePhaseFixedWithOffset(uint32_t offset) noexcept {
            uint32_t next = (phaseAcc32 + phaseIncFixed27 + offset);
            phaseAcc32 = next & kPhaseMask27;
        }

        void updatePhaseIncrement(double freqHz) noexcept {
            if(incrementReady && frequencyHz==freqHz)return;
            frequencyHz = freqHz;
            phaseIncFixed27=compatiblePhaseIncrement(freqHz,sampleRate);
            if (phaseIncFixed27 == 0) phaseIncFixed27 = 1;
            incrementReady=true;
        }

        void reset() noexcept { phaseAcc32 = 0; noise.reset(); }
        void resetPhase(double phase01, int waveIndex) noexcept {
            const double v = (double)phase01 * (double)kPhaseScale27;
            phaseAcc32 = (uint32_t)std::floor(v) & kPhaseMask27;
        }

    private:
        static OscillatorSample interpolate(OscillatorSample first,OscillatorSample second,double fraction) noexcept
        {
            // One table read feeds double audio and its compatible phase-word
            // projection. The word never replaces an audio sample or buffer.
            return {first.value+(second.value-first.value)*fraction,
                first.referenceWord+(second.referenceWord-first.referenceWord)*static_cast<float>(fraction)};
        }
        static OscillatorSample readTable(const double* data,uint32_t phase) noexcept
        {
            const uint32_t index=(phase>>16)&2047u;
            const double first=data[index],second=data[(index+1)&2047u];
            return interpolate({first,static_cast<float>(first)},{second,static_cast<float>(second)},
                static_cast<double>(phase&65535u)*(1.0/65536.0));
        }
        uint32_t phaseAcc32{0}, phaseIncFixed27{1};
        NoiseGenerator noise;
        double sampleRate{44100.0};
        double frequencyHz{440.0};
        bool incrementReady{};
    };

}
