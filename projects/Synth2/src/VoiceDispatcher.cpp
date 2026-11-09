#include "VoiceDispatcher.h"
#include "SynthVoice.h"
#include "VoiceCorePool.h"
#include "RenderControls.h"
#include "dsp/SharedModulationRuntime.h"
#include "dsp/OscillatorTuning.h"
#include "render/ControllerMapping.h"
#include "render/LfoViews.h"
#include "render/SharedModulationConfiguration.h"
#include <algorithm>
#include <optional>

namespace synth2
{
    namespace
    {
        int logicalVoiceLimit (const RenderControls* params, int available)
        {
            if (params == nullptr) return available;
            const int physical = std::clamp (params->polyphony, 1, 32);
            return std::min (available, physical);
        }
    }

    void VoiceDispatcher::trackLfoKey(const ::synth2::audio::MidiMessage& message)
    {
        const int channel=message.getChannel();
        if(channel>=1 && channel<=16)
        {
            const size_t c=static_cast<size_t>(channel-1);
            if(message.isAllNotesOff() || message.isAllSoundOff())
            {inputKeys[c].fill(false);inputSustain[c]=false;}
            if(message.isController() && message.getControllerNumber()==64)
            {
                inputSustain[c]=message.getControllerValue()>=64;
                if(!inputSustain[c] && modulation!=nullptr)
                    for(int key=0;key<128;++key)if(!inputKeys[c][key])modulation->releaseKey(channel,key);
                return;
            }
            if(message.isNoteOnOrOff())
            {
                inputKeys[c][message.getNoteNumber()]=message.isNoteOn();
                if(message.isNoteOff() && inputSustain[c])return;
            }
        }
        if(modulation!=nullptr && params!=nullptr)
            modulation->trackKey(message,render::sharedModulation(*params));
    }

    void VoiceDispatcher::noteOn (int midiChannel, int midiNoteNumber, float velocity)
    {
        if (!dispatchingArpeggiator && midiChannel>=1 && midiChannel<=16 && midiNoteNumber>=0 && midiNoteNumber<128)
            trackLfoKey(::synth2::audio::MidiMessage::noteOn(midiChannel,midiNoteNumber,velocity));
        if (params == nullptr || params->playMode == 0)
        {
            if(nativeOmniInput && midiChannel>=1 && midiChannel<=16 && midiNoteNumber>=0 && midiNoteNumber<128)
            {
                const size_t c=static_cast<size_t>(midiChannel-1);
                physicalKeys[c][midiNoteNumber]=true;heldOrder[c][midiNoteNumber]=++noteOrder;heldVelocity[c][midiNoteNumber]=velocity;
            }
            ::synth2::audio::VoiceBank::noteOn (midiChannel, midiNoteNumber, velocity);
            return;
        }
        if (midiChannel < 1 || midiChannel > 16 || midiNoteNumber < 0 || midiNoteNumber > 127) return;
        const size_t channel = static_cast<size_t> (midiChannel - 1);
        physicalKeys[channel][midiNoteNumber]=true;
        bool overlapping = false;
        for (auto order : heldOrder[channel]) overlapping = overlapping || order != 0;
        heldOrder[channel][midiNoteNumber] = ++noteOrder;
        heldVelocity[channel][midiNoteNumber] = velocity;
        auto* voice = nativeOmniInput && corePool!=nullptr ? corePool->primaryOwner() : monoVoices[channel];
        const bool continuing = voice != nullptr && voice->isVoiceActive() && voice->isPlayingChannel (midiChannel);
        if (! continuing)
            voice = dynamic_cast<SynthVoice*> (findFreeVoice (getSound(0), midiChannel, midiNoteNumber, true));
        if (voice == nullptr) return;
        const auto transition=continuing && overlapping && params->playMode==2 ? dsp::NoteTransitionKind::Legato
            : continuing ? dsp::NoteTransitionKind::RetainedRetrigger
            : dsp::NoteTransitionKind::Retrigger;
        voice->setNextTransition({transition,params->sourcePortamentoDial>0.0f && (!params->portamentoAuto || overlapping)});
        startVoice (voice, getSound(0), midiChannel, midiNoteNumber, velocity);
        monoVoices[channel] = voice;
    }

