#pragma once
#include <algorithm>
#include <array>

namespace synth2
{
    constexpr int nativeDetuneDial (int version, int mode, int source) noexcept
    {
        source = std::clamp (source,0,127);
        if (version == 105 || version == 106) return mode == 0 ? 48 : 70;
        if (version != 107 && version != 108) return source;
        constexpr std::array<int,128> poly {
            1,5,7,10,13,15,18,20,22,25,27,29,31,33,35,36,
            38,40,41,43,45,46,48,49,51,52,53,55,56,57,59,60,
            61,62,64,65,66,67,68,69,70,71,72,73,74,75,76,77,
            78,79,80,81,82,83,83,84,85,86,87,88,88,89,90,91,
            91,92,93,94,94,95,96,96,97,98,99,99,100,100,101,102,
            102,103,104,104,105,105,106,107,107,108,108,109,110,110,111,111,
            112,112,113,113,114,114,115,115,116,117,117,118,118,119,119,119,
            120,120,121,121,122,122,123,123,124,124,125,125,125,126,126,127
        };
        constexpr std::array<int,128> mono {
            1,7,12,16,21,25,28,32,35,38,41,44,47,50,52,55,
            57,59,62,64,66,68,70,72,73,75,77,79,80,82,83,85,
            86,88,89,91,92,93,95,96,97,98,100,101,102,103,104,105,
            106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,
            121,122,123,124,125,126,126,127,127,127,127,127,127,127,127,127,
            127,127,127,127,127,127,127,127,127,127,127,127,127,127,127,127,
            127,127,127,127,127,127,127,127,127,127,127,127,127,127,127,127,
            127,127,127,127,127,127,127,127,127,127,127,127,127,127,127,127
        };
        return (mode == 0 ? poly : mono)[static_cast<std::size_t> (source)];
    }

    struct NativePresetVoiceFields
    {
        int detune, voices, polyphony, delaySpread, delayTone;
    };

    constexpr NativePresetVoiceFields nativePresetVoiceFields (int version, int mode,
        int detune, int voices, int polyphony, int spread, int tone) noexcept
    {
        NativePresetVoiceFields result {nativeDetuneDial (version,mode,detune),
            std::clamp (voices,1,8), polyphony > 0 ? std::clamp (polyphony,1,32) : 32,
            std::clamp (spread,0,127), std::clamp (tone,0,127)};
        if (version >= 105 && version <= 108)
        {
            result.voices = mode == 0 ? 2 : 4;
            result.polyphony = 16;
        }
        if (version == 105 || version == 106) result.delaySpread = 66;
        if ((version >= 105 && version <= 109) || version == 111) result.delayTone = 64;
        return result;
    }

}
