#pragma once
#include <array>
#include "../midi/ControllerRuntime.h"

namespace synth2::render
{
    // Load-time observations consumed by the engine, separate from render controls.
    struct RenderHistory
    {
        bool nativeMidiRoutingLoadHistory {false};
        std::array<int,4> nativeMidiRoutingControls {};
        bool nativeRandomLfoHistoryValid {false};
        std::array<midi::ControllerRoute,2> controllerRoutes {};
        std::array<bool,2> controllerResetOnLoad {{true,true}};
    };
}