    void VoiceDispatcher::noteOff (int channel, int note, float velocity, bool tail)
    {
        if (!dispatchingArpeggiator && channel >= 1 && channel <= 16 && note >= 0 && note < 128)
            trackLfoKey(::synth2::audio::MidiMessage::noteOff(channel,note));
        if (params == nullptr || params->playMode == 0 || channel < 1 || channel > 16 || note < 0 || note > 127)
        {
            if(nativeOmniInput && channel>=1 && channel<=16 && note>=0 && note<128)
            {
                const size_t c=static_cast<size_t>(channel-1);physicalKeys[c][note]=false;
                if(!sustainDown[c])heldOrder[c][note]=0;
            }
            ::synth2::audio::VoiceBank::noteOff (channel, note, velocity, tail);
            return;
        }
        const size_t c = static_cast<size_t> (channel - 1);
        physicalKeys[c][note]=false;
        if(sustainDown[c])
        {
            ::synth2::audio::VoiceBank::noteOff(channel,note,velocity,tail);return;
        }
        heldOrder[c][note] = 0;
        auto* voice = monoVoices[c];
        if (voice != nullptr && voice->isVoiceActive() && voice->isPlayingChannel (channel)
            && voice->getCurrentlyPlayingNote() == note)
        {
            int previous = -1;
            uint64_t newest = 0;
            for (int key = 0; key < 128; ++key)
                if (heldOrder[c][key] > newest) { newest = heldOrder[c][key]; previous = key; }
            if (previous >= 0)
            {
                const auto transition=params->playMode==2 ? dsp::NoteTransitionKind::Legato
                    : dsp::NoteTransitionKind::RetainedRetrigger;
                voice->setNextTransition({transition,params->sourcePortamentoDial>0.0f});
                startVoice (voice, getSound(0), channel, previous, heldVelocity[c][previous]);
                return;
            }
        }
        ::synth2::audio::VoiceBank::noteOff (channel, note, velocity, tail);
    }

    void VoiceDispatcher::handleSustainPedal(int channel,bool down)
    {
        if(!dispatchingArpeggiator && channel>=1 && channel<=16)
            trackLfoKey(::synth2::audio::MidiMessage::controllerEvent(channel,64,down ? 127 : 0));
        if(channel>0 && channel<=16)
        {
            const size_t c=static_cast<size_t>(channel-1);sustainDown[c]=down;
            if(!down && params!=nullptr && params->playMode!=0)
            {
                for(int key=0;key<128;++key)if(!physicalKeys[c][key])heldOrder[c][key]=0;
                auto* voice=monoVoices[c];
                if(voice!=nullptr && voice->isVoiceActive() && voice->getCurrentlyPlayingNote()>=0
                    && !physicalKeys[c][voice->getCurrentlyPlayingNote()])
                    noteOff(channel,voice->getCurrentlyPlayingNote(),0,true);
            }
        }
        ::synth2::audio::VoiceBank::handleSustainPedal(channel,down);
    }

    void VoiceDispatcher::handlePitchWheel(int channel,int value)
    {
        if(channel>=1 && channel<=16)pitchBends[static_cast<size_t>(channel-1)].set(value);
        if(channel==1)applyControllerMessage(224,value&127,value>>7);
        ::synth2::audio::VoiceBank::handlePitchWheel(channel,value);
    }

