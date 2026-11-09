#pragma once
#include "ModulationSettings.h"
#include <array>

namespace synth2::dsp
{
    struct SharedModulationSettings
    {
        std::array<LfoSettings,2> lfos;
        double hostBpm {120};
        bool pitchReceiver {},filterReceiver {},pulseWidthReceiver {};
        bool midiRoutingLoadHistory {};
        std::array<int,4> midiRoutingControls {};
    };
}
