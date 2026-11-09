#pragma once
#include <algorithm>
#include <cstdint>

namespace synth2::dsp
{
    struct SubOscillatorStack
    {
        // Win64 0x2943b, followed by 0x3e540. Preserve the serialized knob.
        static double level(int dial) noexcept
        {
            return static_cast<double>(std::clamp(dial,0,127)*4)/127.0;
        }
        static double denominator(double level,double mix) noexcept
        {
            return (level+1.0)*(1.0-mix)+mix;
        }
        static double normalizer(double level,double mix) noexcept
        {
            return 1.0/denominator(level,mix);
        }
        // Kernels share OSC1's full accumulator. Bit 27 is significant below.
        static uint32_t tableIndex(uint32_t phase,bool down) noexcept
        {
            return (phase>>(down ? 17 : 16))&2047u;
        }
    };
}
