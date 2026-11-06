#pragma once
#include "Synth1PatchSemantics.h"
#include <array>
#include <bitset>

namespace synth1::patch
{
    // Effective native settings are distinct from serialized source values.
    // In particular, omitted p21 may be128 and packed MIDI words exceed127.
    struct NativePatchControls
    {
        int sourceVersion = 105;
        std::array<int,99> values {};
        std::bitset<126> explicitSource;
    };

    constexpr bool isNativeSourceVersion(int version) noexcept
    { return version == 100 || (version >= 104 && version <= 113); }

    int nativeDefaultSourceDial(int index, int version = 105);

    // Exact native default context -> source overlay -> version conversion.
    // Supports version100 (also named unversioned files) and104..113. Does not mutate source,
    // perform host notifications, initialize voices, or advance runtime history.
    NativePatchControls resolveNativePatchControls(const Synth1PatchDocument& source);

    // Low-level conversion for independent native-oracle verification. The
    // complete pre-conversion source context is required for dependent fields.
    int convertNativeLoadingDial(int version,int index,int value,const std::array<int,99>& source);
}
