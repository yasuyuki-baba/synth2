#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace synth2::dsp
{
    // Win64 RVA 0x36460: OSC pitch spans the endpoints, whereas filter pitch
    // divides by the voice count and therefore has a negative mean offset.
    struct Unison
    {
        std::array<float, 8> pitchCents {};
        std::array<double, 8> filterIndex {};

        // RVA0x36870, with source84 normalized as (dial-64)/64.
        // Registered slots repeat the group layout; one active core is centred.
        static float panOffset (int count, int slot, float amount) noexcept
        {
            count = std::clamp (count, 1, 8);
            if (count == 1) return 0.0f;
            return (static_cast<float>(std::max(slot,0)%count)/static_cast<float>(count-1))
                   * (amount*2.0f) - amount;
        }

        static Unison layout(int count, int dial, int pitchDial = 24) noexcept
        {
            Unison result;
            count = std::clamp(count, 1, 8);
            dial = std::clamp(dial, 0, 127);
            if (count == 1) return result;
            const float span = ((std::exp((static_cast<float>(dial)*(1.0f/127.0f))*2.0f)-1.0f)
                                / (std::exp(2.0f)-1.0f))*100.0f;
            // The eight-voice SIMD path uses rcpps plus one Newton step.
            // These are its observed float results for divisors seven/eight.
            const float pitchStep = count == 8 ? 0.142857134342193603515625f : 1.0f/static_cast<float>(count-1);
            const float filterStep = count == 8 ? 0.124999992549419403075f : 1.0f/static_cast<float>(count);
            // RVA 0x3b7d0 converts cents through the logarithmic cutoff table,
            // retaining the native float/double boundaries and truncation.
            const float inverseLogStep = 1.0f/(std::log(1066.6666259765625f)*(1.0f/1024.0f));
            for (int i = 0; i < count; ++i)
            {
                result.pitchCents[i] = (static_cast<float>(i)*pitchStep)*span-span*0.5f;
                // RVA0x371a0/0x36778: raw 24 is neutral; odd group slots only.
                if ((i & 1) != 0) result.pitchCents[i] += static_cast<float>((std::clamp(pitchDial,0,127)-24)*100);
                const float cents = (static_cast<float>(i)*filterStep-0.5f)*span;
                const double ratio = std::pow(2.0, static_cast<double>(cents*(1.0f/1200.0f)));
                result.filterIndex[i] = static_cast<int>(inverseLogStep*static_cast<float>(std::log(ratio)));
            }
            return result;
        }
        static Unison layout(int count,float dial,int pitchDial=24) noexcept
        {
            dial=std::clamp(dial,0.0f,127.0f);
            const int low=static_cast<int>(dial);
            auto result=layout(count,low,pitchDial);
            if(dial==low)return result;
            const auto next=layout(count,std::min(127,low+1),pitchDial);
            const float fraction=dial-low;
            for(std::size_t slot=0;slot<result.pitchCents.size();++slot)
            {
                result.pitchCents[slot]+=(next.pitchCents[slot]-result.pitchCents[slot])*fraction;
                result.filterIndex[slot]+=(next.filterIndex[slot]-result.filterIndex[slot])*fraction;
            }
            return result;
        }
    };
}
