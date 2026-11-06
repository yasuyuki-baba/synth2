#pragma once

namespace synth2::render
{
    struct RandomArpControls
    {
        bool enabled {};
        int type {},gate {},playMode {},firstWave {},secondWave {};
    };

    // Admit observed file-loaded random-arp CRT schedules.
    // Running-history and host-format guards are enforced by the processor.
    inline bool admitRandomArp(const RandomArpControls& c) noexcept
    {
        if(!c.enabled || c.type!=4 || c.gate<=0 || c.gate>127 || (c.playMode<0 || c.playMode>2))return false;
        // Deterministic and random LFO schedules cover poly/mono/legato,
        // partial/full gate, key sync and tempo sync. Waveforms do not select
        // a fallback ARP clock; random receivers still share the same CRT.
        return true;
    }
}
