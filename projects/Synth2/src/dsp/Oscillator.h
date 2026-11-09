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

        double renderSample(int waveIndex, double pulseWidth, uint32_t phaseOffset27 = 0, const WaveTableBank* bank = nullptr, double nativePulseWidthBins = -1) noexcept {
            if (waveIndex == 4) {
                return noise.sample();
            }
            const uint32_t p = (phaseAcc32 + phaseOffset27) & kPhaseMask27;

            if (bank != nullptr) {
                if (waveIndex == 1 && bank->hasShape (5)) {
                    const double bins=nativePulseWidthBins>=0 ? nativePulseWidthBins
                        : ModulationEnvelope::pulseWidthForDial(fractionalDial(pulseWidth),0.0);
                    const auto saw=bank->getNativeTableFor(5,static_cast<float>(frequencyHz));
                    if(saw.first==nullptr)return 0.0;
                    const auto read=[&](uint32_t phase) {
                        const auto index=(phase>>16)&2047u;
                        const double fraction=static_cast<double>(phase&65535u)*(1.0/65536.0);
                        return saw.first[index]+fraction*(saw.first[(index+1)&2047u]-saw.first[index]);
                    };
                    const double origin=read(p);
                    return PulseWidthRead::interpolate(bins,[&](uint32_t offset) {
                        return .5*(origin-read((p+(offset<<16))&kPhaseMask27));
                    });
                }
                double freqHz = (double)((double)phaseIncFixed27 * sampleRate / (double)kPhaseScale27);
                auto tbl = (waveIndex == 0 || waveIndex == 2) ? bank->getNativeTableFor (waveIndex,static_cast<float>(frequencyHz)) : bank->getTableFor(waveIndex, freqHz);
                if (tbl.first != nullptr) {
                    const double* data = tbl.first;
                    const size_t len = tbl.second;

                    uint32_t i0 = (uint32_t)((p >> 16) & 0x7ffu);
                    double frac = (double)(p & 0xffffu) / 65536.0;
                    uint32_t i1 = (i0 + 1) % 2048;
                    return data[i0] + frac * (data[i1] - data[i0]);
                }
            }
            return 0.0;
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
        uint32_t phaseAcc32{0}, phaseIncFixed27{1};
        NoiseGenerator noise;
        double sampleRate{44100.0};
        double frequencyHz{440.0};
        bool incrementReady{};
    };

}