    void VoiceDispatcher::applyControllerMessage(int status,int first,int second)
    {
        if(params!=nullptr && controllerSettings!=nullptr)
            controllers.message(status,first,second,*controllerSettings,[&](int destination,int dial) {
                render::applyControllerDial(*params,destination,dial);
                if(destination==2)
                {
                    const float exponent=dsp::OscillatorTuning::coarseExponent(params->osc2PitchSemis);
                    if(corePool!=nullptr)corePool->setOsc2Coarse(exponent,false);
                }
                if(corePool!=nullptr)corePool->setControllerDial(destination,static_cast<int>(getSampleRate()));
                if(modulation!=nullptr)
                {
                    if(destination==44 || destination==49)
                        modulation->applyHistory({4,destination,dial},params->hostBpm);
                    if(destination==43)modulation->touchRate(0);
                    if(destination==48)modulation->touchRate(1);
                    modulation->synchronize(render::sharedModulation(*params),getSampleRate());
                }
            });
    }

    void VoiceDispatcher::handleController(int channel,int number,int value)
    {
        // Native CC121 centers the ordinary bend receiver; routed MIDI
        // inputs and the sustain latch have independent lifetimes.
        if(channel>=1 && channel<=16 && number==121)
        {
            pitchBends[static_cast<size_t>(channel-1)].set(8192);
            ::synth2::audio::VoiceBank::handlePitchWheel(channel,8192);
        }
        if(channel==1 && number==1)applyControllerMessage(176,number,value);
        ::synth2::audio::VoiceBank::handleController(channel,number,value);
    }

    void VoiceDispatcher::setPlaybackRate (double rate, bool preserveNotes)
    {
        const ::synth2::audio::ScopedValueSetter<bool> preserving (preservingNotesForRateChange, preserveNotes);
        setCurrentPlaybackSampleRate (rate);
    }

    void VoiceDispatcher::allNotesOff (int channel, bool tail)
    {
        // The voice bank calls this from its sample-rate setter. Native hot
        // reprepare retains notes; ordinary MIDI/all-notes-off still stops them.
        if (preservingNotesForRateChange) return;
        if(!dispatchingArpeggiator && modulation!=nullptr)modulation->releaseKeys(channel);
        for (int c = 0; c < 16; ++c)
            if (channel == 0 || channel == c + 1)
            {
                heldOrder[c].fill (0); heldVelocity[c].fill (0.0f); monoVoices[c] = nullptr;
                physicalKeys[c].fill(false);sustainDown[c]=false;
                if(!dispatchingArpeggiator){inputKeys[c].fill(false);inputSustain[c]=false;}
            }
        ::synth2::audio::VoiceBank::allNotesOff (channel, tail);
    }

    ::synth2::audio::Voice* VoiceDispatcher::findFreeVoice (::synth2::audio::Sound* sound,
                                                                  int, int, bool steal) const
    {
        const int count = corePool != nullptr && params != nullptr && params->playMode == 0
            ? getNumVoices() : logicalVoiceLimit (params, getNumVoices());
        ::synth2::audio::Voice* oldest = nullptr;
        int oldestIndex = -1;
        // DLL RVA0x36f30 searches cyclically after the previous polyphonic
        // slot, preserving the pending/random state of unused voices.
        // Physical core ownership is assigned separately when the note starts.
        const bool rotate = params != nullptr && params->playMode == 0;
        const int first = rotate && count > 0 ? (lastAllocatedVoice + 1) % count : 0;
        for (int offset = 0; offset < count; ++offset)
        {
            const int i = (first + offset) % count;
            auto* voice = getVoice (i);
            if (! voice->canPlaySound (sound)) continue;
            if (! voice->isVoiceActive())
            {
                lastAllocatedVoice = i;
                return voice;
            }
            if (oldest == nullptr || voice->wasStartedBefore (*oldest)) { oldest = voice; oldestIndex = i; }
        }
        if (steal) lastAllocatedVoice = oldestIndex;
        return steal ? oldest : nullptr;
    }

