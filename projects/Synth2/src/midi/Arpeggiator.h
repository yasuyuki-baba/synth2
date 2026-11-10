#pragma once
#include "audio/Midi.h"

#include "ArpeggiatorSettings.h"
#include "../dsp/ArpeggiatorClock.h"
#include "../dsp/RandomLfo.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace synth2::midi
{
    // Fixed-size keyboard state and event scheduling; generated short MIDI
    // messages go directly to a sink without a growing intermediate MidiBuffer.
    class Arpeggiator
    {
    public:
        explicit Arpeggiator(dsp::CrtRandom& random) noexcept : randomSource(random) {}
        void prepare (float rate) noexcept
        {
            sampleRate = rate;
            clock = 0;
            enabled = false;
            interval = 0;
            beat=11;tempo=120.0f;
            idleTiming = false;
            idleGate = idleGateFrames = 0;
            channels = {};
        }

        // Read-only lookahead; only process() advances the scheduler or RNG.
        int framesUntilBoundary(int64_t frame,int limit) const noexcept
        {
            if(!enabled)return limit;
            for(const auto& channel:channels)
                for(int64_t next:{channel.next,channel.off,channel.idleNext})
                {
                    const int64_t distance=next-frame;
                    if(distance>0 && distance<limit)limit=static_cast<int>(distance);
                }
            return limit;
        }

        template <typename Sink>
        void process (const ::synth2::audio::MidiBuffer& input, int start, int count,
                      const ArpeggiatorSettings& params, float bpm, Sink&& sink)
        {
            process(input,start,count,params,bpm,std::forward<Sink>(sink),[](int){});
        }

        template <typename Sink,typename Boundary>
        void process (const ::synth2::audio::MidiBuffer& input, int start, int count,
                      const ArpeggiatorSettings& params, float bpm, Sink&& sink,Boundary&& boundary)
        {
            process(input,start,count,params,bpm,std::forward<Sink>(sink),std::forward<Boundary>(boundary),[](const ::synth2::audio::MidiMessage&,int){});
        }

        template <typename Sink,typename Boundary,typename InputEvent>
        void process (const ::synth2::audio::MidiBuffer& input, int start, int count,
                      const ArpeggiatorSettings& params, float bpm, Sink&& sink,Boundary&& boundary,InputEvent&& inputEvent,bool nativeOmni=false)
        {
            if (count <= 0) return;
            const int64_t begin = clock, end = begin + count;
            auto emit = [&](const ::synth2::audio::MidiMessage& msg, int64_t time)
            {
                sink (msg, start + static_cast<int> (time - begin));
            };
            const float safeBpm = std::isfinite (bpm) && bpm > 0 ? bpm : 120.0f;
            // One integer period/gate clock at every supported host rate.
            const int nextInterval=std::max(1,dsp::ArpeggiatorClock::period(
                sampleRate,static_cast<float>(safeBpm),params.beat));
            // Host beat setters precede the callback's tempo update. Preserve
            // each native Float32 counter conversion, including intermediate
            // truncation when both controls change together.
            if(interval>0 && (beat!=params.beat || tempo!=safeBpm))
            {
                if(beat!=params.beat)
                    updatePeriod(begin,std::max(1,dsp::ArpeggiatorClock::period(sampleRate,tempo,params.beat)),params.gate);
                if(tempo!=safeBpm)updatePeriod(begin,nextInterval,params.gate);
            }
            if(interval>0 && idleGate!=params.gate)updateGate(begin,params.gate);
            beat=params.beat;tempo=safeBpm;
            interval = nextInterval;
            idleGate=params.gate;
            arpType=params.type;
            randomTiming=params.type==4 && params.gate>0;
            idleTiming= ((params.type>=1 && params.type<=3) || randomTiming) && idleGate>0 && idleGate<=127;
            if(!idleTiming)for(auto& ch:channels)ch.idleNext=infinity;
            idleGateFrames=idleTiming ? std::max(1,dsp::ArpeggiatorClock::gateFrames(static_cast<int>(interval),idleGate)) : 0;
            if (params.enabled != enabled)
            {
                for (int c = 0; c < 16; ++c)
                {
                    auto& ch = channels[c];
                    stop (ch, c, begin, emit);
                    ch.next = infinity;
                    ch.idleNext = infinity;
                    ch.octave=0;ch.descending=params.type==3;
                    if (params.enabled)
                    {
                        // A pedal pressed while bypassed may still be down in
                        // the downstream synth even when no keys are held.
                        if (ch.sustain) emit (::synth2::audio::MidiMessage::controllerEvent (c + 1, 64, 0), begin);
                        if (hasNotes (ch))
                        {
                            for(int i=0;i<ch.heldCount;++i)
                                emit(::synth2::audio::MidiMessage::noteOff(c+1,ch.heldOrder[static_cast<size_t>(i)]),begin);
                            ch.next = begin;
                        }
                        else if(randomTiming && c==0)
                        {
                            ch.idleNext=begin;ch.idleGateEnd=false;
                        }
                    }
                    else
                    {
                        emit (::synth2::audio::MidiMessage::controllerEvent (c + 1, 64, ch.sustain ? 127 : 0), begin);
                        for (int index=0;index<ch.heldCount;++index)
                        {
                            const int key=ch.heldOrder[static_cast<size_t>(index)];
                            emit (::synth2::audio::MidiMessage::noteOn (c + 1, key, ch.velocity[key]), begin);
                            if (! ch.down[key]) emit (::synth2::audio::MidiMessage::noteOff (c + 1, key), begin);
                        }
                    }
                }
                enabled = params.enabled;
            }

            auto advance = [&](int64_t limit)
            {
                while (enabled)
                {
                    int64_t next = infinity;
                    for (const auto& ch : channels) next = std::min ({next,ch.next,ch.off,ch.idleNext});
                    const int64_t sample=next;
                    if (sample >= limit) break;
                    for (int c = 0; c < 16; ++c)
                    {
                        auto& ch = channels[c];
                        // An empty native keyboard still partitions the audio
                        // at gate/period boundaries; it emits no MIDI message.
                        if(ch.idleNext<=sample)
                        {
                            boundary(start+static_cast<int>(sample-begin));
                            if(idleGate==127)
                            {
                                // Full gate keeps one idle boundary per period.
                                // Only random order consumes an octave draw.
                                if(randomTiming)randomSource.next();
                                ch.idleNext+=interval;
                            }
                            else
                            {
                                if(randomTiming && !ch.idleGateEnd)randomSource.next();
                                ch.idleNext+=ch.idleGateEnd ? interval-idleGateFrames : idleGateFrames;
                                ch.idleGateEnd=!ch.idleGateEnd;
                            }
                        }
                        if (ch.off <= sample)
                        {
                            // An out-of-range random step emits no note, but
                            // native CArp still partitions at its gate boundary.
                            if(randomTiming)boundary(start+static_cast<int>(sample-begin));
                            stop (ch, c, sample, emit);
                        }
                        if (ch.next <= sample)
                        {
                            // Finish preceding CLFO draws before this arp step.
                            if(randomTiming)boundary(start+static_cast<int>(sample-begin));
                            step (ch, c, sample, params, emit);
                        }
                    }
                }
            };
            auto it = input.findNextSamplePosition (start);
            while (it != input.cend() && (*it).samplePosition < start + count)
            {
                const int position = (*it).samplePosition;
                const int64_t time = begin + position - start;
                advance (time);
                do
                {
                    // Native input note-off partitions the renderer even when
                    // the arp gate has already stopped its generated note.
                    if(enabled && idleTiming && (*it).getMessage().isNoteOff())boundary(position);
                    auto message=(*it).getMessage();
                    // Synth1 has one upstream keyboard and performance state;
                    // channel nibbles do not partition its voice/ARP ownership.
                    if(nativeOmni && message.getChannel()!=0)message.setChannel(1);
                    inputEvent(message,position);
                    handle (message, time, emit);
                    ++it;
                } while (it != input.cend() && (*it).samplePosition == position);
                // Same-sample chord notes are collected before the first step.
                advance (time + 1);
            }
            advance (end);
            clock = end;
        }

    private:
        static constexpr int64_t infinity=std::numeric_limits<int64_t>::max();
        struct Channel
        {
            std::array<float, 128> velocity {};
            std::array<bool, 128> down {};
            std::array<uint8_t,128> heldOrder {};
            int heldCount=0;
            bool sustain = false;
            int64_t next = infinity, off = infinity, idleNext = infinity;
            bool idleGateEnd = true;
            int lastKey=-1,octave=0;
            bool descending=false;
            int sounding = -1;
            float triggerVelocity=0;
        };
        // Counter values are small relative positions. Absolute sample clocks
        // remain int64_t and never pass through floating-point arithmetic.
        int counterAt(const Channel& ch,int64_t begin) const noexcept
        {
            if(ch.next!=infinity)
                return ch.next<=begin ? 0 : static_cast<int>(std::clamp<int64_t>(interval-(ch.next-begin),0,interval-1));
            if(ch.idleNext!=infinity)
            {
                const int edge=ch.idleGateEnd ? idleGateFrames : interval;
                return static_cast<int>(std::clamp<int64_t>(edge-(ch.idleNext-begin),0,interval-1));
            }
            return 0;
        }
        static int scaledCounter(int counter,int next,int previous) noexcept
        {
            const float scaled=static_cast<float>(counter)*static_cast<float>(next);
            return std::clamp(dsp::ArpeggiatorClock::frameCount(scaled/static_cast<float>(previous)),0,next-1);
        }
        void updatePeriod(int64_t begin,int next,int gate) noexcept
        {
            for(auto& ch:channels)
            {
                if(ch.next==infinity && ch.idleNext==infinity)continue;
                const int counter=scaledCounter(counterAt(ch,begin),next,interval);
                const int gateFrames=dsp::ArpeggiatorClock::gateFrames(next,gate);
                if(ch.next!=infinity)
                {
                    ch.next=begin+(counter==0 ? 0 : next-counter);
                    ch.off=gate<127 && counter<=gateFrames ? begin+gateFrames-counter : infinity;
                }
                if(ch.idleNext!=infinity)
                {
                    if(counter==0){ch.idleNext=begin;ch.idleGateEnd=false;}
                    else if(gate<127 && counter<=gateFrames){ch.idleNext=begin+gateFrames-counter;ch.idleGateEnd=true;}
                    else {ch.idleNext=begin+next-counter;ch.idleGateEnd=false;}
                }
            }
            interval=next;
            idleGateFrames=std::max(1,dsp::ArpeggiatorClock::gateFrames(interval,gate));
        }
        void updateGate(int64_t begin,int gate) noexcept
        {
            const int gateFrames=dsp::ArpeggiatorClock::gateFrames(interval,gate);
            for(auto& ch:channels)
            {
                const int counter=counterAt(ch,begin);
                if(ch.next!=infinity)ch.off=gate<127 && counter<=gateFrames ? begin+gateFrames-counter : infinity;
                if(ch.idleNext!=infinity)
                {
                    if(counter==0){ch.idleNext=begin;ch.idleGateEnd=false;}
                    else if(gate<127 && counter<=gateFrames){ch.idleNext=begin+gateFrames-counter;ch.idleGateEnd=true;}
                    else {ch.idleNext=begin+interval-counter;ch.idleGateEnd=false;}
                }
            }
        }
        static void resetCursor(Channel& ch) noexcept
        {ch.lastKey=-1;ch.octave=0;ch.descending=false;}
        static int heldKey(const Channel& ch,int from,bool down) noexcept
        {
            for(int key=from;key>=0 && key<128;key+=down ? -1 : 1)
                if(ch.velocity[static_cast<size_t>(key)]>0)return key;
            return -1;
        }
        static int orderedPitch(Channel& ch,const ArpeggiatorSettings& p) noexcept
        {
            const int range=std::clamp(p.range,0,3);
            int key=heldKey(ch,ch.lastKey<0 ? (ch.descending ? 127 : 0) : ch.lastKey+(ch.descending ? -1 : 1),ch.descending);
            if(key<0)
            {
                if(!ch.descending && ch.octave<range){++ch.octave;key=heldKey(ch,0,false);}
                else if(ch.descending && ch.octave>0){--ch.octave;key=heldKey(ch,127,true);}
                else if(p.type==1)
                {
                    ch.descending=!ch.descending;
                    key=heldKey(ch,ch.lastKey+(ch.descending ? -1 : 1),ch.descending);
                    if(key<0 && ch.lastKey>=0 && ch.velocity[static_cast<size_t>(ch.lastKey)]>0)
                    {
                        key=ch.lastKey;
                        if(range>0)ch.octave+=ch.descending ? -1 : 1;
                    }
                }
                else {ch.octave=ch.descending ? range : 0;key=heldKey(ch,ch.descending ? 127 : 0,ch.descending);}
            }
            ch.lastKey=key;
            return key<0 ? -1 : key+12*ch.octave;
        }
        static bool hasNotes(const Channel& ch) noexcept {return ch.heldCount!=0;}
        static void removeKey(Channel& ch,int key) noexcept
        {
            int index=0;while(index<ch.heldCount && ch.heldOrder[static_cast<size_t>(index)]!=key)++index;
            if(index==ch.heldCount)return;
            for(;index+1<ch.heldCount;++index)ch.heldOrder[static_cast<size_t>(index)]=ch.heldOrder[static_cast<size_t>(index+1)];
            --ch.heldCount;
        }
        template <typename Emit>
        static void stop (Channel& ch, int c, int64_t time, Emit& emit)
        {
            if (ch.sounding >= 0) emit (::synth2::audio::MidiMessage::noteOff (c + 1, ch.sounding), time);
            ch.sounding = -1;
            ch.off = infinity;
        }
        template <typename Emit>
        void handle (const ::synth2::audio::MidiMessage& msg, int64_t time, Emit& emit)
        {
            const int c = msg.getChannel() - 1;
            if (c < 0 || c >= 16) { emit (msg, time); return; }
            auto& ch = channels[c];
            const bool heldBefore=hasNotes(ch);
            if (msg.isNoteOn())
            {
                const bool empty = ! hasNotes (ch);
                removeKey(ch,msg.getNoteNumber());
                ch.heldOrder[static_cast<size_t>(ch.heldCount++)]=static_cast<uint8_t>(msg.getNoteNumber());
                ch.down[msg.getNoteNumber()] = true;
                ch.velocity[msg.getNoteNumber()] = msg.getFloatVelocity();
                if (enabled && empty) { ch.next = time; ch.idleNext=infinity; resetCursor(ch);ch.descending=arpType==3; ch.triggerVelocity=msg.getFloatVelocity(); }
            }
            else if (msg.isNoteOff())
            {
                ch.down[msg.getNoteNumber()] = false;
                if (! ch.sustain){ch.velocity[msg.getNoteNumber()]=0;removeKey(ch,msg.getNoteNumber());}
            }
            else if (msg.isSustainPedalOn() || msg.isSustainPedalOff())
            {
                ch.sustain = msg.isSustainPedalOn();
                if (! ch.sustain)
                    for (int key = 0; key < 128; ++key)if(!ch.down[key]){ch.velocity[key]=0;removeKey(ch,key);}
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                stop (ch, c, time, emit);
                ch = {};
                if(enabled && idleTiming)
                {
                    if(randomTiming)randomSource.next();
                    ch.idleNext=time+idleGateFrames;ch.idleGateEnd=idleGate<127;
                }
                emit (msg, time);
                return;
            }
            if (enabled && ! hasNotes (ch))
            {
                stop (ch, c, time, emit);
                ch.next = infinity;
                // Native last-key release resets the arp counter but leaves
                // the enabled clock running while voice/effect tails decay.
                if((heldBefore || msg.isNoteOff() || msg.isSustainPedalOff()) && idleTiming)
                {
                    if(randomTiming)randomSource.next();
                    ch.idleNext=time+idleGateFrames;
                    ch.idleGateEnd=idleGate<127;
                }
            }
            if (! enabled || (! msg.isNoteOnOrOff() && ! msg.isSustainPedalOn() && ! msg.isSustainPedalOff()))
                emit (msg, time);
        }
        template <typename Emit>
        void step (Channel& ch, int c, int64_t time, const ArpeggiatorSettings& p, Emit& emit)
        {
            if(!hasNotes(ch)){ch.next=infinity;stop(ch,c,time,emit);return;}
            if(p.gate<=0){stop(ch,c,time,emit);ch.next+=interval;return;}
            int note;
            if(p.type==4)
            {
                const int octave=randomSource.next()%(std::clamp(p.range,0,3)+1);
                std::array<int,128> held{};int keys=0;
                for(int key=0;key<128;++key)if(ch.velocity[key]>0)held[static_cast<size_t>(keys++)]=key;
                note=held[static_cast<size_t>(randomSource.next()%keys)]+12*octave;
            }
            else note=orderedPitch(ch,p);
            const int gate=std::clamp(p.gate,0,127);
            const int gateSamples=dsp::ArpeggiatorClock::gateFrames(static_cast<int>(interval),gate);
            if(note<0 || note>127)
            {
                stop(ch,c,time,emit);
                ch.off=ch.next+std::max(1,gateSamples);
                ch.next+=interval;return;
            }
            // At full gate overlap different notes so mono/legato can glide.
            // Repeated pitches need Off before On to avoid killing the new voice.
            const int previous = ch.sounding;
            // Native random CArp::step stops its previous note before the
            // new note-on, including different pitches under full gate.
            if (previous == note || ((randomTiming || p.gate<127) && previous >= 0)) stop (ch, c, time, emit);
            emit (::synth2::audio::MidiMessage::noteOn (c + 1, note, ch.triggerVelocity), time);
            if (previous >= 0 && previous != note && !randomTiming && p.gate>=127) emit (::synth2::audio::MidiMessage::noteOff (c + 1, previous), time);
            ch.sounding = note;
            ch.off = p.gate >= 127 ? infinity : ch.next + std::max (1, gateSamples);
            ch.next += interval;
        }
        std::array<Channel, 16> channels {};
        float sampleRate=44100;
        int64_t clock=0;
        int interval=0,beat=11,arpType=2;
        float tempo=120.0f;
        bool enabled = false, idleTiming = false;
        bool randomTiming=false;
        dsp::CrtRandom& randomSource;
        int idleGate=0,idleGateFrames=0;
    };
}
