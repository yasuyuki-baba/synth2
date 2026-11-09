#pragma once

#include <array>

// Value-only controls shared by the loading bridge and modulation components.
// No file format, host automation, voice allocation or global engine dependency.
namespace synth2
{
    struct LfoSettings
    {
        bool on {false};
        int wave {0}; // Sine, triangle, saw, square, sample-and-hold, smoothed random.
        int dest {0}; // OSC2, both oscillators, filter, amplitude, PW, FM, pan.
        double speed {0.5},amount {0.0};
        bool tempoSync {false},keySync {false};
    };

    struct ModulationEnvelopeControls
    {
        double amountDial {64};
        double attackDial {0},decayDial {0};
        bool  on         { false };
        int   dest       { 0 };     // 0=osc2, 2=fm, 3=pw
    };

}
