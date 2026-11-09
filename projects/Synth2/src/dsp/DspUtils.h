#pragma once

#include "audio/AudioTypes.h"
#include "FrequencyModulation.h"
#include "FractionalDial.h"

namespace synth2
{
    // Static FM shares the native compatible knots; fractional edits interpolate
    // after loading has resolved any legacy integer source values.
    static double float01ToFmTarget(double v01) noexcept
    {
        return dsp::FrequencyModulation::depth(
            static_cast<double>(dsp::fractionalDial(v01)),0.0);
    }

}
