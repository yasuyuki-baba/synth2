#pragma once
#include "audio/AudioTypes.h"
#include <array>

namespace synth2::dsp
{
    // Engine-owned, prepared storage. Voices only read it during rendering.
    struct LfoBlock
    {
        ::synth2::audio::AudioBuffer<float> samples;
        int startSample = 0;
        std::array<std::array<double,256>,2> amplitudeControl {};
        std::array<std::array<double,256>,2> projectedPan {};
        std::array<std::array<int,256>,2> controlRemaining {};
    };

}
