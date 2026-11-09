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
        double time     { 0.0 };  // p56
        double feedback { 0.0 };  // p57
        double level    { 0.0 };  // p58
        int   type     { 0 };     // p82
        double spread   { 0.5 };  // p83
        double tone     { 0.5 };  // p98
    };

    /**
        Chorus components (p59-p65)
    */
    struct ChorusSettings
    {
        bool  enabled { false };
        int   type { 0 };
        double time { 0.5 };
        double depth { 0.5 };
        double rate { 0.5 };
        double feedback { 0.2 };
        double level { 0.5 };
    };

    /**
        EQ components (p66-p70)
    */
    struct EqualizerSettings
    {
        double level { 0.5 };
        double q { 0.5 };
        double tone { 0.5 };
        double freq { 0.5 };
    };

}
