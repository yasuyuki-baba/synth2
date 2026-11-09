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
        double attackSec  { 0.01 };
        double decaySec   { 0.1 };
        double sustain    { 0.8 }; // 0..1
        double releaseSec { 0.2 };
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
        std::array<double,3> nativeMultiEffectDials {};
        double osc1StackDial {0}; // Effective p76; rounded zero retains the scalar route.
        double osc1WaveIndex   { 0.0 };
        double osc2WaveIndex   { 0.0 };
        double oscMix          { 0.0 };   // 0..1
        double unisonPhaseDial {0}; // Semantic p92: deterministic offsets in fixed-phase groups.
        double oscillatorPhaseDial {1}; // Semantic p91: rounded zero selects random; fixed amount retains fractions.
        double osc1FmAmount    { 0.0 };   // 0..1
        double osc2PitchSemis  { 0.0 };   // semitones
        double osc2FineCents   { 0.0 };   // cents
        double osc2Sync        { 0.0 };   // 0..1 (bool-ish)
        double ringMod         { 0.0 };   // 0..1 (bool-ish)
        double pulseWidth      { 0.5 };   // 0..1
        double osc2KbdTrack    { 1.0 };   // p4 (V105) or related in V112
        int   octaveBase      { 0 };      // p9: Octave (-2..+2)
        int   keyShift        { 0 };      // p13: Key Shift (semitones)

        // Sub Oscillator
        double subOscGain      { 0.0 }; // p95: 0..1
        int   subOscWave      { 0 };    // p96: 0..3
        int   subOscOctave    { -1 };   // p97: -2 or -1

        // Amplifier
        // Import mapper selects native block segments; direct callers keep sample-clocked behavior.
        // Complete native VCF + gain + output Tone route, including verified LP24.
        // Core-history admission only; Filter always uses the common kernels.
        double ampAttackSec      { 0.01 };
        double ampDecaySec       { 0.1 }; // Finite endpoint, not an exponential time constant.
        double ampSustain        { 0.8 };
        double ampReleaseSec     { 0.2 }; // Nominal endpoint; imported range .01..60 seconds.
        double ampGainMultiplier { 0.8 }; // p35
        double masterVolume       { 1.0 };
        uint64_t globalSampleCount { 0 };
 // p78

        // Filter
        double cutoffIndex     { 1023.0 };
        double filterCutoffAdjustment {}; // Fractional extension of the native base-index curve.
        double cutoffHz        { 0.0 };
        double resonanceNorm   { 0.0 };
        double filterEnvAmount { 0.0 };
        double filterKeyTrackNorm { 0.0 }; // Original dial value for native integer tracking.
        double filterVelocity  { 0.0 }; // p24: enables the measured filter-velocity response.
        double ampVelocity     { 0.0 }; // p30: 0..1 sets up to 30 dB velocity attenuation.
        int   filterType      { 0 };
        double filterSaturation{ 0.0 };
        double pan             { 0.0 }; // p90: -1..+1

        // Modulation
        LfoSettings lfo1, lfo2;
        FilterEnvelopeControls filterEnv;
        ModulationEnvelopeControls modEnv;

        // Voice management
        int   polyphony      { 32 };
        int   playMode       { 0 };
        int   unisonVoices   { 0 };
        int   unisonPitchDial { 24 }; // Source p85; odd group slots are transposed.
        double unisonDetune   { 0.0 };
        double unisonPan      { 0.0 }; // Signed native spread; source64 is neutral.
        bool  isPolyphonic   { true };
        bool  portamentoAuto { false };
        double sourcePortamentoDial {0}; // Resolved real dial for the physical portamento receiver.
        bool resetIdleFilter {false}; // Audited mono noise idle reuse.

        // Effects
        bool nativePhaser { false };
        int nativePhaserPairs {1};
        double nativePhaserDepth {0},nativePhaserRate {0},nativePhaserFeedback {64};
        DelaySettings delay;
        ChorusSettings chorus;
        EqualizerSettings eq;

        // Global
        double pitchBendRange  { 2.0 };   // p72 (semitones)
        double globalFineTune  { 0.0 };   // p72: OSC1/2 Fine Tune, cents
        double hostBpm         { 120.0 };
        bool hasLfo(int destination) const noexcept
        { return (lfo1.on && lfo1.dest==destination) || (lfo2.on && lfo2.dest==destination); }
        bool hasPitchLfo() const noexcept { return hasLfo(0) || hasLfo(1); }

    };
}
