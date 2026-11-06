#pragma once

#include <optional>
#include <bitset>
#include <string>

inline constexpr int kSynth1LatestPatchFormatVersion = 113;

enum class Synth1OscillatorWaveform {
    sine,
    saw,
    pulse,
    triangle,
    noise,
};

enum class Synth1ArpeggiatorType {
    upDown = 1,
    up = 2,
    down = 3,
    random = 4,
};

enum class Synth1OscillatorModDestination {
    osc2,
    fm,
    pulseWidth,
};

enum class Synth1SubOscillatorOctave {
    same,
    minusOne,
};

enum class Synth1FilterType {
    lp12,
    lp24,
    hp12,
    bp12,
    diode,
};

enum class Synth1DelayType {
    stereo,
    crossFeedback,
    pingPong,
};

enum class Synth1ChorusType {
    x1 = 1,
    x2 = 2,
    x4 = 4,
};

enum class Synth1LfoDestination {
    osc2 = 1,
    osc1And2 = 2,
    filter = 3,
    amp = 4,
    pulseWidth = 5,
    fm = 6,
    pan = 7,
};

enum class Synth1LfoWaveform {
    saw,
    triangle,
    square,
    randomSampleHold,
    randomSmoothed,
    sine,
};

enum class Synth1VoicePlayMode {
    poly,
    mono,
    legato,
};

enum class Synth1EffectType {
    overdrive1,
    overdrive2,
    decimatorDrive,
    decimator,
    ringMod,
    compressor,
    phaser1,
    phaser2,
    phaser3,
    phaser4,
};

enum class Synth1MidiControlSource {
    modWheel = 45057,
    breathControl = 45058,
    control3 = 45059,
    footControl = 45060,
    portamentoTime = 45061,
    dataEntry = 45062,
    volume = 45063,
    balance = 45064,
    control9 = 45065,
    pan = 45066,
    expression = 45067,
    aftertouch = 53248,
    pitchBend = 57344,
};

enum class Synth1MidiControlDestination {
    none = -1,
    osc2Pitch = 2,
    oscMix = 5,
    oscPulseWidth = 8,
    oscEnvAmount = 11,
    oscEnvAttack = 12,
    oscEnvDecay = 13,
    filterAttack = 15,
    filterDecay = 16,
    filterSustain = 17,
    filterRelease = 18,
    filterFreq = 19,
    filterResonance = 20,
    filterAmount = 21,
    filterKeyTrack = 22,
    filterSaturation = 23,
    ampAttack = 25,
    ampDecay = 26,
    ampSustain = 27,
    ampRelease = 28,
    ampGain = 29,
    arpGate = 34,
    delayTime = 35,
    delayFeedback = 36,
    delayDryWet = 37,
    portamentoTime = 39,
    lfo1Speed = 43,
    lfo1Depth = 44,
    osc1Fm = 45,
    lfo2Speed = 48,
    lfo2Depth = 49,
    chorusTime = 52,
    chorusDepth = 53,
    chorusRate = 54,
    chorusFeedback = 55,
    chorusLevel = 56,
    eqTone = 60,
    eqFreq = 61,
    eqLevel = 62,
    eqQ = 63,
    unisonDetune = 75,
    osc1Detune = 76,
    effectControl1 = 79,
    effectControl2 = 80,
    effectOutput = 81,
    delaySpread = 83,
    unisonSpread = 84,
    unisonPitch = 85,
    pan = 90,
    oscPhaseShift = 91,
    unisonPhaseShift = 92,
    osc1SubLevel = 95,
    delayTone = 98,
};

struct Synth1OscillatorPatch {
    std::optional<Synth1OscillatorWaveform> shape;
    std::optional<int> octave;
    std::optional<int> coarseTune;
    std::optional<int> fineTune;
    std::optional<int> detune;
    std::optional<int> color;
    std::optional<int> phase;
    std::optional<int> level;
};

struct Synth1OscillatorsPatch {
    Synth1OscillatorPatch osc1;
    Synth1OscillatorPatch osc2;
    std::optional<bool> osc2KeyTrack;
    std::optional<int> mix;
    std::optional<bool> sync;
    std::optional<bool> ring;
    std::optional<int> pulseWidth;
    std::optional<int> keyShift;
    std::optional<bool> modEnvEnabled;
    std::optional<int> modEnvAmount;
    std::optional<int> modEnvAttack;
    std::optional<int> modEnvDecay;
    std::optional<Synth1OscillatorModDestination> modEnvDestination;
    std::optional<int> fmAmount;
    std::optional<int> noiseLevel;
    std::optional<int> reserved72;
    std::optional<int> phaseShift;
    std::optional<int> subOscLevel;
    std::optional<Synth1OscillatorWaveform> subOscShape;
    std::optional<Synth1SubOscillatorOctave> subOscOctave;
    std::optional<int> spread;
};

