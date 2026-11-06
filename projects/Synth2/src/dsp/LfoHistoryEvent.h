#pragma once
#include <array>

namespace synth2::dsp
{
    struct LfoHistoryEvent
    {
        // kind0: rate; kind3: sync; kind4: random edit (parameter,value); kind1/2: load.
        int kind{},a{},b{};
        std::array<int,4> routes{};
        std::array<int,2> sync{};
        // Native destination/wave/depth, captured with each load marker.
        std::array<std::array<int,3>,2> randomControls{};
        bool randomAdmitted=false;
        bool randomArpeggio=false;
    };

}
