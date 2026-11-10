#pragma once

#include "audio/AudioTypes.h"
#include "FrequencyModulation.h"
#include "FractionalDial.h"

namespace synth2
{
    // Static FM shares the native compatible knots; fractional edits interpolate
    // after loading has resolved any legacy integer source values.
    static float float01ToFmTarget(float v01) noexcept
    {
        return dsp::FrequencyModulation::depth(
            static_cast<float>(dsp::fractionalDial(v01)),0.0f);
    }

}
