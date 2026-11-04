#pragma once
#include <algorithm>
#include <array>

namespace synth1::patch
{
    // Presence bits cover only the six envelope controls audited in the DLL.
    inline constexpr std::array<int, 6> nativeEnvelopeIndices {13,16,18,21,26,28};

    constexpr int nativeEnvelopePresenceBit (int index) noexcept
    {
        for (int bit = 0; bit < 6; ++bit)
            if (nativeEnvelopeIndices[bit] == index) return 1 << bit;
        return 0;
    }

    // Effective modern dials for omitted fields. Serialized p21=128 also occurs
    // in real banks; its extra step is retained outside host automation ranges.
    constexpr int nativeMissingEnvelopeDial (int version, int index) noexcept
    {
        const bool legacy = version == 105 || version == 106;
        if (index == 13) return 0;
        if (index == 21) return legacy ? 127 : 128;
        return legacy ? 62 : 64;
    }

    // Versions105..108 restore both phase controls from the native default
    // context at RVA0x27723, regardless of their explicit serialized dials.
    constexpr int nativePhaseImportDial (int version, int source) noexcept
    {
        return version >= 105 && version <= 108 ? 0 : std::clamp (source, 0, 127);
    }

    // Native source105/106 initialize Unison spread to its neutral default.
    constexpr int nativeUnisonSpreadImportDial (int version, int source) noexcept
    {
        return version == 105 || version == 106 ? 64 : std::clamp (source, 0, 127);
    }

    // Native .sy1 loader mapping, verified over all dials in three patch contexts.
    // The source value stays serialized; only the modern DSP control is adapted.
    constexpr int nativeChorusTimeDial (int sourceVersion, int sourceDial) noexcept
    {
        sourceDial = std::clamp (sourceDial, 0, 127);
        if (sourceVersion != 105 && sourceVersion != 106) return sourceDial;
        constexpr std::array<int, 128> legacy {
        9,10,10,11,11,12,13,13,14,14,15,16,16,17,18,18,
        19,20,21,21,22,23,24,24,25,26,27,28,28,29,30,31,
        32,33,33,34,35,36,37,38,39,40,41,42,43,44,45,46,
        47,48,49,50,51,52,53,54,55,56,57,58,60,61,62,63,
        64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,
        80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,
        96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,
        112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127
        };
        return legacy[static_cast<size_t> (sourceDial)];
    }

    // DLL source-version dispatcher 0x278a0 and legacy conversion 0x27591.
    // The decay map searches modern float(.01 * 6000^(dial/127)) for the
    // nearest old value 60^((source/127)^2.95)-1, retaining later ties.
    // Audited against actual on-disk loading, all dials and versions 104..113.
    inline constexpr std::array<int,128> nativeLegacyDecayDials {{
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,1,4,6,8,10,12,14,16,18,20,21,23,25,26,28,
        29,30,32,33,34,35,37,38,39,40,41,42,43,45,46,47,
        48,49,50,51,52,52,53,54,55,56,57,58,59,60,61,62,
        62,63,64,65,66,67,68,68,69,70,71,72,73,74,74,75,
        76,77,78,79,80,81,81,82,83,84,85,86,87,88,89,90,
        91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,
        108,109,110,111,112,114,115,116,117,119,120,121,123,124,125,127,
    }};

    inline int nativeEnvelopeImportDial(int version,int index,int source,int semanticFallback) noexcept
    {
        if(index!=13 && index!=16 && index!=18 && index!=21 && index!=26 && index!=28)
            return semanticFallback;
        if(version<104 || version>113)return semanticFallback;
        source=std::clamp(source,0,127);
        if(version!=105 && version!=106)return source;
        if(index==21)return 63+(source+1)/2;
        return nativeLegacyDecayDials[static_cast<std::size_t>(source)];
    }
}
