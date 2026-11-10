#pragma once
#include <Synth1NativeLoading.h>
#include <array>
#include <cmath>

namespace synth2
{
    // The integer loading state is the compatibility boundary. Defaults,
    // omissions, packed words and source-version conversion are already resolved
    // by resolveNativePatchControls before this promotion is called.
    struct PromotedControlState
    {
        std::array<float,99> dials {};

        float normalized(int index) const noexcept
        { return dials[static_cast<std::size_t>(index)]/127.0f; }
    };

    // Recover the serialized float dial before entering the float render domain.
    // Extended source words keep their range; receivers own their own limits.
    inline float promoteNormalizedSource(float value) noexcept
    {
        const float raw=value*127.0f,nearest=std::round(raw);
        const float dial=value==nearest/127.0f || value==nearest*(1.0f/127.0f) ? nearest : raw;
        return static_cast<float>(dial)/127.0f;
    }

    inline PromotedControlState promoteCompatibleControls(
        const synth1::patch::NativePatchControls& compatible) noexcept
    {
        PromotedControlState result;
        // Admitted source words (including extended coarse and packed MIDI
        // words) fit exactly in float. The receiver applies its own limits;
        // do not clamp source words to host presentation ranges.
        for(std::size_t i=0;i<result.dials.size();++i)
            result.dials[i]=static_cast<float>(compatible.values[i]);
        return result;
    }
}
