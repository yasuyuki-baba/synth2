#pragma once
#include "RandomArpAdmission.h"

namespace synth2 { struct RenderControls; }

namespace synth2::render
{
    // The loading adapter supplies control meanings, never a file document,
    // source version, preset identity, APVTS object or indexed parameter array.
    struct RenderPolicyInputs
    {
        bool nativeSource {};
        bool randomHistoryValid {};
        RandomArpControls arpeggio;
        bool effectControlActive {},unisonControlActive {},stackControlActive {};
        bool effectEnabled {};
        int effectType {};
        int stackDial {},subLevelDial {};
        int modulationAmountDial {64},modulationAttackDial {},modulationDecayDial {};
    };

    // Select implementations and receiver composition after value mapping.
    // Running-history validity is an explicit input from the engine.
    void selectRenderPolicy(const RenderPolicyInputs&,RenderControls& target);
}
