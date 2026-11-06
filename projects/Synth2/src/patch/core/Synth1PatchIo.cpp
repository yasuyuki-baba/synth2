#include "Synth1PatchIo.h"
#include "Synth1NativeLoading.h"

#include "Synth1PatchIoCodecInternal.h"
#include "Synth1PatchDocumentIo.h"
#include "Synth1PatchParser.h"

Synth1Patch decodePatchControls(const Synth1PatchDocument& normalizedDocument, const Synth1PatchDocument& document) {
    Synth1Patch patch;
    patch.name = normalizedDocument.name;
    patch.source.emplace();
    patch.source->version = document.version();
    for (const auto& entry : document.entries)
        if (entry.index >= 0 && entry.index < 126)
            patch.source->explicitParameters.set(static_cast<std::size_t>(entry.index));

    patch.oscillators.osc1.shape = decodeOsc1Wave(normalizedDocument);
    patch.oscillators.osc2.shape = decodeOsc2Wave(normalizedDocument);
    patch.oscillators.osc2.coarseTune = readRaw(normalizedDocument, 2);
    patch.oscillators.osc2.fineTune = readRaw(normalizedDocument, 3);
    patch.oscillators.osc2KeyTrack = readToggle(normalizedDocument, 4);
    patch.oscillators.mix = readRaw(normalizedDocument, 5);
    patch.oscillators.sync = readToggle(normalizedDocument, 6);
    patch.oscillators.ring = readToggle(normalizedDocument, 7);
    patch.oscillators.pulseWidth = readRaw(normalizedDocument, 8);
    patch.oscillators.keyShift = readRaw(normalizedDocument, 9);
    patch.oscillators.modEnvEnabled = readToggle(normalizedDocument, 10);
    patch.oscillators.modEnvAmount = readRaw(normalizedDocument, 11);
    patch.oscillators.modEnvAttack = readRaw(normalizedDocument, 12);
    patch.oscillators.modEnvDecay = readRaw(normalizedDocument, 13);
    patch.oscillators.modEnvDestination = decodeOscModDest(normalizedDocument);
    patch.oscillators.fmAmount = readRaw(normalizedDocument, 45);
    patch.oscillators.reserved72 = readRaw(normalizedDocument, 72);
    patch.oscillators.phaseShift = readRaw(normalizedDocument, 91);
    patch.oscillators.subOscLevel = readRaw(normalizedDocument, 95);
    patch.oscillators.subOscShape = decodeSubOscWave(normalizedDocument);
    patch.oscillators.subOscOctave = decodeSubOscOctave(normalizedDocument);
    patch.oscillators.osc1.detune = readRaw(normalizedDocument, 76);

    patch.filter.type = decodeFilterKind(normalizedDocument);
    patch.filter.attack = readRaw(normalizedDocument, 15);
    patch.filter.decay = readRaw(normalizedDocument, 16);
    patch.filter.sustain = readRaw(normalizedDocument, 17);
    patch.filter.release = readRaw(normalizedDocument, 18);
    patch.filter.freq = readRaw(normalizedDocument, 19);
    patch.filter.resonance = readRaw(normalizedDocument, 20);
    patch.filter.envelopeAmount = readRaw(normalizedDocument, 21);
    patch.filter.keyTrack = readRaw(normalizedDocument, 22);
    patch.filter.saturation = readRaw(normalizedDocument, 23);
    patch.filter.velocitySwitch = readToggle(normalizedDocument, 24);

    patch.amplifier.attack = readRaw(normalizedDocument, 25);
    patch.amplifier.decay = readRaw(normalizedDocument, 26);
    patch.amplifier.sustain = readRaw(normalizedDocument, 27);
    patch.amplifier.release = readRaw(normalizedDocument, 28);
    patch.amplifier.level = readRaw(normalizedDocument, 29);
    patch.amplifier.velocitySensitivity = readRaw(normalizedDocument, 30);

    patch.arpeggiator.type = decodeArpType(normalizedDocument);
    patch.arpeggiator.range = readRaw(normalizedDocument, 32);
    patch.arpeggiator.beat = readRaw(normalizedDocument, 33);
    patch.arpeggiator.gate = readRaw(normalizedDocument, 34);
    patch.arpeggiator.enabled = readToggle(normalizedDocument, 59);

    patch.tempoDelay.enabled = readToggle(normalizedDocument, 65);
    patch.tempoDelay.type = decodeDelayKind(normalizedDocument);
    patch.tempoDelay.time = readRaw(normalizedDocument, 35);
    patch.tempoDelay.spread = readRaw(normalizedDocument, 83);
    patch.tempoDelay.feedback = readRaw(normalizedDocument, 36);
    patch.tempoDelay.tone = readRaw(normalizedDocument, 98);
    patch.tempoDelay.mix = readRaw(normalizedDocument, 37);

    patch.chorusFlanger.enabled = readToggle(normalizedDocument, 66);
    patch.chorusFlanger.type = decodeChorusKind(normalizedDocument);
    patch.chorusFlanger.time = readRaw(normalizedDocument, 52);
    patch.chorusFlanger.depth = readRaw(normalizedDocument, 53);
    patch.chorusFlanger.rate = readRaw(normalizedDocument, 54);
    patch.chorusFlanger.feedback = readRaw(normalizedDocument, 55);
    patch.chorusFlanger.level = readRaw(normalizedDocument, 56);

    patch.equalizerPan.tone = readRaw(normalizedDocument, 60);
    patch.equalizerPan.freq = readRaw(normalizedDocument, 61);
    patch.equalizerPan.level = readRaw(normalizedDocument, 62);
    patch.equalizerPan.q = readRaw(normalizedDocument, 63);
    patch.equalizerPan.pan = readRaw(normalizedDocument, 90);

    patch.effect.enabled = readToggle(normalizedDocument, 77);
    patch.effect.type = decodeEffectKind(normalizedDocument);
    importEffectControls(normalizedDocument, patch.effect);

    patch.voice.playMode = decodeVoiceMode(normalizedDocument);
    patch.voice.portamento = readRaw(normalizedDocument, 39);
    patch.voice.unisonEnabled = readToggle(normalizedDocument, 73);
    patch.voice.portamentoAuto = readToggle(normalizedDocument, 74);
    patch.voice.unisonDetune = readRaw(normalizedDocument, 75);
    patch.voice.spread = readRaw(normalizedDocument, 84);
    patch.voice.unisonPitch = readRaw(normalizedDocument, 85);
    patch.voice.unisonPhaseShift = readRaw(normalizedDocument, 92);
    patch.voice.unisonVoices = readRaw(normalizedDocument, 93);
    patch.voice.polyphony = readRaw(normalizedDocument, 94);

    patch.wheelMidi.pitchBendRange = readRaw(normalizedDocument, 40);
    patch.wheelMidi.route1.sensitivity = readRaw(normalizedDocument, 50);
    patch.wheelMidi.route2.sensitivity = readRaw(normalizedDocument, 51);
    if (normalizedDocument.family() != Synth1PatchFormatFamily::format108) {
        patch.wheelMidi.route1.source = decodeMidiSource(normalizedDocument, 86);
        patch.wheelMidi.route1.destination = decodeMidiDest(normalizedDocument, 87);
        patch.wheelMidi.route2.source = decodeMidiSource(normalizedDocument, 88);
        patch.wheelMidi.route2.destination = decodeMidiDest(normalizedDocument, 89);
    }

    patch.lfo.lfo1.enabled = readToggle(normalizedDocument, 57);
    patch.lfo.lfo1.destination = decodeLfoDest(normalizedDocument, 41);
    patch.lfo.lfo1.wave = decodeLfoWave(normalizedDocument, 42);
    patch.lfo.lfo1.rate = readRaw(normalizedDocument, 43);
    patch.lfo.lfo1.amount = readRaw(normalizedDocument, 44);
    patch.lfo.lfo1.tempoSync = readToggle(normalizedDocument, 67);
    patch.lfo.lfo1.keySync = readToggle(normalizedDocument, 68);

    patch.lfo.lfo2.enabled = readToggle(normalizedDocument, 58);
    patch.lfo.lfo2.destination = decodeLfoDest(normalizedDocument, 46);
    patch.lfo.lfo2.wave = decodeLfoWave(normalizedDocument, 47);
    patch.lfo.lfo2.rate = readRaw(normalizedDocument, 48);
    patch.lfo.lfo2.amount = readRaw(normalizedDocument, 49);
    patch.lfo.lfo2.tempoSync = readToggle(normalizedDocument, 69);
    patch.lfo.lfo2.keySync = readToggle(normalizedDocument, 70);

    return patch;
}

