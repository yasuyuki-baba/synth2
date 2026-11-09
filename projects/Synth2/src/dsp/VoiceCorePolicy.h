#pragma once

namespace synth2::dsp
{
    struct CoreStartPlan
    {
        bool resetFilter {},resetFilterEnvelope {},settleFm {};
    };
    struct CoreReusePolicy
    {
        // Physical activity selects the common reuse plan, independently of
        // source/history or logical note lifetime.
        static CoreStartPlan select(bool sounding) noexcept
        { return sounding ? CoreStartPlan{} : CoreStartPlan{true,true,true}; }
    };
}