    void VoiceDispatcher::renderVoicesWithLfos (::synth2::audio::AudioBuffer<float>& output, int start, int count,bool completeGainDispatch)
    {
        bool receiverBoundary=completeGainDispatch;
        if(params!=nullptr)
        {
            const auto settings=render::sharedModulation(*params);
            // Recompute the32-sample bend horizon after every root boundary.
            // A shorter LFO span must not leave a residual bend subspan.
            const bool movingBend=std::any_of(pitchBends.begin(),pitchBends.end(),[](const auto& bend){return bend.moving();});
            receiverBoundary=receiverBoundary || movingBend;
            const int bendSpan=movingBend ? std::min(count,32) : count;
            const int span=modulation!=nullptr ? modulation->randomBoundary(bendSpan,settings) : bendSpan;
            if(span>0 && span<count)
            {
                renderVoicesWithLfos(output,start,span,true);renderVoicesWithLfos(output,start+span,count-span,completeGainDispatch);return;
            }
            for(auto& bend:pitchBends)if(bend.moving())bend.advance(static_cast<int>(params->pitchBendRange));
            if(modulation!=nullptr)modulation->renderSpan(start,count,settings);
        }
        // Gain interpolation follows its own grid; shared clocks keep their
        // existing dispatches. Known events bound every predicted gain span.
        int gainHorizon=0,gainBoundary=0;
        if(params!=nullptr && params->arpeggiator.enabled && modulation!=nullptr)
        {
            const int offset=start-modulation->block().startSample;
            const auto frame=params->globalSampleCount+static_cast<uint64_t>(offset);
            gainHorizon=64-static_cast<int>(frame%64);
            const int next=arpeggiator.framesUntilBoundary(static_cast<double>(frame),count+64);
            gainBoundary=receiverBoundary ? std::min(count,next) : next<count+64 ? next : 0;
            if(gainBoundary>0)gainHorizon=std::min(gainHorizon,gainBoundary);
        }
        bool renderedSignal=false;
        for(int i=0;i<getNumVoices();++i)
        {
            if(auto* voice=dynamic_cast<SynthVoice*>(getVoice(i)))
            {
                int channel=1;
                for(int candidate=1;candidate<=16;++candidate)if(voice->isPlayingChannel(candidate)){channel=candidate;break;}
                voice->setPitchBendSource(params!=nullptr ? &pitchBends[static_cast<size_t>(channel-1)] : nullptr);
                voice->setRenderWorkspaceOrigin(nativeWorkspaceOrigin);
                voice->setAmplitudeGainHorizon(gainHorizon,gainBoundary);
                voice->beginOutputObservation();
            }
            else renderedSignal=renderedSignal || getVoice(i)->isVoiceActive();
        }
        if(nativeOmniInput && params!=nullptr && params->playMode!=0 && corePool!=nullptr)
        {
            // Native mono admission renders the first physical group while
            // higher polyphonic cores retain their paused receiver history.
            if(auto* primary=corePool->primaryOwner())primary->renderNextBlock(output,start,count);
        }
        else renderVoices(output,start,count);
        for(int i=0;i<getNumVoices();++i)
            if(auto* voice=dynamic_cast<SynthVoice*>(getVoice(i)))
                renderedSignal=renderedSignal || voice->didRenderSignal();
        renderedVoicesInRegion=renderedVoicesInRegion || renderedSignal;
        if(activeOutputSink)activeOutputSink(activeOutputContext,start,count,!renderedSignal);
    }