struct Synth1FilterPatch {
    std::optional<Synth1FilterType> type;
    std::optional<int> freq;
    std::optional<int> resonance;
    std::optional<int> envelopeAmount;
    std::optional<int> keyTrack;
    std::optional<int> saturation;
    std::optional<bool> velocitySwitch;
    std::optional<int> attack;
    std::optional<int> decay;
    std::optional<int> sustain;
    std::optional<int> release;
};

struct Synth1AmplifierPatch {
    std::optional<int> level;
    std::optional<int> attack;
    std::optional<int> decay;
    std::optional<int> sustain;
    std::optional<int> release;
    std::optional<int> velocitySensitivity;
};

struct Synth1ArpeggiatorPatch {
    std::optional<bool> enabled;
    std::optional<Synth1ArpeggiatorType> type;
    std::optional<int> range;
    std::optional<int> beat;
    std::optional<int> gate;
};

struct Synth1TempoDelayPatch {
    std::optional<bool> enabled;
    std::optional<Synth1DelayType> type;
    std::optional<int> time;
    std::optional<int> spread;
    std::optional<int> feedback;
    std::optional<int> tone;
    std::optional<int> mix;
};

struct Synth1ChorusFlangerPatch {
    std::optional<bool> enabled;
    std::optional<Synth1ChorusType> type;
    std::optional<int> time;
    std::optional<int> rate;
    std::optional<int> depth;
    std::optional<int> feedback;
    std::optional<int> level;
};

struct Synth1EqualizerPanPatch {
    std::optional<int> tone;
    std::optional<int> freq;
    std::optional<int> level;
    std::optional<int> q;
    std::optional<int> pan;
};

struct Synth1DistortionEffectPatch {
    std::optional<int> amount;
    std::optional<int> lowPassCutoff;
    std::optional<int> level;
};

struct Synth1DecimatorEffectPatch {
    std::optional<int> sampleRate;
    std::optional<int> bitDepth;
    std::optional<int> mix;
};

struct Synth1RingModEffectPatch {
    std::optional<int> frequency;
    std::optional<int> reservedControl2;
    std::optional<int> mix;
};

struct Synth1CompressorEffectPatch {
    std::optional<int> depth;
    std::optional<int> attackTime;
    std::optional<int> level;
};

struct Synth1PhaserEffectPatch {
    std::optional<int> lfoDepth;
    std::optional<int> lfoFrequency;
    std::optional<int> feedback;
};

struct Synth1EffectPatch {
    std::optional<bool> enabled;
    std::optional<Synth1EffectType> type;
    Synth1DistortionEffectPatch distortion;
    Synth1DecimatorEffectPatch decimator;
    Synth1RingModEffectPatch ringMod;
    Synth1CompressorEffectPatch compressor;
    Synth1PhaserEffectPatch phaser;
};

struct Synth1VoicePatch {
    std::optional<Synth1VoicePlayMode> playMode;
    std::optional<int> portamento;
    std::optional<bool> portamentoAuto;
    std::optional<bool> unisonEnabled;
    std::optional<int> unisonVoices;
    std::optional<int> unisonDetune;
    std::optional<int> unisonPhaseShift;
    std::optional<int> spread;
    std::optional<int> unisonPitch;
    std::optional<int> polyphony;
};

struct Synth1MidiRoutePatch {
    std::optional<Synth1MidiControlSource> source;
    std::optional<int> sensitivity;
    std::optional<Synth1MidiControlDestination> destination;
};

struct Synth1WheelMidiPatch {
    std::optional<int> pitchBendRange;
    Synth1MidiRoutePatch route1;
    Synth1MidiRoutePatch route2;
};

struct Synth1LfoPatch {
    std::optional<bool> enabled;
    std::optional<Synth1LfoDestination> destination;
    std::optional<Synth1LfoWaveform> wave;
    std::optional<int> rate;
    std::optional<int> amount;
    std::optional<bool> keySync;
    std::optional<bool> tempoSync;
};

struct Synth1LfosPatch {
    Synth1LfoPatch lfo1;
    Synth1LfoPatch lfo2;
};

// Source identity is independent of converted values and implicit derivations.
// Programmatically constructed models have no source; canonical export does not
// reinterpret this metadata as an edit/preservation policy.
struct Synth1PatchSourceInfo {
    std::optional<int> version;
    std::bitset<126> explicitParameters;

    bool hasParameter(int index) const noexcept {
        return index >= 0 && index < 126 && explicitParameters[static_cast<std::size_t>(index)];
    }
};

struct Synth1Patch {
    std::string name;
    Synth1OscillatorsPatch oscillators;
    Synth1FilterPatch filter;
    Synth1AmplifierPatch amplifier;
    Synth1ArpeggiatorPatch arpeggiator;
    Synth1TempoDelayPatch tempoDelay;
    Synth1ChorusFlangerPatch chorusFlanger;
    Synth1EqualizerPanPatch equalizerPan;
    Synth1EffectPatch effect;
    Synth1VoicePatch voice;
    Synth1WheelMidiPatch wheelMidi;
    Synth1LfosPatch lfo;
    std::optional<Synth1PatchSourceInfo> source;
};
