#pragma once

#include "midi/ArpeggiatorSettings.h"
#include <cstdint>
#include <array>
#include "dsp/ModulationSettings.h"
#include "dsp/EffectSettings.h"

namespace synth2
{
    /**
        Filter Envelope components (p21-p24)
    */
    struct FilterEnvelopeControls
    {
        float attackSec  { 0.01f };
        float decaySec   { 0.1f };
        float sustain    { 0.8f }; // 0..1
        float releaseSec { 0.2f };
    };

    /**
        DSP render controls only. Source data, mapping cache and load history
        belong to the loading adapter and engine, not to voices.
    */
    struct RenderControls
    {
        ArpeggiatorSettings arpeggiator;
        // Oscillators
        int nativeMultiEffectType {-1};
        std::array<float,3> nativeMultiEffectDials {};
        float osc1StackDial {0}; // Effective p76; rounded zero retains the scalar route.
        float osc1WaveIndex   { 0.0f };
        float osc2WaveIndex   { 0.0f };
        float oscMix          { 0.0f };   // 0..1
        float unisonPhaseDial {0}; // Semantic p92: deterministic offsets in fixed-phase groups.
        float oscillatorPhaseDial {1}; // Semantic p91: rounded zero selects random; fixed amount retains fractions.
        float osc1FmAmount    { 0.0f };   // 0..1
        float osc2PitchSemis  { 0.0f };   // semitones
        float osc2FineCents   { 0.0f };   // cents
        float osc2Sync        { 0.0f };   // 0..1 (bool-ish)
        float ringMod         { 0.0f };   // 0..1 (bool-ish)
        float pulseWidth      { 0.5f };   // 0..1
        float osc2KbdTrack    { 1.0f };   // p4 (V105) or related in V112
        int   octaveBase      { 0 };      // p9: Octave (-2..+2)
        int   keyShift        { 0 };      // p13: Key Shift (semitones)

        // Sub Oscillator
        float subOscGain      { 0.0f }; // p95: 0..1
        int   subOscWave      { 0 };    // p96: 0..3
        int   subOscOctave    { -1 };   // p97: -2 or -1

        // Amplifier
        // Import mapper selects native block segments; direct callers keep sample-clocked behavior.
        // Complete native VCF + gain + output Tone route, including verified LP24.
        // Core-history admission only; Filter always uses the common kernels.
        float ampAttackSec      { 0.01f };
        float ampDecaySec       { 0.1f }; // Finite endpoint, not an exponential time constant.
        float ampSustain        { 0.8f };
        float ampReleaseSec     { 0.2f }; // Nominal endpoint; imported range .01..60 seconds.
        float ampGainMultiplier { 0.8f }; // p35
        float masterVolume       { 1.0f };
        uint64_t globalSampleCount { 0 };
 // p78

        // Filter
        float cutoffIndex     { 1023.0f };
        float filterCutoffAdjustment {}; // Fractional extension of the native base-index curve.
        float cutoffHz        { 0.0f };
        float resonanceNorm   { 0.0f };
        float filterEnvAmount { 0.0f };
        float filterKeyTrackNorm { 0.0f }; // Original dial value for native integer tracking.
        float filterVelocity  { 0.0f }; // p24: enables the measured filter-velocity response.
        float ampVelocity     { 0.0f }; // p30: 0..1 sets up to 30 dB velocity attenuation.
        int   filterType      { 0 };
        float filterSaturation{ 0.0f };
        float pan             { 0.0f }; // p90: -1..+1

        // Modulation
        LfoSettings lfo1, lfo2;
        FilterEnvelopeControls filterEnv;
        ModulationEnvelopeControls modEnv;

        // Voice management
        int   polyphony      { 32 };
        int   playMode       { 0 };
        int   unisonVoices   { 0 };
        int   unisonPitchDial { 24 }; // Source p85; odd group slots are transposed.
        float unisonDetune   { 0.0f };
        float unisonPan      { 0.0f }; // Signed native spread; source64 is neutral.
        bool  isPolyphonic   { true };
        bool  portamentoAuto { false };
        float sourcePortamentoDial {0}; // Resolved real dial for the physical portamento receiver.
        bool resetIdleFilter {false}; // Audited mono noise idle reuse.

        // Effects
        bool nativePhaser { false };
        int nativePhaserPairs {1};
        float nativePhaserDepth {0},nativePhaserRate {0},nativePhaserFeedback {64};
        DelaySettings delay;
        ChorusSettings chorus;
        EqualizerSettings eq;

        // Global
        float pitchBendRange  { 2.0f };   // p72 (semitones)
        float globalFineTune  { 0.0f };   // p72: OSC1/2 Fine Tune, cents
        float hostBpm         { 120.0f };
        bool hasLfo(int destination) const noexcept
        { return (lfo1.on && lfo1.dest==destination) || (lfo2.on && lfo2.dest==destination); }
        bool hasPitchLfo() const noexcept { return hasLfo(0) || hasLfo(1); }

    };
}
