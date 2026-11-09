#pragma once
#include <array>

namespace synth2::dsp
{
    // Engine-owned, prepared storage. Voices only read it during rendering.
    struct LfoBlock
    {
        int frameCapacity {256};
        int startSample = 0;
        std::array<std::array<double,256>,2> controls {};
        std::array<std::array<int,256>,2> controlRemaining {};
    };

}
