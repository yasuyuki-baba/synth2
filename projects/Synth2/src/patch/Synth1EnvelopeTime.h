#pragma once
#include <algorithm>
#include <cmath>

namespace synth2
{
    // Fitted to original v1.13 beta3 amplitude-envelope Attack slopes via Wine.
    // Amp raw 64/80/96/112/127 agree within 0.02%; the shortest attacks are obscured
    // by the original output smoothing and need additional deconvolution.
    // Independent Filter cutoff inversion supports the same timing family.
    inline float envelopeAttackSeconds (int raw) noexcept
    {
        return 0.0005f * std::pow (56000.0f, std::clamp (raw, 0, 127) / 127.0f);
    }
    // Nominal endpoint, not the e-folding time. Native decay/release peaks follow
    // (1 - t/T) * exp(-8*t/T); output smoothing obscures the shortest settings.
    inline float envelopeDecayReleaseSeconds (int raw) noexcept
    {
        return 0.01f * std::pow (6000.0f, std::clamp (raw, 0, 127) / 127.0f);
    }
    // Same measured gain law as Amp Gain, with 1/2048 downward quantization.
    // Raw zero is silent. The complete 128-step native sweep is archived.
    inline float ampSustainLevel (int raw) noexcept
    {
        raw = std::clamp (raw, 0, 127);
        if (raw == 0) return 0.0f;
        const double gain = std::pow (2048.0, std::pow (raw / 127.0, 0.3) - 1.0);
        return static_cast<float> (std::floor (gain * 2048.0) / 2048.0);
    }
}
