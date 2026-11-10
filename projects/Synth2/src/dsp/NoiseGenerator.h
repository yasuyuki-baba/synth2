#pragma once
#include <cstdint>

namespace synth2::dsp
{
    struct NoiseGenerator
    {
        uint32_t state=10;
        void reset() noexcept { state=10; }
        float sample() noexcept
        {
            const bool bit=(state&0x80000u)!=0;
            state=(state<<1)^(bit ? 0x87u : 0u);
            return bit ? 1.0f : -1.0f;
        }
        // Sync/noise kernels reset the LFSR after the masked master wraps.
        // OSC2 has no phase advance or fractional slave-phase reset here.
        void synchronize(uint32_t master,uint32_t nextMaster) noexcept
        {
            if((nextMaster&0x07ffffffu)<(master&0x07ffffffu))reset();
        }
    };
}
