#pragma once

namespace synth2::dsp
{
    struct CoreReuseSettings
    {
        bool filterEnvelope {},idleFilterReset {},stateVariable {},filterLfo {},resonantNoise {};
        bool pulseWidthLfo {},modulationOn {},pitchEnvelope {},nativePerformance {};
        int filterType {},stackDial {};
    };
    struct CoreStartPlan
    {
        bool resetFilter {},resetFilterEnvelope {},settleFm {};
    };
    struct IdleCoreStrategy
    {
        static CoreStartPlan select(const CoreReuseSettings& settings) noexcept
        { return {true,settings.nativePerformance && settings.stateVariable,true}; }
    };
    struct SoundingCoreStrategy
    {
        static CoreStartPlan select(const CoreReuseSettings& settings) noexcept
        {
            const bool retain=settings.filterEnvelope || settings.idleFilterReset
                || (settings.stateVariable && (settings.filterType==1 || settings.stackDial>0
                    || settings.filterLfo || settings.resonantNoise))
                || (settings.pulseWidthLfo && settings.modulationOn && settings.pitchEnvelope
                    && (settings.stateVariable || settings.filterEnvelope));
            return {!retain,false,false};
        }
    };
    struct CoreReusePolicy
    {
        // Value-based Strategy selection; actions execute against caller-owned
        // core state in the original order after phase/glide initialization.
        static CoreStartPlan select(const CoreReuseSettings& settings,bool sounding) noexcept
        { return sounding ? SoundingCoreStrategy::select(settings) : IdleCoreStrategy::select(settings); }
    };
}
