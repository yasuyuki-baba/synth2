#pragma once

#include "Synth1PatchDocumentIo.h"
#include "Synth1Patch.h"

#include <optional>

// Decode controls only; callers select native resolution or legacy adaptation.
Synth1Patch decodePatchControls(const Synth1PatchDocument& normalized, const Synth1PatchDocument& source);

std::optional<bool> readToggle(const Synth1PatchDocument& document, int index);
std::optional<int> readRaw(const Synth1PatchDocument& document, int index);

std::optional<Synth1OscillatorWaveform> decodeOsc1Wave(const Synth1PatchDocument& document);
std::optional<Synth1OscillatorWaveform> decodeOsc2Wave(const Synth1PatchDocument& document);
std::optional<Synth1ArpeggiatorType> decodeArpType(const Synth1PatchDocument& document);
std::optional<Synth1FilterType> decodeFilterKind(const Synth1PatchDocument& document);
std::optional<Synth1OscillatorModDestination> decodeOscModDest(const Synth1PatchDocument& document);
std::optional<Synth1SubOscillatorOctave> decodeSubOscOctave(const Synth1PatchDocument& document);
std::optional<Synth1OscillatorWaveform> decodeSubOscWave(const Synth1PatchDocument& document);
std::optional<Synth1VoicePlayMode> decodeVoiceMode(const Synth1PatchDocument& document);
std::optional<Synth1LfoDestination> decodeLfoDest(const Synth1PatchDocument& document, int index);
std::optional<Synth1LfoWaveform> decodeLfoWave(const Synth1PatchDocument& document, int index);
std::optional<Synth1EffectType> decodeEffectKind(const Synth1PatchDocument& document);
std::optional<Synth1DelayType> decodeDelayKind(const Synth1PatchDocument& document);
std::optional<Synth1ChorusType> decodeChorusKind(const Synth1PatchDocument& document);
std::optional<Synth1MidiControlSource> decodeMidiSource(const Synth1PatchDocument& document, int index);
std::optional<Synth1MidiControlDestination> decodeMidiDest(const Synth1PatchDocument& document, int index);

template <typename T>
void writeOptional(Synth1PatchDocument& document, int index, const std::optional<T>& value) {
    if (!value.has_value()) {
        eraseSynth1PatchEntryByIndex(document, index);
        return;
    }

    upsertSynth1PatchEntryByIndex(document, index, static_cast<int>(value.value()));
}

void writeToggle(Synth1PatchDocument& document, int index, const std::optional<bool>& value);

std::optional<int> encodeOsc1Wave(const std::optional<Synth1OscillatorWaveform>& waveform);
std::optional<int> encodeOsc2Wave(const std::optional<Synth1OscillatorWaveform>& waveform);
std::optional<int> encodeArpType(const std::optional<Synth1ArpeggiatorType>& type);
std::optional<int> encodeOscModDest(const std::optional<Synth1OscillatorModDestination>& destination);
std::optional<int> encodeSubOscOctave(const std::optional<Synth1SubOscillatorOctave>& octave);
std::optional<int> encodeSubOscWave(const std::optional<Synth1OscillatorWaveform>& waveform);
std::optional<int> encodeFilterKind(const std::optional<Synth1FilterType>& type);
std::optional<int> encodeVoiceMode(const std::optional<Synth1VoicePlayMode>& mode);
std::optional<int> encodeLfoDest(const std::optional<Synth1LfoDestination>& destination);
std::optional<int> encodeLfoWave(const std::optional<Synth1LfoWaveform>& waveform);
std::optional<int> encodeEffectKind(const std::optional<Synth1EffectType>& type);
std::optional<int> encodeDelayKind(const std::optional<Synth1DelayType>& type);
std::optional<int> encodeChorusKind(const std::optional<Synth1ChorusType>& type);
std::optional<int> encodeMidiSource(const std::optional<Synth1MidiControlSource>& source);
std::optional<int> encodeMidiDest(const std::optional<Synth1MidiControlDestination>& destination);

void importEffectControls(const Synth1PatchDocument& document, Synth1EffectPatch& effect);
void exportEffectControls(Synth1PatchDocument& document, const Synth1EffectPatch& effect);