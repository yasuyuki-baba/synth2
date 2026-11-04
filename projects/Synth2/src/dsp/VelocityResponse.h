#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace synth2::dsp
{
    // Native isolated-tone ratios agree with a 30 dB maximum velocity depth.
    inline float ampVelocityGain (float velocity, float sensitivity) noexcept
    {
        return std::pow (10.0f, -1.5f * std::clamp (sensitivity, 0.0f, 1.0f)
                                     * (1.0f - std::clamp (velocity, 0.0f, 1.0f)));
    }

    // Empirical interpolation of signed LP24 inversions. This is not a recovered
    // native formula; held-out velocities and amount/Sustain settings are checked
    // separately. MIDI velocity zero means note-off, with zero modulation here.
    inline float filterVelocityScale (float velocity) noexcept
    {
        struct Anchor { float midi, scale; };
        constexpr std::array<Anchor, 10> points {{{0, 0}, {1, .0076242005f},
            {16, .1728747350f}, {32, .3266418639f}, {48, .4605966627f},
            {64, .5827871623f}, {80, .6851373187f}, {96, .7710362108f},
            {112, .8469225749f}, {127, .9099806251f}}};
        const float midi = std::clamp (velocity, 0.0f, 1.0f) * 127.0f;
        for (size_t i = 1; i < points.size(); ++i)
            if (midi <= points[i].midi)
            {
                const auto& a = points[i-1]; const auto& b = points[i];
                return a.scale + (b.scale-a.scale) * (midi-a.midi) / (b.midi-a.midi);
            }
        return points.back().scale;
    }
}
