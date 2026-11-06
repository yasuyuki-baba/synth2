#pragma once

// Value-only effect settings shared by loading and output components.
namespace synth2
{
    /**
        Delay components (p55-p58, p82, p83, p98)
    */
    struct DelaySettings
    {
        bool  enable   { false }; // p55
        float time     { 0.0f };  // p56
        float feedback { 0.0f };  // p57
        float level    { 0.0f };  // p58
        int   type     { 0 };     // p82
        float spread   { 0.5f };  // p83
        float tone     { 0.5f };  // p98
    };

    /**
        Chorus components (p59-p65)
    */
    struct ChorusSettings
    {
        bool  enabled { false };
        int   type { 0 };
        float time { 0.5f };
        float depth { 0.5f };
        float rate { 0.5f };
        float feedback { 0.2f };
        float level { 0.5f };
    };

    /**
        EQ components (p66-p70)
    */
    struct EqualizerSettings
    {
        float level { 0.5f };
        float q { 0.5f };
        float tone { 0.5f };
        float freq { 0.5f };
    };

}
