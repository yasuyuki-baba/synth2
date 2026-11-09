#pragma once

namespace synth2
{
    struct ArpeggiatorSettings
    {
        bool enabled { false };
        int type { 2 }; // 1=up/down, 2=up, 3=down, 4=random
        int range { 0 }; // 0..3 additional octaves
        int beat { 11 }; // Native 19-position duration table
        int gate { 64 }; // 0..127
    };
}
