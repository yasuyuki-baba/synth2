#pragma once
#include <cstdint>
#include <utility>

namespace synth2::dsp
{
    struct OscillatorSync
    {
        // Native ordinary Sync advances the master before testing its masked
        // wrap. The slave reset uses the old master and a low-32-bit product.
        // The native DIV instruction requires a nonzero master increment.
        static std::pair<uint32_t,uint32_t> advance(uint32_t master,uint32_t slave,
                                                   uint32_t masterIncrement,uint32_t slaveIncrement,
                                                   uint32_t fmOffset=0) noexcept
        {
            constexpr uint32_t mask=0x07ffffffu;
            // FM/Sync kernel0x42150 tests the modulated master wrap, but its
            // reset division retains the unmodulated master increment.
            const uint32_t next=master+masterIncrement+fmOffset;
            if((next&mask)<(master&mask))
                slave=static_cast<uint32_t>((master&mask)*slaveIncrement)/masterIncrement;
            else
                slave+=slaveIncrement;
            return {next,slave};
        }
    };
}