Synth1Patch importNativeSynth1Patch(const Synth1PatchDocument& document) {
    return decodeNativePatchControls(synth1::patch::resolveNativePatchControls(document),document);
}

Synth1Patch decodeNativePatchControls(const synth1::patch::NativePatchControls& resolved, const Synth1PatchDocument& document) {
    std::vector<Synth1PatchEntry> entries;
    for(int index=0;index<99;++index)entries.push_back({index,resolved.values[index]});
    auto patch=decodePatchControls(Synth1PatchDocument::makeParsed(document.name,113,std::move(entries)),document);
    // The native Ring receiver treats any nonzero stored control as enabled.
    // Keep its original integer in resolved controls and the source document.
    patch.oscillators.ring=resolved.values[7]!=0;
    return patch;
}

Synth1PatchDocument exportSynth1Patch(const Synth1Patch& patch) {
    Synth1PatchDocument document = Synth1PatchDocument::makeParsed(
        patch.name,
        kSynth1LatestPatchFormatVersion,
        {});

    writeOptional(document, 0, encodeOsc1Wave(patch.oscillators.osc1.shape));
    writeOptional(document, 1, encodeOsc2Wave(patch.oscillators.osc2.shape));
    writeOptional(document, 2, patch.oscillators.osc2.coarseTune);
    writeOptional(document, 3, patch.oscillators.osc2.fineTune);
    writeToggle(document, 4, patch.oscillators.osc2KeyTrack);
    writeOptional(document, 5, patch.oscillators.mix);
    writeToggle(document, 6, patch.oscillators.sync);
    writeToggle(document, 7, patch.oscillators.ring);
    writeOptional(document, 8, patch.oscillators.pulseWidth);
    writeOptional(document, 9, patch.oscillators.keyShift);
    writeToggle(document, 10, patch.oscillators.modEnvEnabled);
    writeOptional(document, 11, patch.oscillators.modEnvAmount);
    writeOptional(document, 12, patch.oscillators.modEnvAttack);
    writeOptional(document, 13, patch.oscillators.modEnvDecay);
    writeOptional(document, 71, encodeOscModDest(patch.oscillators.modEnvDestination));
    writeOptional(document, 45, patch.oscillators.fmAmount);
    writeOptional(document, 72, patch.oscillators.reserved72);
    writeOptional(document, 91, patch.oscillators.phaseShift);
    writeOptional(document, 95, patch.oscillators.subOscLevel);
    writeOptional(document, 96, encodeSubOscWave(patch.oscillators.subOscShape));
    writeOptional(document, 97, encodeSubOscOctave(patch.oscillators.subOscOctave));
    writeOptional(document, 76, patch.oscillators.osc1.detune);

    writeOptional(document, 14, encodeFilterKind(patch.filter.type));
    writeOptional(document, 15, patch.filter.attack);
    writeOptional(document, 16, patch.filter.decay);
    writeOptional(document, 17, patch.filter.sustain);
    writeOptional(document, 18, patch.filter.release);
    writeOptional(document, 19, patch.filter.freq);
    writeOptional(document, 20, patch.filter.resonance);
    writeOptional(document, 21, patch.filter.envelopeAmount);
    writeOptional(document, 22, patch.filter.keyTrack);
    writeOptional(document, 23, patch.filter.saturation);
    writeToggle(document, 24, patch.filter.velocitySwitch);

    writeOptional(document, 25, patch.amplifier.attack);
    writeOptional(document, 26, patch.amplifier.decay);
    writeOptional(document, 27, patch.amplifier.sustain);
    writeOptional(document, 28, patch.amplifier.release);
    writeOptional(document, 29, patch.amplifier.level);
    writeOptional(document, 30, patch.amplifier.velocitySensitivity);

    writeOptional(document, 31, encodeArpType(patch.arpeggiator.type));
    writeOptional(document, 32, patch.arpeggiator.range);
    writeOptional(document, 33, patch.arpeggiator.beat);
    writeOptional(document, 34, patch.arpeggiator.gate);
    writeToggle(document, 59, patch.arpeggiator.enabled);

    writeToggle(document, 65, patch.tempoDelay.enabled);
    writeOptional(document, 82, encodeDelayKind(patch.tempoDelay.type));
    writeOptional(document, 35, patch.tempoDelay.time);
    writeOptional(document, 83, patch.tempoDelay.spread);
    writeOptional(document, 36, patch.tempoDelay.feedback);
    writeOptional(document, 98, patch.tempoDelay.tone);
    writeOptional(document, 37, patch.tempoDelay.mix);

    writeToggle(document, 66, patch.chorusFlanger.enabled);
    writeOptional(document, 64, encodeChorusKind(patch.chorusFlanger.type));
    writeOptional(document, 52, patch.chorusFlanger.time);
    writeOptional(document, 53, patch.chorusFlanger.depth);
    writeOptional(document, 54, patch.chorusFlanger.rate);
    writeOptional(document, 55, patch.chorusFlanger.feedback);
    writeOptional(document, 56, patch.chorusFlanger.level);

    writeOptional(document, 60, patch.equalizerPan.tone);
    writeOptional(document, 61, patch.equalizerPan.freq);
    writeOptional(document, 62, patch.equalizerPan.level);
    writeOptional(document, 63, patch.equalizerPan.q);
    writeOptional(document, 90, patch.equalizerPan.pan);

    writeOptional(document, 38, encodeVoiceMode(patch.voice.playMode));
    writeOptional(document, 39, patch.voice.portamento);
    writeToggle(document, 73, patch.voice.unisonEnabled);
    writeToggle(document, 74, patch.voice.portamentoAuto);
    writeOptional(document, 75, patch.voice.unisonDetune);
    writeOptional(document, 84, patch.voice.spread);
    writeOptional(document, 85, patch.voice.unisonPitch);
    writeOptional(document, 92, patch.voice.unisonPhaseShift);
    writeOptional(document, 93, patch.voice.unisonVoices);
    writeOptional(document, 94, patch.voice.polyphony);

    writeOptional(document, 40, patch.wheelMidi.pitchBendRange);
    writeToggle(document, 77, patch.effect.enabled);
    writeOptional(document, 78, encodeEffectKind(patch.effect.type));
    exportEffectControls(document, patch.effect);

    writeOptional(document, 50, patch.wheelMidi.route1.sensitivity);
    writeOptional(document, 51, patch.wheelMidi.route2.sensitivity);
    writeOptional(document, 86, encodeMidiSource(patch.wheelMidi.route1.source));
    writeOptional(document, 87, encodeMidiDest(patch.wheelMidi.route1.destination));
    writeOptional(document, 88, encodeMidiSource(patch.wheelMidi.route2.source));
    writeOptional(document, 89, encodeMidiDest(patch.wheelMidi.route2.destination));

    writeToggle(document, 57, patch.lfo.lfo1.enabled);
    writeOptional(document, 41, encodeLfoDest(patch.lfo.lfo1.destination));
    writeOptional(document, 42, encodeLfoWave(patch.lfo.lfo1.wave));
    writeOptional(document, 43, patch.lfo.lfo1.rate);
    writeOptional(document, 44, patch.lfo.lfo1.amount);
    writeToggle(document, 67, patch.lfo.lfo1.tempoSync);
    writeToggle(document, 68, patch.lfo.lfo1.keySync);

    writeToggle(document, 58, patch.lfo.lfo2.enabled);
    writeOptional(document, 46, encodeLfoDest(patch.lfo.lfo2.destination));
    writeOptional(document, 47, encodeLfoWave(patch.lfo.lfo2.wave));
    writeOptional(document, 48, patch.lfo.lfo2.rate);
    writeOptional(document, 49, patch.lfo.lfo2.amount);
    writeToggle(document, 69, patch.lfo.lfo2.tempoSync);
    writeToggle(document, 70, patch.lfo.lfo2.keySync);
    return document;
}

std::string formatSynth1PatchText(const Synth1Patch& patch) {
    return formatSynth1PatchText(exportSynth1Patch(patch));
}
Synth1Patch decodeNativePatchControls(const synth1::patch::NativePatchControls& resolved,
    const Synth1PatchDocument& document, Synth1PatchImportWorkspace& workspace) {
    auto normalized=Synth1PatchDocument::makeParsed(document.name,113,{});
    normalized.entries.swap(workspace.normalizedEntries);
    normalized.entries.clear();
    for(int index=0;index<99;++index)normalized.entries.push_back({index,resolved.values[index]});
    auto patch=decodePatchControls(normalized,document);
    normalized.entries.swap(workspace.normalizedEntries);
    patch.oscillators.ring=resolved.values[7]!=0;
    return patch;
}