    void VoiceDispatcher::renderRegion (::synth2::audio::AudioBuffer<float>& output,
                                               const ::synth2::audio::MidiBuffer& midi,
                                               int startSample, int numSamples,
                                               void* outputContext,OutputSink outputSink)
    {
        // Internal chunks handle only MIDI events inside this half-open region.
        // Later callbacks retain ownership of trailing events.
        const ::synth2::audio::ScopedValueSetter<void*> context(activeOutputContext,outputContext);
        const ::synth2::audio::ScopedValueSetter<OutputSink> sink(activeOutputSink,outputSink);
        renderedVoicesInRegion=false;
        if (corePool != nullptr && params != nullptr) corePool->limitCapacity(params->playMode!=0 ? std::max(params->polyphony,params->unisonVoices) : params->polyphony);
        const int voiceLimit = corePool != nullptr && params != nullptr && params->playMode == 0
            ? getNumVoices() : logicalVoiceLimit (params, getNumVoices());
        for (int i = voiceLimit; i < getNumVoices(); ++i)
            if (auto* voice = getVoice (i); voice->isVoiceActive()) stopVoice (voice, 0.0f, false);
        const int endSample = startSample + numSamples;
        int cursor = startSample;
        nativeWorkspaceOrigin=startSample;
        auto inputBoundary=midi.findNextSamplePosition(startSample);
        std::optional<::synth2::audio::MidiMessage> pendingArpOff;
        int pendingArpPosition=0;
        const auto flushArpOff=[&] {
            if(!pendingArpOff)return;
            const ::synth2::audio::ScopedValueSetter<bool> generated(dispatchingArpeggiator,true);
            handleMidiEvent(*pendingArpOff);pendingArpOff.reset();
        };
        // Silent arp boundaries use the same render partition as MIDI events.
        const auto renderUntil=[&](int position) {
            if(position>cursor)
            {
                flushArpOff();renderVoicesWithLfos(output,cursor,position-cursor,true);
            }
            cursor=position;
            // The native VST bridge reuses its aligned workspace for each
            // input-MIDI region; internal arp boundaries keep advancing it.
            while(inputBoundary!=midi.cend() && (*inputBoundary).samplePosition<position)++inputBoundary;
            if(inputBoundary!=midi.cend() && (*inputBoundary).samplePosition==position)nativeWorkspaceOrigin=position;
        };
        arpeggiator.process (midi, startSample, numSamples,
                             params != nullptr ? params->arpeggiator : ArpeggiatorSettings {},
                             params != nullptr ? params->hostBpm : 120.0,
                             [&](const ::synth2::audio::MidiMessage& message, int position)
        {
            renderUntil(position);
            const ::synth2::audio::ScopedValueSetter<bool> generated(dispatchingArpeggiator,true);
            if(pendingArpOff)
            {
                if(message.isNoteOn() && position==pendingArpPosition
                    && message.getChannel()==pendingArpOff->getChannel())
                {
                    // A connected full-gate native legato step retains the
                    // sounding envelope even for a repeated pitch. Keep the
                    // held-note overlap until its new assignment is installed.
                    handleMidiEvent(message);
                    if(message.getNoteNumber()!=pendingArpOff->getNoteNumber())handleMidiEvent(*pendingArpOff);
                    pendingArpOff.reset();return;
                }
                flushArpOff();
            }
            if(message.isNoteOff() && params!=nullptr
                && params->playMode==2 && params->arpeggiator.enabled
                // Random order uses the same connected full-gate assignment
                // as ordered ARP; do not release its core between same-time events.
                && ((params->arpeggiator.type>=1 && params->arpeggiator.type<=3)
                    || params->arpeggiator.type==4)
                && params->arpeggiator.gate==127)
            {
                pendingArpOff=message;pendingArpPosition=position;return;
            }
            handleMidiEvent (message);
        },renderUntil,[&](const ::synth2::audio::MidiMessage& message,int position) {
            if(message.isController() || message.isPitchWheel())renderUntil(position);
            if(message.isNoteOnOrOff() || message.isAllNotesOff() || message.isAllSoundOff()
                || (message.isController() && message.getControllerNumber()==64))
            {
                // Native key sync belongs to the upstream keyboard, not the
                // generated ARP gate. Preserve exact input-event boundaries.
                renderUntil(position);trackLfoKey(message);
            }
        },nativeOmniInput);
        flushArpOff();
        if (cursor < endSample) renderVoicesWithLfos (output, cursor, endSample - cursor);
    }

}
