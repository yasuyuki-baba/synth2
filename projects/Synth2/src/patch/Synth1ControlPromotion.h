#pragma once
#include <Synth1NativeLoading.h>
#include <array>

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
