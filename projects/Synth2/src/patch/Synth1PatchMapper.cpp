#include "../dsp/FractionalDial.h"
#include "../dsp/CompatibleControlCurve.h"
#include "Synth1PatchMapper.h"
#include "../render/RenderPolicy.h"
#include "../render/FilterParameterMapping.h"
#include "../dsp/MidiRoutingHistory.h"
#include "Synth1SourceRange.h"
#include "Synth1PresetControls.h"
#include "Synth1ControlPromotion.h"
#include "Synth1Tuning.h"
#include "Synth1EnvelopeTime.h"
#include <Synth1PatchIo.h>
#include <Synth1NativeLoading.h>
#include <Synth1PatchNativeControls.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace synth2
{
    namespace
    {
        template<class Curve> double envelopeTime(double dial,Curve curve)
        {
            const int low=static_cast<int>(dial);
            const double first=curve(low),second=curve(std::min(127,low+1));
            // Compatible timing knots define frame boundaries. Their exponential
            // spacing remains exponential between knots, with double progress.
            return first*std::pow(second/first,dial-low);
        }

        double unit (std::optional<int> value, int fallback = 0)
        {
            return std::clamp (value.value_or (fallback), 0, 127) / 127.0;
        }

        double wave (std::optional<Synth1OscillatorWaveform> shape,
                    Synth1OscillatorWaveform fallback)
        {
            switch (shape.value_or (fallback))
            {
                case Synth1OscillatorWaveform::saw: return 0.0;
                case Synth1OscillatorWaveform::pulse: return 1.0 / 3.0;
                case Synth1OscillatorWaveform::triangle: return 2.0 / 3.0;
                case Synth1OscillatorWaveform::sine: return 1.0;
                case Synth1OscillatorWaveform::noise: return 4.0 / 3.0;
            }
            return 0.0;
        }

        LfoSettings lfo (const Synth1LfoPatch& patch)
        {
            LfoSettings out;
            out.on = patch.enabled.value_or (false);
            // Native source/getter destination0 reaches the ordinary setter's
            // lower bound1 (OSC2), not the CLFO destination0/no-receiver path.
            // Keep the raw/resolved source0 separate from this receiver choice.
            // See docs/lfo-zero-loading.md for stock/observer donor captures.
            out.dest = static_cast<int> (patch.destination.value_or (Synth1LfoDestination::osc2)) - 1;
            // Synth1 and the current DSP use different waveform enumerations.
            switch (patch.wave.value_or (Synth1LfoWaveform::sine))
            {
                case Synth1LfoWaveform::sine: out.wave = 0; break;
                case Synth1LfoWaveform::triangle: out.wave = 1; break;
                case Synth1LfoWaveform::saw: out.wave = 2; break;
                case Synth1LfoWaveform::square: out.wave = 3; break;
                case Synth1LfoWaveform::randomSampleHold: out.wave = 4; break;
                case Synth1LfoWaveform::randomSmoothed: out.wave = 5; break;
            }
            out.speed = unit (patch.rate, 64);
            out.amount = unit (patch.amount);
            out.tempoSync = patch.tempoSync.value_or (false);
            out.keySync = patch.keySync.value_or (false);
            return out;
        }
    }

    void Synth1PatchMapper::mapParameterValues (const std::array<std::atomic<float>*,126>& values,
                                            RenderControls& target)
    {
        std::array<float, 128> raw {};
        bool changed = mappedRandomHistoryValid != loadHistory.nativeRandomLfoHistoryValid || ! hasMapped || mappedTarget != &target
            || mappedMissingEnvelopeFields != missingEnvelopeFields
            || mappedNativeSourceLoading != nativeSourceLoading
            || (nativeSourceLoading && (mappedNativePresence != nativeSourcePresence
                || mappedNativeInitial != nativeInitialControls || mappedNativeEdits != nativeRuntimeEdits));
        for (int i = 0; i < 126; ++i)
        {
            if (auto* p = values[static_cast<size_t>(i)])
                raw[i] = p->load() / 127.0f;
            changed = changed || raw[i] != mappedSourceValues[i];
        }
        if (changed) mapFromRawBuffer (raw.data(), target);
    }

    void Synth1PatchMapper::mapFromRawBuffer (const float* raw, RenderControls& target)
    {
        const bool native=directControls || (nativeSourceLoading && synth1::patch::isNativeSourceVersion(sourceVersion));
        auto source=Synth1PatchDocument::makeParsed("Synth2",directControls ? 113 : sourceVersion,{});
        source.entries.swap(mappingEntries);
        source.entries.clear();
        for (int i = 0; i < 126; ++i)
            if(directControls || !native || (nativeSourcePresence[i/64] & (uint64_t{1}<<(i%64)))!=0)
                source.entries.push_back ({i, std::clamp (static_cast<int> (std::lround (raw[i] * 127.0f)), sourceMinimum(i), sourceMaximum(i))});
        synth1::patch::NativePatchControls resolved;
        if(native)
        {
            resolved.sourceVersion=directControls ? 113 : sourceVersion;resolved.values=nativeInitialControls;
            for(int i=0;i<99;++i)
                if(directControls || (nativeRuntimeEdits[i/64]&(uint64_t{1}<<(i%64)))!=0)
                    resolved.values[i]=std::clamp(static_cast<int>(std::lround(raw[i]*127.0f)),sourceMinimum(i),sourceMaximum(i));
        }
        auto patch=native ? decodeNativePatchControls(resolved,source,importWorkspace) : importSynth1Patch(source,importWorkspace);
        source.entries.swap(mappingEntries);
        std::array<double,128> effective{};
        for(size_t i=0;i<effective.size();++i)effective[i]=promoteNormalizedSource(raw[i]);
        if(native)
        {
            resolvedNativeControls=resolved.values;
            const auto promoted=promoteCompatibleControls(resolved);
            for(int i=0;i<99;++i)effective[i]=promoted.normalized(i);
            if(continuousControls)
                for(int i=0;i<99;++i)
                    if(directControls || (nativeRuntimeEdits[i/64]&(uint64_t{1}<<(i%64)))!=0)
                        effective[i]=promoteNormalizedSource(raw[i]);
        }
        const auto* controls=effective.data();
        loadHistory.nativeMidiRoutingLoadHistory=native;
        loadHistory.nativeMidiRoutingControls=native ? std::array<int,4>{resolved.values[86],resolved.values[87],resolved.values[88],resolved.values[89]}
                                                : std::array<int,4>{};
        mappedRandomHistoryValid=loadHistory.nativeRandomLfoHistoryValid;
        loadHistory.controllerRoutes={};loadHistory.controllerResetOnLoad={true,true};
        if(native)
            for(size_t i=0;i<2;++i)
            {
                loadHistory.controllerResetOnLoad[i]=dsp::MidiRoutingHistory::validSource(resolved.values[86+i*2]);
                if(loadHistory.controllerResetOnLoad[i])controllerSources[i]=resolved.values[86+i*2];
                if(dsp::MidiRoutingHistory::validDestination(resolved.values[87+i*2]))controllerDestinations[i]=resolved.values[87+i*2];
                const int dest=controllerDestinations[i];
                const double dial=dsp::fractionalDial(controls[50+i]);
                loadHistory.controllerRoutes[i]={controllerSources[i],dest,
                    dest>=0 && dest<99 ? resolved.values[dest] : 0,0,dest==35 ? 19 : dest==85 ? 48 : 127,
                    static_cast<float>((dial<63 ? dial-63 : dial>64 ? dial-64 : 0.0)*(1.0/63.0))};
            }
        mappedNativeSourceLoading=nativeSourceLoading;
        mappedNativePresence=nativeSourcePresence;
        mappedNativeInitial=nativeInitialControls;
        mappedNativeEdits=nativeRuntimeEdits;
        // Resolve only audited omitted fields; all other APVTS defaults retain
        // their historical behavior. Never encode p21=128 in a host parameter.
        if (!native && sourceVersion >= 104 && sourceVersion <= 113)
        {
            auto applyMissing = [&] (int index, std::optional<int>& value)
            {
                if ((missingEnvelopeFields & synth1::patch::nativeEnvelopePresenceBit (index)) != 0)
                    value = synth1::patch::nativeMissingEnvelopeDial (sourceVersion, index);
            };
            applyMissing (13, patch.oscillators.modEnvDecay);
            applyMissing (16, patch.filter.decay);
            applyMissing (18, patch.filter.release);
            applyMissing (21, patch.filter.envelopeAmount);
            applyMissing (26, patch.amplifier.decay);
            applyMissing (28, patch.amplifier.release);
        }
        mappedMissingEnvelopeFields = missingEnvelopeFields;
        mappedTarget = &target;
        hasMapped = true;
        if (raw != mappedSourceValues.data())
            std::copy_n (raw, 128, mappedSourceValues.begin());
        target.osc1WaveIndex = wave (patch.oscillators.osc1.shape, Synth1OscillatorWaveform::sine);
        target.osc2WaveIndex = wave (patch.oscillators.osc2.shape, Synth1OscillatorWaveform::saw);
        target.oscMix = unit (patch.oscillators.mix);
        target.unisonPhaseDial = std::clamp (patch.voice.unisonPhaseShift.value_or (0),0,127);
        target.oscillatorPhaseDial = std::clamp (patch.oscillators.phaseShift.value_or (0),0,127);
        target.osc1StackDial = std::clamp(patch.oscillators.osc1.detune.value_or(0),0,127);
        target.osc1FmAmount = unit (patch.oscillators.fmAmount);
        target.osc2PitchSemis = static_cast<double> (coarseTuneSemitones (patch.oscillators.osc2.coarseTune.value_or (64)));
        target.osc2FineCents = fineTuneCents (patch.oscillators.osc2.fineTune.value_or (64));
        // The original DLL identifies serialized p72 as OSC1/2 Fine Tune.
        target.globalFineTune = fineTuneCents (patch.oscillators.reserved72.value_or (64));
        target.osc2KbdTrack = patch.oscillators.osc2KeyTrack.value_or (true) ? 1.0 : 0.0;
        // Preserve the setter's float reciprocal multiply before pulse-width
        // modulation and integer table-bin truncation.
        target.pulseWidth = static_cast<double>(std::clamp(patch.oscillators.pulseWidth.value_or(64),0,127))/127.0;
        target.osc2Sync = patch.oscillators.sync.value_or (false) ? 1.0 : 0.0;
        target.ringMod = patch.oscillators.ring.value_or (false) ? 1.0 : 0.0;
        target.octaveBase = 0;
        target.keyShift = patch.oscillators.keyShift.value_or (0);
        target.modEnv.on = patch.oscillators.modEnvEnabled.value_or (false);
        const auto dest = patch.oscillators.modEnvDestination.value_or (Synth1OscillatorModDestination::osc2);
        target.modEnv.dest = dest == Synth1OscillatorModDestination::fm ? 2 : dest == Synth1OscillatorModDestination::pulseWidth ? 3 : 0;

        target.filterType = static_cast<int> (patch.filter.type.value_or (Synth1FilterType::lp12));
        target.cutoffIndex = unit (patch.filter.freq, 127) * 1023.0;
        target.resonanceNorm = unit (patch.filter.resonance);
        // Native display/response is centred at raw 63 (64 displays +1).
        // The file/getter context can retain128, but the ordinary receiver
        // clamps the dial to127 before deriving the DSP depth.
        target.filterEnvAmount = (std::clamp(patch.filter.envelopeAmount.value_or (63),0,127) - 63.0) / 64.0;
        // Independent fixed-OSC2 captures fit a logarithmic tracking-depth knob.
        target.filterKeyTrackNorm = unit (patch.filter.keyTrack);
        target.filterSaturation = unit (patch.filter.saturation);
        target.filterVelocity = patch.filter.velocitySwitch.value_or (false) ? 1.0 : 0.0;
        target.filterEnv = {envelopeAttackSeconds (patch.filter.attack.value_or (0)),
                            envelopeDecayReleaseSeconds (patch.filter.decay.value_or (0)),
                            unit (patch.filter.sustain, 127),
                            envelopeDecayReleaseSeconds (patch.filter.release.value_or (0))};
        target.ampAttackSec = envelopeAttackSeconds (patch.amplifier.attack.value_or (0));
        target.ampDecaySec = envelopeDecayReleaseSeconds (patch.amplifier.decay.value_or (0));
        target.ampSustain = ampSustainLevel (patch.amplifier.sustain.value_or (127));
        target.ampReleaseSec = envelopeDecayReleaseSeconds (patch.amplifier.release.value_or (0));
        target.ampGainMultiplier = unit (patch.amplifier.level, 100);
        target.ampVelocity = unit (patch.amplifier.velocitySensitivity);
        target.pitchBendRange = static_cast<double> (std::clamp (patch.wheelMidi.pitchBendRange.value_or (12), 0, 24));
        target.arpeggiator.enabled = patch.arpeggiator.enabled.value_or (false);
        target.arpeggiator.type = static_cast<int> (patch.arpeggiator.type.value_or (Synth1ArpeggiatorType::up));
        target.arpeggiator.range = std::clamp (patch.arpeggiator.range.value_or (0), 0, 3);
        target.arpeggiator.beat = std::clamp (patch.arpeggiator.beat.value_or (11), 0, 18);
        target.arpeggiator.gate = std::clamp (patch.arpeggiator.gate.value_or (64), 0, 127);
        target.playMode = static_cast<int> (patch.voice.playMode.value_or (Synth1VoicePlayMode::poly));
        target.isPolyphonic = target.playMode == 0;
        const int polyphony = patch.voice.polyphony.value_or (0);
        target.polyphony = polyphony > 0 ? std::clamp (polyphony, 1, 32) : 32;
        target.portamentoAuto = patch.voice.portamentoAuto.value_or (false);
        target.sourcePortamentoDial = patch.voice.portamento.value_or(0);
        target.lfo1 = lfo (patch.lfo.lfo1);
        target.lfo2 = lfo (patch.lfo.lfo2);
        // Route admission must also see sub levels below half an integer dial.
        target.subOscGain = continuousControls ? std::clamp(controls[95],0.0,1.0)
                                              : unit (patch.oscillators.subOscLevel);
        target.subOscWave = static_cast<int> (std::lround (wave (patch.oscillators.subOscShape, Synth1OscillatorWaveform::sine) * 3.0));
        target.subOscOctave = patch.oscillators.subOscOctave.value_or (Synth1SubOscillatorOctave::same) == Synth1SubOscillatorOctave::same ? 0 : -1;
        target.unisonVoices = patch.voice.unisonEnabled.value_or (false) ? std::clamp (patch.voice.unisonVoices.value_or (2), 1, 8) : 1;
        target.unisonDetune = unit (patch.voice.unisonDetune);
        // Source/getter retains 0..127, but native dispatch clamps p85 to 0..48.
        target.unisonPitchDial = std::clamp(patch.voice.unisonPitch.value_or(24),0,48);
        target.unisonPan = (patch.voice.spread.value_or (64) - 64.0) * (1.0/64.0);
        const int pan = std::clamp (patch.equalizerPan.pan.value_or (64), 0, 127);
        target.pan = (pan - 64.0) / (pan >= 64 ? 63.0 : 64.0);
        target.delay.enable = patch.tempoDelay.enabled.value_or (false);
        target.delay.time = unit (patch.tempoDelay.time);
        target.delay.feedback = unit (patch.tempoDelay.feedback);
        target.delay.level = unit (patch.tempoDelay.mix);
        target.delay.type = static_cast<int> (patch.tempoDelay.type.value_or (Synth1DelayType::stereo));
        target.delay.spread = unit (patch.tempoDelay.spread, 66);
        target.delay.tone = unit (patch.tempoDelay.tone, 64);
        if (!native && sourceVersion >= 104 && sourceVersion <= 113)
        {
            auto dial = [raw] (int index) { return std::clamp (static_cast<int> (std::lround (raw[index] * 127.0)),0,127); };
            const auto fields = nativePresetVoiceFields (sourceVersion,target.playMode,
                dial(75),dial(93),dial(94),dial(83),dial(98));
            target.unisonDetune = fields.detune / 127.0;
            target.unisonVoices = dial(73) != 0 ? fields.voices : 1;
            target.portamentoAuto = dial(74) != 0;
            target.polyphony = fields.polyphony;
            target.delay.spread = fields.delaySpread / 127.0;
            target.delay.tone = fields.delayTone / 127.0;
        }
        target.chorus.type = static_cast<int>(patch.chorusFlanger.type.value_or(Synth1ChorusType::x2));
        if(native || (sourceVersion >= 104 && sourceVersion <= 113))
        {
            // Getter/source p64 is retained verbatim. Native DSP dispatch first
            // clamps it to1..4; the Chorus setter then maps the invalid3 to2.
            const int voices=std::clamp(static_cast<int>(std::lround(controls[64]*127.0)),1,4);
            target.chorus.type=voices==3 ? 2 : voices;
        }
        target.chorus.enabled = patch.chorusFlanger.enabled.value_or (false);
        target.chorus.time = native || (sourceVersion >= 104 && sourceVersion <= 113)
            ? unit (patch.chorusFlanger.time, 64)
            : std::clamp (static_cast<int> (std::lround (raw[52] * 127.0)), 0, 127) / 127.0; // Preserve the prior unaudited source-version route.
        target.chorus.depth = unit (patch.chorusFlanger.depth);
        target.chorus.rate = unit (patch.chorusFlanger.rate);
        target.chorus.feedback = unit (patch.chorusFlanger.feedback, 64);
        target.chorus.level = unit (patch.chorusFlanger.level);
        // Effect type IDs differ from this engine's experimental algorithms.
        // The recovered native children below bypass the experimental adapter.
        const int effectType=static_cast<int>(patch.effect.type.value_or(Synth1EffectType::overdrive1));
        target.nativeMultiEffectDials={dsp::fractionalDial(controls[79]),dsp::fractionalDial(controls[80]),dsp::fractionalDial(controls[81])};
        target.nativePhaserDepth=dsp::fractionalDial(controls[79]);
        target.nativePhaserRate=dsp::fractionalDial(controls[80]);
        target.nativePhaserFeedback=dsp::fractionalDial(controls[81]);
        target.eq.tone = unit (patch.equalizerPan.tone, 64);
        target.eq.freq = unit (patch.equalizerPan.freq, 64);
        target.eq.level = unit (patch.equalizerPan.level, 64);
        target.eq.q = unit (patch.equalizerPan.q, 64);
        render::RenderPolicyInputs policy;
        policy.effectControlActive=controls[77]!=0;
        policy.stackControlActive=controls[76]!=0;
        policy.effectEnabled=patch.effect.enabled.value_or(false);
        policy.effectType=effectType;
        policy.stackDial=patch.oscillators.osc1.detune.value_or(0);
        policy.subLevelDial=patch.oscillators.subOscLevel.value_or(0);
        policy.modulationAmountDial=patch.oscillators.modEnvAmount.value_or(64);
        policy.modulationAttackDial=patch.oscillators.modEnvAttack.value_or(0);
        policy.modulationDecayDial=patch.oscillators.modEnvDecay.value_or(0);
        render::selectRenderPolicy(policy,target);

        target.filterCutoffAdjustment=0;
        if(continuousControls)
        {
            const auto dial=[controls](int i) { return dsp::fractionalDial(controls[i]); };
            const auto normalized=[&](int i) { return dial(i)/127.0; };
            const auto attack=[&](int i) { return envelopeTime(dial(i),envelopeAttackSeconds); };
            const auto decay=[&](int i) { return envelopeTime(dial(i),envelopeDecayReleaseSeconds); };
            target.osc2FineCents=fineTuneCents(dsp::fractionalDial(controls[3]));
            target.globalFineTune=fineTuneCents(dsp::fractionalDial(controls[72]));
            target.oscMix=normalized(5);
            target.osc1FmAmount=normalized(45);
            target.pulseWidth=normalized(8);
            target.modEnv.amountDial=dsp::fractionalDial(controls[11]);
            // Legacy family conversion has already expanded its integer dials.
            // Only resolved modern controls carry fractional timing coordinates.
            if(native)
            {
                target.modEnv.attackDial=dsp::fractionalDial(controls[12]);
                target.modEnv.decayDial=dsp::fractionalDial(controls[13]);
            }
            for(int index:{19,21,22})render::applyFilterParameter(target,index,controls[index],native);
            target.resonanceNorm=normalized(20);
            target.filterSaturation=dsp::fractionalDial(controls[23])/127.0;
            target.filterEnv={attack(15),decay(16),normalized(17),decay(18)};
            target.ampAttackSec=attack(25);
            target.ampDecaySec=decay(26);
            const double sustain=dial(27);
            const int sustainLow=static_cast<int>(std::floor(sustain));
            const double lowGain=ampSustainLevel(sustainLow);
            target.ampSustain=lowGain+(ampSustainLevel(std::min(127,sustainLow+1))-lowGain)*(sustain-sustainLow);
            target.ampReleaseSec=decay(28);
            target.ampGainMultiplier=dsp::fractionalDial(controls[29])/127.0;
            target.ampVelocity=dsp::fractionalDial(controls[30])/127.0;
            target.sourcePortamentoDial=dsp::fractionalDial(controls[39]);
            target.lfo1.speed=normalized(43);
            target.lfo1.amount=normalized(44);
            target.lfo2.speed=normalized(48);
            target.lfo2.amount=normalized(49);
            target.subOscGain=(dial(95)*4.0)/127.0;
            target.unisonDetune=normalized(75);
            if(native)target.unisonPhaseDial=dsp::fractionalDial(controls[92]);
            if(native)
            {
                target.oscillatorPhaseDial=dsp::fractionalDial(controls[91]);
                // Retain the control-based stack admission selected above.
                if(target.osc1StackDial>0)target.osc1StackDial=dsp::fractionalDial(controls[76]);
            }
            target.unisonPan=(dial(84)-64.0)/64.0;
            const double pan=dial(90);
            target.pan=(pan-64.0)/(pan>=64.0 ? 63.0 : 64.0);
            target.delay.time=normalized(35);
            target.delay.feedback=normalized(36);
            target.delay.level=normalized(37);
            target.delay.spread=normalized(83);
            target.delay.tone=normalized(98);
            target.chorus.time=normalized(52);
            target.chorus.depth=normalized(53);
            target.chorus.rate=normalized(54);
            target.chorus.feedback=normalized(55);
            target.chorus.level=normalized(56);
            target.eq.tone=normalized(60);
            target.eq.freq=normalized(61);
            target.eq.level=normalized(62);
            target.eq.q=normalized(63);
        }

    }
}
