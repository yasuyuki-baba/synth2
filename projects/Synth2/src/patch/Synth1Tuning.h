#pragma once
#include <algorithm>

namespace synth2
{
    // Semantic/internal raw values after synth1probe's family normalization.
    // Synth1.pdf p23 lists octave indicators and duplicate adjacent raw steps.
    inline int coarseTuneSemitones (int raw) noexcept
    {
        return std::clamp (raw, 0, 127) * 120 / 127 - 60;
    }

    inline float fineTuneCents (float raw) noexcept
    {
        const float value = std::clamp (raw, 0.0f, 127.0f);
        // Native p3/p72 sweeps: the outer slopes join the 62..66 zero band.
        // Subtracting 64 outside the band introduces a two-cent discontinuity.
        return static_cast<float> (value < 62 ? value - 62 : value > 66 ? value - 66 : 0);
    }
}
