#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include "CompatibleControlCurve.h"

namespace synth2::dsp
{
    class OscillatorPhaseRandom
    {
    public:
        float nextFraction() noexcept
        {
            state = state * 214013u + 2531011u;
            const auto random15 = (state >> 16) & 32767u;
            const float fraction = static_cast<float> (static_cast<double> (static_cast<float> (random15)) * (1.0 / 32767.0));
            return fraction;
        }
        uint32_t nextPhase() noexcept
        {
            return static_cast<uint32_t> (nextFraction() * 134217728.0f);
        }
    private:
        uint32_t state {1};
    };

    struct OscillatorPhaseState
    {
        uint32_t osc1 {},osc2 {};
        std::array<uint32_t,8> lanes {};
        bool pending {true};
        double dial {-1};

        void setDial (double value) noexcept
        {
            value = std::clamp (value,0.0,127.0);
            // Preserve the compatible selection word at the random/fixed boundary. Fractional
            // edits inside random mode do not publish a new pending draw.
            if (std::lround(static_cast<float>(value))==0 && std::lround(static_cast<float>(dial))!=0) pending = true;
            dial = value;
        }

        // DLL random branch consumes two scalar and eight lane draws once per
        // pending request. Fixed mode restarts the two scalars on every note.
        bool start (OscillatorPhaseRandom& random, int count = 1, int slot = 0, double spreadDial = 0) noexcept
        {
            if (std::lround(static_cast<float>(dial)) > 0)
            {
                const auto fixedPhase=[&](float random,float scale) {
                    return static_cast<uint32_t>(compatibleControlCurve(dial,[&](int source) {
                        const float amount=(std::max(static_cast<float>(source),1.0f)-1.0f)*(1.0f/126.0f);
                        return static_cast<uint32_t>((amount*random)*scale);
                    }));
                };
                osc1=0;osc2=fixedPhase(1.0f,67108864.0f);
                OscillatorPhaseRandom pattern;
                for(auto& phase:lanes)
                {
                    const float random=pattern.nextFraction();
                    phase=fixedPhase(random,134217728.0f)&0x07ffffffu;
                }
                // DLL reset0x3e6d1: fixed multi-slot groups add ten offsets
                // from the seed-1 table, starting at slot*10. This consumes no
                // process/global random state. Preserve unsigned addition here;
                // oscillator consumers wrap the sum to27 bits.
                if (count >= 2 && spreadDial > 0)
                {
                    OscillatorPhaseRandom offsets;
                    for (int draw = 0; draw < std::clamp(slot,0,7)*10; ++draw) offsets.nextFraction();
                    const double spread=std::clamp(spreadDial,0.0,127.0);
                    auto offset=[&] {
                        // Draw once; interpolation must not advance the local table.
                        const float random=offsets.nextFraction();
                        return static_cast<uint32_t>(compatibleControlCurve(spread,[&](int source) {
                            return static_cast<uint32_t>(((static_cast<float>(source)*(1.0f/127.0f))*random)*134217728.0f);
                        }))&0x07ffffffu;
                    };
                    osc1 += offset(); osc2 += offset();
                    for (auto& phase : lanes) phase += offset();
                }
                pending = false;
                return true;
            }
            if (! pending) return false;
            osc1 = random.nextPhase(); osc2 = random.nextPhase();
            for (auto& phase : lanes) phase = random.nextPhase();
            pending = false;
            return true;
        }
    };
}
