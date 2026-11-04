#include "Synth1PatchIoCodecInternal.h"

namespace {

std::optional<int> findRawValue(const Synth1PatchDocument& document, int index) {
    for (const Synth1PatchEntry& entry : document.entries) {
        if (entry.index == index) {
            return entry.rawValue;
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<bool> readToggle(const Synth1PatchDocument& document, int index) {
    const std::optional<int> rawValue = findRawValue(document, index);
    if (!rawValue.has_value()) {
        return std::nullopt;
    }
    if (rawValue.value() == 0) {
        return false;
    }
    if (rawValue.value() == 1) {
        return true;
    }
    return std::nullopt;
}

std::optional<int> readRaw(const Synth1PatchDocument& document, int index) {
    return findRawValue(document, index);
}

std::optional<Synth1OscillatorWaveform> decodeOsc1Wave(const Synth1PatchDocument& document) {
    const std::optional<int> rawValue = findRawValue(document, 0);
    if (!rawValue.has_value()) {
        return std::nullopt;
    }

    switch (rawValue.value()) {
    case 0:
        return Synth1OscillatorWaveform::sine;
    case 1:
        return Synth1OscillatorWaveform::saw;
    case 2:
        return Synth1OscillatorWaveform::pulse;
    case 3:
        return Synth1OscillatorWaveform::triangle;
    default:
        return std::nullopt;
    }
}

std::optional<Synth1OscillatorWaveform> decodeOsc2Wave(const Synth1PatchDocument& document) {
    const std::optional<int> rawValue = findRawValue(document, 1);
    if (!rawValue.has_value()) {
        return std::nullopt;
    }

    switch (rawValue.value()) {
    case 1:
        return Synth1OscillatorWaveform::saw;
    case 2:
        return Synth1OscillatorWaveform::pulse;
    case 3:
        return Synth1OscillatorWaveform::triangle;
    case 4:
        return Synth1OscillatorWaveform::noise;
    default:
        return std::nullopt;
    }
}

std::optional<Synth1ArpeggiatorType> decodeArpType(const Synth1PatchDocument& document) {
    const std::optional<int> rawValue = findRawValue(document, 31);
    if (!rawValue.has_value()) {
        return std::nullopt;
    }
    switch (rawValue.value()) {
    case 1:
        return Synth1ArpeggiatorType::upDown;
    case 2:
        return Synth1ArpeggiatorType::up;
    case 3:
        return Synth1ArpeggiatorType::down;
    case 4:
        return Synth1ArpeggiatorType::random;
    default:
        return std::nullopt;
    }
}

std::optional<Synth1FilterType> decodeFilterKind(const Synth1PatchDocument& document) {
    const std::optional<int> rawValue = findRawValue(document, 14);
    if (!rawValue.has_value()) {
        return std::nullopt;
    }
    switch (rawValue.value()) {
    case 0:
        return Synth1FilterType::lp12;
    case 1:
        return Synth1FilterType::lp24;
    case 2:
        return Synth1FilterType::hp12;
    case 3:
        return Synth1FilterType::bp12;
    case 4:
        return Synth1FilterType::diode;
    default:
        return std::nullopt;
    }
}

std::optional<Synth1OscillatorModDestination> decodeOscModDest(const Synth1PatchDocument& document) {
    const std::optional<int> rawValue = findRawValue(document, 71);
    if (!rawValue.has_value()) {
        return std::nullopt;
    }

    switch (rawValue.value()) {
    case 0:
        return Synth1OscillatorModDestination::osc2;
    case 1:
        return Synth1OscillatorModDestination::fm;
    case 2:
        return Synth1OscillatorModDestination::pulseWidth;
    default:
        return std::nullopt;
    }
}

std::optional<Synth1SubOscillatorOctave> decodeSubOscOctave(const Synth1PatchDocument& document) {
    const std::optional<int> rawValue = findRawValue(document, 97);
    if (!rawValue.has_value()) {
        return std::nullopt;
    }

    switch (rawValue.value()) {
    case 0:
        return Synth1SubOscillatorOctave::same;
    case 1:
        return Synth1SubOscillatorOctave::minusOne;
    default:
        return std::nullopt;
    }
}

std::optional<Synth1OscillatorWaveform> decodeSubOscWave(const Synth1PatchDocument& document) {
    const std::optional<int> rawValue = findRawValue(document, 96);
    if (!rawValue.has_value()) {
        return std::nullopt;
    }

    switch (rawValue.value()) {
    case 0:
        return Synth1OscillatorWaveform::sine;
    case 1:
        return Synth1OscillatorWaveform::triangle;
    case 2:
        return Synth1OscillatorWaveform::saw;
    case 3:
        return Synth1OscillatorWaveform::pulse;
    default:
        return std::nullopt;
    }
}

std::optional<Synth1VoicePlayMode> decodeVoiceMode(const Synth1PatchDocument& document) {
    const std::optional<int> rawValue = findRawValue(document, 38);
    if (!rawValue.has_value()) {
        return std::nullopt;
    }

    switch (rawValue.value()) {
    case 0:
        return Synth1VoicePlayMode::poly;
    case 1:
        return Synth1VoicePlayMode::mono;
    case 2:
        return Synth1VoicePlayMode::legato;
    default:
        return std::nullopt;
    }
}

std::optional<Synth1LfoDestination> decodeLfoDest(const Synth1PatchDocument& document, int index) {
    const std::optional<int> rawValue = findRawValue(document, index);
    if (!rawValue.has_value()) {
        return std::nullopt;
    }

    switch (rawValue.value()) {
    case 1:
        return Synth1LfoDestination::osc2;
    case 2:
        return Synth1LfoDestination::osc1And2;
    case 3:
        return Synth1LfoDestination::filter;
    case 4:
        return Synth1LfoDestination::amp;
    case 5:
        return Synth1LfoDestination::pulseWidth;
    case 6:
        return Synth1LfoDestination::fm;
    case 7:
        return Synth1LfoDestination::pan;
    default:
        return std::nullopt;
    }
}

std::optional<Synth1LfoWaveform> decodeLfoWave(const Synth1PatchDocument& document, int index) {
    const std::optional<int> rawValue = findRawValue(document, index);
    if (!rawValue.has_value()) {
        return std::nullopt;
    }

    switch (rawValue.value()) {
    case 0:
        return Synth1LfoWaveform::saw;
    case 1:
        return Synth1LfoWaveform::triangle;
    case 2:
        return Synth1LfoWaveform::square;
    case 3:
        return Synth1LfoWaveform::randomSampleHold;
    case 4:
        return Synth1LfoWaveform::randomSmoothed;
    case 5:
        return Synth1LfoWaveform::sine;
    default:
        return std::nullopt;
    }
}

std::optional<Synth1EffectType> decodeEffectKind(const Synth1PatchDocument& document) {
    const std::optional<int> rawValue = findRawValue(document, 78);
    if (!rawValue.has_value()) {
        return std::nullopt;
    }

    switch (rawValue.value()) {
    case 0:
        return Synth1EffectType::overdrive1;
    case 1:
        return Synth1EffectType::overdrive2;
    case 2:
        return Synth1EffectType::decimatorDrive;
    case 3:
        return Synth1EffectType::decimator;
    case 4:
        return Synth1EffectType::ringMod;
    case 5:
        return Synth1EffectType::compressor;
    case 6:
        return Synth1EffectType::phaser1;
    case 7:
        return Synth1EffectType::phaser2;
    case 8:
        return Synth1EffectType::phaser3;
    case 9:
        return Synth1EffectType::phaser4;
    default:
        return std::nullopt;
    }
}

std::optional<Synth1DelayType> decodeDelayKind(const Synth1PatchDocument& document) {
    const std::optional<int> rawValue = findRawValue(document, 82);
    if (!rawValue.has_value()) {
        return std::nullopt;
    }

    switch (rawValue.value()) {
    case 0:
        return Synth1DelayType::stereo;
    case 1:
        return Synth1DelayType::crossFeedback;
    case 2:
        return Synth1DelayType::pingPong;
    default:
        return std::nullopt;
    }
}

std::optional<Synth1ChorusType> decodeChorusKind(const Synth1PatchDocument& document) {
    const std::optional<int> rawValue = findRawValue(document, 64);
    if (!rawValue.has_value()) {
        return std::nullopt;
    }

    switch (rawValue.value()) {
    case 1:
        return Synth1ChorusType::x1;
    case 2:
        return Synth1ChorusType::x2;
    case 4:
        return Synth1ChorusType::x4;
    default:
        return std::nullopt;
    }
}

std::optional<Synth1MidiControlSource> decodeMidiSource(const Synth1PatchDocument& document, int index) {
    const std::optional<int> rawValue = findRawValue(document, index);
    if (!rawValue.has_value()) {
        return std::nullopt;
    }

    switch (rawValue.value()) {
    case 45057:
        return Synth1MidiControlSource::modWheel;
    case 45058:
        return Synth1MidiControlSource::breathControl;
    case 45059:
        return Synth1MidiControlSource::control3;
    case 45060:
        return Synth1MidiControlSource::footControl;
    case 45061:
        return Synth1MidiControlSource::portamentoTime;
    case 45062:
        return Synth1MidiControlSource::dataEntry;
    case 45063:
        return Synth1MidiControlSource::volume;
    case 45064:
        return Synth1MidiControlSource::balance;
    case 45065:
        return Synth1MidiControlSource::control9;
    case 45066:
        return Synth1MidiControlSource::pan;
    case 45067:
        return Synth1MidiControlSource::expression;
    case 53248:
        return Synth1MidiControlSource::aftertouch;
    case 57344:
        return Synth1MidiControlSource::pitchBend;
    default:
        return std::nullopt;
    }
}

std::optional<Synth1MidiControlDestination> decodeMidiDest(const Synth1PatchDocument& document, int index) {
    const std::optional<int> rawValue = findRawValue(document, index);
    if (!rawValue.has_value()) {
        return std::nullopt;
    }

    switch (rawValue.value()) {
    case -1:
        return Synth1MidiControlDestination::none;
    case 2:
        return Synth1MidiControlDestination::osc2Pitch;
    case 5:
        return Synth1MidiControlDestination::oscMix;
    case 8:
        return Synth1MidiControlDestination::oscPulseWidth;
    case 11:
        return Synth1MidiControlDestination::oscEnvAmount;
    case 12:
        return Synth1MidiControlDestination::oscEnvAttack;
    case 13:
        return Synth1MidiControlDestination::oscEnvDecay;
    case 15:
        return Synth1MidiControlDestination::filterAttack;
    case 16:
        return Synth1MidiControlDestination::filterDecay;
    case 17:
        return Synth1MidiControlDestination::filterSustain;
    case 18:
        return Synth1MidiControlDestination::filterRelease;
    case 19:
        return Synth1MidiControlDestination::filterFreq;
    case 20:
        return Synth1MidiControlDestination::filterResonance;
    case 21:
        return Synth1MidiControlDestination::filterAmount;
    case 22:
        return Synth1MidiControlDestination::filterKeyTrack;
    case 23:
        return Synth1MidiControlDestination::filterSaturation;
    case 25:
        return Synth1MidiControlDestination::ampAttack;
    case 26:
        return Synth1MidiControlDestination::ampDecay;
    case 27:
        return Synth1MidiControlDestination::ampSustain;
    case 28:
        return Synth1MidiControlDestination::ampRelease;
    case 29:
        return Synth1MidiControlDestination::ampGain;
    case 34:
        return Synth1MidiControlDestination::arpGate;
    case 35:
        return Synth1MidiControlDestination::delayTime;
    case 36:
        return Synth1MidiControlDestination::delayFeedback;
    case 37:
        return Synth1MidiControlDestination::delayDryWet;
    case 39:
        return Synth1MidiControlDestination::portamentoTime;
    case 43:
        return Synth1MidiControlDestination::lfo1Speed;
    case 44:
        return Synth1MidiControlDestination::lfo1Depth;
    case 45:
        return Synth1MidiControlDestination::osc1Fm;
    case 48:
        return Synth1MidiControlDestination::lfo2Speed;
    case 49:
        return Synth1MidiControlDestination::lfo2Depth;
    case 52:
        return Synth1MidiControlDestination::chorusTime;
    case 53:
        return Synth1MidiControlDestination::chorusDepth;
    case 54:
        return Synth1MidiControlDestination::chorusRate;
    case 55:
        return Synth1MidiControlDestination::chorusFeedback;
    case 56:
        return Synth1MidiControlDestination::chorusLevel;
    case 60:
        return Synth1MidiControlDestination::eqTone;
    case 61:
        return Synth1MidiControlDestination::eqFreq;
    case 62:
        return Synth1MidiControlDestination::eqLevel;
    case 63:
        return Synth1MidiControlDestination::eqQ;
    case 75:
        return Synth1MidiControlDestination::unisonDetune;
    case 76:
        return Synth1MidiControlDestination::osc1Detune;
    case 79:
        return Synth1MidiControlDestination::effectControl1;
    case 80:
        return Synth1MidiControlDestination::effectControl2;
    case 81:
        return Synth1MidiControlDestination::effectOutput;
    case 83:
        return Synth1MidiControlDestination::delaySpread;
    case 84:
        return Synth1MidiControlDestination::unisonSpread;
    case 85:
        return Synth1MidiControlDestination::unisonPitch;
    case 90:
        return Synth1MidiControlDestination::pan;
    case 91:
        return Synth1MidiControlDestination::oscPhaseShift;
    case 92:
        return Synth1MidiControlDestination::unisonPhaseShift;
    case 95:
        return Synth1MidiControlDestination::osc1SubLevel;
    case 98:
        return Synth1MidiControlDestination::delayTone;
    default:
        return std::nullopt;
    }
}

void writeToggle(Synth1PatchDocument& document, int index, const std::optional<bool>& value) {
    if (!value.has_value()) {
        eraseSynth1PatchEntryByIndex(document, index);
        return;
    }

    upsertSynth1PatchEntryByIndex(document, index, value.value() ? 1 : 0);
}

std::optional<int> encodeOsc1Wave(const std::optional<Synth1OscillatorWaveform>& waveform) {
    if (!waveform.has_value()) {
        return std::nullopt;
    }

    switch (waveform.value()) {
    case Synth1OscillatorWaveform::sine:
        return 0;
    case Synth1OscillatorWaveform::saw:
        return 1;
    case Synth1OscillatorWaveform::pulse:
        return 2;
    case Synth1OscillatorWaveform::triangle:
        return 3;
    case Synth1OscillatorWaveform::noise:
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<int> encodeOsc2Wave(const std::optional<Synth1OscillatorWaveform>& waveform) {
    if (!waveform.has_value()) {
        return std::nullopt;
    }

    switch (waveform.value()) {
    case Synth1OscillatorWaveform::saw:
        return 1;
    case Synth1OscillatorWaveform::pulse:
        return 2;
    case Synth1OscillatorWaveform::triangle:
        return 3;
    case Synth1OscillatorWaveform::noise:
        return 4;
    case Synth1OscillatorWaveform::sine:
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<int> encodeArpType(const std::optional<Synth1ArpeggiatorType>& type) {
    if (!type.has_value()) {
        return std::nullopt;
    }
    return static_cast<int>(type.value());
}

std::optional<int> encodeOscModDest(const std::optional<Synth1OscillatorModDestination>& destination) {
    if (!destination.has_value()) {
        return std::nullopt;
    }

    switch (destination.value()) {
    case Synth1OscillatorModDestination::osc2:
        return 0;
    case Synth1OscillatorModDestination::fm:
        return 1;
    case Synth1OscillatorModDestination::pulseWidth:
        return 2;
    }

    return std::nullopt;
}

std::optional<int> encodeSubOscOctave(const std::optional<Synth1SubOscillatorOctave>& octave) {
    if (!octave.has_value()) {
        return std::nullopt;
    }

    switch (octave.value()) {
    case Synth1SubOscillatorOctave::same:
        return 0;
    case Synth1SubOscillatorOctave::minusOne:
        return 1;
    }

    return std::nullopt;
}

std::optional<int> encodeSubOscWave(const std::optional<Synth1OscillatorWaveform>& waveform) {
    if (!waveform.has_value()) {
        return std::nullopt;
    }

    switch (waveform.value()) {
    case Synth1OscillatorWaveform::sine:
        return 0;
    case Synth1OscillatorWaveform::triangle:
        return 1;
    case Synth1OscillatorWaveform::saw:
        return 2;
    case Synth1OscillatorWaveform::pulse:
        return 3;
    case Synth1OscillatorWaveform::noise:
        return std::nullopt;
    }

    return std::nullopt;
}

std::optional<int> encodeFilterKind(const std::optional<Synth1FilterType>& type) {
    if (!type.has_value()) {
        return std::nullopt;
    }

    switch (type.value()) {
    case Synth1FilterType::lp12:
        return 0;
    case Synth1FilterType::lp24:
        return 1;
    case Synth1FilterType::hp12:
        return 2;
    case Synth1FilterType::bp12:
        return 3;
    case Synth1FilterType::diode:
        return 4;
    }

    return std::nullopt;
}

std::optional<int> encodeVoiceMode(const std::optional<Synth1VoicePlayMode>& mode) {
    if (!mode.has_value()) {
        return std::nullopt;
    }

    switch (mode.value()) {
    case Synth1VoicePlayMode::poly:
        return 0;
    case Synth1VoicePlayMode::mono:
        return 1;
    case Synth1VoicePlayMode::legato:
        return 2;
    }

    return std::nullopt;
}

std::optional<int> encodeLfoDest(const std::optional<Synth1LfoDestination>& destination) {
    if (!destination.has_value()) {
        return std::nullopt;
    }
    return static_cast<int>(destination.value());
}

std::optional<int> encodeLfoWave(const std::optional<Synth1LfoWaveform>& waveform) {
    if (!waveform.has_value()) {
        return std::nullopt;
    }

    switch (waveform.value()) {
    case Synth1LfoWaveform::saw:
        return 0;
    case Synth1LfoWaveform::triangle:
        return 1;
    case Synth1LfoWaveform::square:
        return 2;
    case Synth1LfoWaveform::randomSampleHold:
        return 3;
    case Synth1LfoWaveform::randomSmoothed:
        return 4;
    case Synth1LfoWaveform::sine:
        return 5;
    }

    return std::nullopt;
}

std::optional<int> encodeEffectKind(const std::optional<Synth1EffectType>& type) {
    if (!type.has_value()) {
        return std::nullopt;
    }
    return static_cast<int>(type.value());
}

std::optional<int> encodeDelayKind(const std::optional<Synth1DelayType>& type) {
    if (!type.has_value()) {
        return std::nullopt;
    }

    switch (type.value()) {
    case Synth1DelayType::stereo:
        return 0;
    case Synth1DelayType::crossFeedback:
        return 1;
    case Synth1DelayType::pingPong:
        return 2;
    }

    return std::nullopt;
}

std::optional<int> encodeChorusKind(const std::optional<Synth1ChorusType>& type) {
    if (!type.has_value()) {
        return std::nullopt;
    }
    return static_cast<int>(type.value());
}

std::optional<int> encodeMidiSource(const std::optional<Synth1MidiControlSource>& source) {
    if (!source.has_value()) {
        return std::nullopt;
    }
    return static_cast<int>(source.value());
}

std::optional<int> encodeMidiDest(const std::optional<Synth1MidiControlDestination>& destination) {
    if (!destination.has_value()) {
        return std::nullopt;
    }
    return static_cast<int>(destination.value());
}

void importEffectControls(const Synth1PatchDocument& document, Synth1EffectPatch& effect) {
    if (!effect.type.has_value()) {
        return;
    }

    const std::optional<int> control1 = readRaw(document, 79);
    const std::optional<int> control2 = readRaw(document, 80);
    const std::optional<int> control3 = readRaw(document, 81);

    switch (effect.type.value()) {
    case Synth1EffectType::overdrive1:
    case Synth1EffectType::overdrive2:
    case Synth1EffectType::decimatorDrive:
        effect.distortion.amount = control1;
        effect.distortion.lowPassCutoff = control2;
        effect.distortion.level = control3;
        break;
    case Synth1EffectType::decimator:
        effect.decimator.sampleRate = control1;
        effect.decimator.bitDepth = control2;
        effect.decimator.mix = control3;
        break;
    case Synth1EffectType::ringMod:
        effect.ringMod.frequency = control1;
        effect.ringMod.reservedControl2 = control2;
        effect.ringMod.mix = control3;
        break;
    case Synth1EffectType::compressor:
        effect.compressor.depth = control1;
        effect.compressor.attackTime = control2;
        effect.compressor.level = control3;
        break;
    case Synth1EffectType::phaser1:
    case Synth1EffectType::phaser2:
    case Synth1EffectType::phaser3:
    case Synth1EffectType::phaser4:
        effect.phaser.lfoDepth = control1;
        effect.phaser.lfoFrequency = control2;
        effect.phaser.feedback = control3;
        break;
    }
}

void exportEffectControls(Synth1PatchDocument& document, const Synth1EffectPatch& effect) {
    if (!effect.type.has_value()) {
        eraseSynth1PatchEntryByIndex(document, 79);
        eraseSynth1PatchEntryByIndex(document, 80);
        eraseSynth1PatchEntryByIndex(document, 81);
        return;
    }

    switch (effect.type.value()) {
    case Synth1EffectType::overdrive1:
    case Synth1EffectType::overdrive2:
    case Synth1EffectType::decimatorDrive:
        writeOptional(document, 79, effect.distortion.amount);
        writeOptional(document, 80, effect.distortion.lowPassCutoff);
        writeOptional(document, 81, effect.distortion.level);
        break;
    case Synth1EffectType::decimator:
        writeOptional(document, 79, effect.decimator.sampleRate);
        writeOptional(document, 80, effect.decimator.bitDepth);
        writeOptional(document, 81, effect.decimator.mix);
        break;
    case Synth1EffectType::ringMod:
        writeOptional(document, 79, effect.ringMod.frequency);
        writeOptional(document, 80, effect.ringMod.reservedControl2);
        writeOptional(document, 81, effect.ringMod.mix);
        break;
    case Synth1EffectType::compressor:
        writeOptional(document, 79, effect.compressor.depth);
        writeOptional(document, 80, effect.compressor.attackTime);
        writeOptional(document, 81, effect.compressor.level);
        break;
    case Synth1EffectType::phaser1:
    case Synth1EffectType::phaser2:
    case Synth1EffectType::phaser3:
    case Synth1EffectType::phaser4:
        writeOptional(document, 79, effect.phaser.lfoDepth);
        writeOptional(document, 80, effect.phaser.lfoFrequency);
        writeOptional(document, 81, effect.phaser.feedback);
        break;
    }
}
