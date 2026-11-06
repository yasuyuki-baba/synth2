#pragma once
#include <algorithm>
#include <cstdint>

namespace synth2::dsp
{
    struct SubOscillatorStack
    {
        // Win64 0x2943b, followed by 0x3e540. Preserve the serialized knob.
        static float level(int dial) noexcept
        {
            return static_cast<float>(std::clamp(dial,0,127)*4)*(1.0f/127.0f);
        }
        static float denominator(float level,float mix) noexcept
        {
            return (level+1.0f)*(1.0f-mix)+mix;
        }
        static float normalizer(float level,float mix) noexcept
        {
            return 1.0f/denominator(level,mix);
        }
        // Kernels share OSC1's full accumulator. Bit 27 is significant below.
        static uint32_t tableIndex(uint32_t phase,bool down) noexcept
        {
            return (phase>>(down ? 17 : 16))&2047u;
        }
    };
}
