#pragma once
#include "audio/Midi.h"

#include "ArpeggiatorSettings.h"
#include "../dsp/ArpeggiatorClock.h"
#include "../dsp/RandomLfo.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace synth2::midi
{
    // Fixed-size keyboard state and event scheduling; generated short MIDI
    // messages go directly to a sink without a growing intermediate MidiBuffer.
    class Arpeggiator
    {
    public:
        void setNativeRandomSource(dsp::CrtRandom* source) noexcept { nativeRandomSource=source; }
        static double beatsForIndex (int index) noexcept
        {
            static constexpr double beats[] {4, 3.5, 3, 2, 1.75, 1.5, 4.0/3, 1,
                0.875, 0.75, 2.0/3, 0.5, 0.375, 1.0/3, 0.25, 1.0/6, 0.125, 1.0/12, 1.0/24};
            return beats[std::clamp (index, 0, 18)];
        }

        void prepare (double rate) noexcept
        {
            sampleRate = rate;
            clock = 0;
            enabled = false;
            interval = 0;
            nativeClockTiming = nativeIdleTiming = false;
            idleGate = idleGateFrames = 0;
            channels = {};
            for (int c = 0; c < 16; ++c) channels[c].random = 0xa341316cu + static_cast<uint32_t> (c);
        }

        // Read-only lookahead; only process() advances the scheduler or RNG.
        int framesUntilBoundary(double frame,int limit) const noexcept
        {
            if(!enabled)return limit;
            for(const auto& channel:channels)
                for(double next:{channel.next,channel.off,channel.idleNext})
                {
                    const double distance=std::ceil(next-1e-8)-frame;
                    if(distance>0 && distance<limit)limit=static_cast<int>(distance);
                }
            return limit;
        }

        template <typename Sink>
        void process (const ::synth2::audio::MidiBuffer& input, int start, int count,
                      const ArpeggiatorSettings& params, double bpm, Sink&& sink)
        {
            process(input,start,count,params,bpm,std::forward<Sink>(sink),[](int){});
        }

        template <typename Sink,typename Boundary>
        void process (const ::synth2::audio::MidiBuffer& input, int start, int count,
                      const ArpeggiatorSettings& params, double bpm, Sink&& sink,Boundary&& boundary)
        {
            process(input,start,count,params,bpm,std::forward<Sink>(sink),std::forward<Boundary>(boundary),[](const ::synth2::audio::MidiMessage&,int){});
        }

        template <typename Sink,typename Boundary,typename InputEvent>
        void process (const ::synth2::audio::MidiBuffer& input, int start, int count,
                      const ArpeggiatorSettings& params, double bpm, Sink&& sink,Boundary&& boundary,InputEvent&& inputEvent,bool nativeOmni=false)
        {
            if (count <= 0) return;
            const double begin = clock, end = begin + count;
            auto emit = [&](const ::synth2::audio::MidiMessage& msg, double time)
            {
                sink (msg, start + static_cast<int> (time - begin));
            };
            const double safeBpm = std::isfinite (bpm) && bpm > 0 ? bpm : 120.0;
            // The DLL multiplies integer rate/ticks, divides in float, and
            // truncates. Keep the preceding policy outside the audited domain.
            nativeClockTiming = sampleRate >= 1 && sampleRate <= 192000
                && sampleRate == std::floor(sampleRate) && safeBpm >= 1 && safeBpm <= 1000000;
            const double nextInterval = nativeClockTiming
                ? std::max(1, dsp::ArpeggiatorClock::period(static_cast<int>(sampleRate),static_cast<float>(safeBpm),params.beat))
                : std::max (1.0, std::floor (sampleRate * 60.0 / safeBpm * beatsForIndex (params.beat)));
            if (interval > 0 && interval != nextInterval)
                for (auto& ch : channels)
                {
                    if (std::isfinite (ch.next)) ch.next = begin + std::max (0.0, ch.next - begin) * nextInterval / interval;
                    if (std::isfinite (ch.off)) ch.off = begin + std::max (0.0, ch.off - begin) * nextInterval / interval;
                }
            if(interval != nextInterval || idleGate != params.gate)
                for(auto& ch:channels)ch.idleNext=infinity;
            interval = nextInterval;
            idleGate=params.gate;
            nativeRandomTiming=nativeClockTiming && params.nativeRandom && params.type==4 && nativeRandomSource!=nullptr;
            nativeIdleTiming=nativeClockTiming && ((params.type>=1 && params.type<=3) || nativeRandomTiming) && idleGate>0 && idleGate<=127;
            if(!nativeIdleTiming)for(auto& ch:channels)ch.idleNext=infinity;
            idleGateFrames=nativeIdleTiming ? std::max(1,dsp::ArpeggiatorClock::gateFrames(static_cast<int>(interval),idleGate)) : 0;
            if (params.enabled != enabled)
            {
                for (int c = 0; c < 16; ++c)
                {
                    auto& ch = channels[c];
                    stop (ch, c, begin, emit);
                    ch.next = infinity;
                    ch.idleNext = infinity;
                    ch.step = 0;
                    if (params.enabled)
                    {
                        // A pedal pressed while bypassed may still be down in
                        // the downstream synth even when no keys are held.
                        if (ch.sustain) emit (::synth2::audio::MidiMessage::controllerEvent (c + 1, 64, 0), begin);
                        if (hasNotes (ch))
                        {
                            emit (::synth2::audio::MidiMessage::allSoundOff (c + 1), begin);
                            ch.next = begin;
                        }
                        else if(nativeRandomTiming && c==0)
                        {
                            ch.idleNext=begin;ch.idleGateEnd=false;
                        }
                    }
                    else
                    {
                        emit (::synth2::audio::MidiMessage::controllerEvent (c + 1, 64, ch.sustain ? 127 : 0), begin);
                        for (int key = 0; key < 128; ++key)
                            if (ch.velocity[key] > 0)
                            {
                                emit (::synth2::audio::MidiMessage::noteOn (c + 1, key, ch.velocity[key]), begin);
                                if (! ch.down[key]) emit (::synth2::audio::MidiMessage::noteOff (c + 1, key), begin);
                            }
                    }
                }
                enabled = params.enabled;
            }

            auto advance = [&](double limit)
            {
                while (enabled)
                {
                    double next = infinity;
                    for (const auto& ch : channels) next = std::min ({next,ch.next,ch.off,ch.idleNext});
                    const double sample = std::ceil (next - 1e-8);
                    if (sample >= limit) break;
                    for (int c = 0; c < 16; ++c)
                    {
                        auto& ch = channels[c];
                        // An empty native keyboard still partitions the audio
                        // at gate/period boundaries; it emits no MIDI message.
                        if(std::ceil(ch.idleNext-1e-8)<=sample)
                        {
                            boundary(start+static_cast<int>(sample-begin));
                            if(idleGate==127)
                            {
                                // Full gate keeps one idle boundary per period.
                                // Only random order consumes an octave draw.
                                if(nativeRandomTiming)nativeRandomSource->next();
                                ch.idleNext+=interval;
                            }
                            else
                            {
                                if(nativeRandomTiming && !ch.idleGateEnd)nativeRandomSource->next();
                                ch.idleNext+=ch.idleGateEnd ? interval-idleGateFrames : idleGateFrames;
                                ch.idleGateEnd=!ch.idleGateEnd;
                            }
                        }
                        if (std::ceil (ch.off - 1e-8) <= sample)
                        {
                            // An out-of-range random step emits no note, but
                            // native CArp still partitions at its gate boundary.
                            if(nativeRandomTiming)boundary(start+static_cast<int>(sample-begin));
                            stop (ch, c, sample, emit);
                        }
                        if (std::ceil (ch.next - 1e-8) <= sample)
                        {
                            // Finish preceding CLFO draws before this arp step.
                            if(nativeRandomTiming)boundary(start+static_cast<int>(sample-begin));
                            step (ch, c, sample, params, emit);
                        }
                    }
                }
            };
            auto it = input.findNextSamplePosition (start);
            while (it != input.cend() && (*it).samplePosition < start + count)
            {
                const int position = (*it).samplePosition;
                const double time = begin + position - start;
                advance (time);
                do
                {
                    // Native input note-off partitions the renderer even when
                    // the arp gate has already stopped its generated note.
                    if(enabled && nativeIdleTiming && (*it).getMessage().isNoteOff())boundary(position);
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
        static constexpr double infinity = std::numeric_limits<double>::infinity();
        struct Channel
        {
            std::array<float, 128> velocity {};
            std::array<bool, 128> down {};
            bool sustain = false;
            double next = infinity, off = infinity, idleNext = infinity;
            bool idleGateEnd = true;
            uint64_t step = 0;
            uint32_t random = 0xa341316cu;
            int sounding = -1;
            float triggerVelocity=0;
        };
        static bool hasNotes (const Channel& ch) noexcept
        {
            for (auto velocity : ch.velocity) if (velocity > 0) return true;
            return false;
        }
        template <typename Emit>
        static void stop (Channel& ch, int c, double time, Emit& emit)
        {
            if (ch.sounding >= 0) emit (::synth2::audio::MidiMessage::noteOff (c + 1, ch.sounding), time);
            ch.sounding = -1;
            ch.off = infinity;
        }
        template <typename Emit>
        void handle (const ::synth2::audio::MidiMessage& msg, double time, Emit& emit)
        {
            const int c = msg.getChannel() - 1;
            if (c < 0 || c >= 16) { emit (msg, time); return; }
            auto& ch = channels[c];
            const bool heldBefore=hasNotes(ch);
            if (msg.isNoteOn())
            {
                const bool empty = ! hasNotes (ch);
                ch.down[msg.getNoteNumber()] = true;
                ch.velocity[msg.getNoteNumber()] = msg.getFloatVelocity();
                if (enabled && empty) { ch.next = time; ch.idleNext=infinity; ch.step = 0; ch.triggerVelocity=msg.getFloatVelocity(); }
            }
            else if (msg.isNoteOff())
            {
                ch.down[msg.getNoteNumber()] = false;
                if (! ch.sustain) ch.velocity[msg.getNoteNumber()] = 0;
            }
            else if (msg.isSustainPedalOn() || msg.isSustainPedalOff())
            {
                ch.sustain = msg.isSustainPedalOn();
                if (! ch.sustain)
                    for (int key = 0; key < 128; ++key) if (! ch.down[key]) ch.velocity[key] = 0;
            }
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            {
                stop (ch, c, time, emit);
                const auto random = ch.random;
                ch = {};
                ch.random = random;
                if(enabled && nativeIdleTiming)
                {
                    if(nativeRandomTiming)nativeRandomSource->next();
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
                if((heldBefore || msg.isNoteOff() || msg.isSustainPedalOff()) && nativeIdleTiming)
                {
                    if(nativeRandomTiming)nativeRandomSource->next();
                    ch.idleNext=time+idleGateFrames;
                    ch.idleGateEnd=idleGate<127;
                }
            }
            if (! enabled || (! msg.isNoteOnOrOff() && ! msg.isSustainPedalOn() && ! msg.isSustainPedalOff()))
                emit (msg, time);
        }
        template <typename Emit>
        void step (Channel& ch, int c, double time, const ArpeggiatorSettings& p, Emit& emit)
        {
            struct Candidate { int pitch; float velocity; };
            std::array<Candidate, 512> candidates {};
            int count = 0;
            // Preserve overlapping octave notes: native C+C' across three
            // octaves plays C,C',C',C'',C'',C''' rather than deduplicating.
            for (int octave = 0; octave <= std::clamp (p.range, 0, 3); ++octave)
                for (int key = 0; key + octave * 12 < 128; ++key)
                    if (ch.velocity[key] > 0) candidates[count++] = {key + octave * 12, ch.velocity[key]};
            if (count == 0) { ch.next = infinity; stop (ch, c, time, emit); return; }
            if (p.gate <= 0)
            {
                stop (ch, c, time, emit);
                ch.next += interval; ++ch.step;
                return;
            }
            int index = static_cast<int> (ch.step % static_cast<uint64_t> (count));
            int nativePitch=-1;float nativeVelocity=0;
            if (p.type == 1 && count > 1)
            {
                const int turn = static_cast<int> (ch.step % static_cast<uint64_t> (2 * (count - 1)));
                index = turn < count ? turn : 2 * (count - 1) - turn;
            }
            // Native Down begins at the highest held keyboard key, descends
            // through the input octave, then wraps to the highest extension.
            else if (p.type == 3)
            {
                int highest = 0, first = 0;
                for (int key = 0; key < 128; ++key) if (ch.velocity[key] > 0) highest = key;
                for (int i = 0; i < count; ++i)
                    if (candidates[i].pitch == highest) { first = i; break; }
                index = (first + count - index) % count;
            }
            else if (p.type == 4)
            {
                if(nativeRandomTiming)
                {
                    const int octave=nativeRandomSource->next()%(std::clamp(p.range,0,3)+1);
                    std::array<int,128> held{};int keys=0;
                    for(int key=0;key<128;++key)if(ch.velocity[key]>0)held[static_cast<size_t>(keys++)]=key;
                    const int key=held[static_cast<size_t>(nativeRandomSource->next()%keys)];
                    // CArp::reset stores the first-key velocity, not the drawn key's.
                    nativePitch=key+12*octave;nativeVelocity=ch.triggerVelocity;
                }
                else
                {
                    ch.random ^= ch.random << 13; ch.random ^= ch.random >> 17; ch.random ^= ch.random << 5;
                    index = static_cast<int> (ch.random % static_cast<uint32_t> (count));
                }
            }
            const int note = nativeRandomTiming ? nativePitch : candidates[index].pitch;
            const int gate=std::clamp(p.gate,0,127);
            const double gateSamples=nativeClockTiming
                ? static_cast<double>(dsp::ArpeggiatorClock::gateFrames(static_cast<int>(interval),gate))
                : interval*gate/127.0;
            if(note>127)
            {
                stop(ch,c,time,emit);
                ch.off=ch.next+std::max(1.0,gateSamples);
                ch.next+=interval;++ch.step;return;
            }
            // At full gate overlap different notes so mono/legato can glide.
            // Repeated pitches need Off before On to avoid killing the new voice.
            const int previous = ch.sounding;
            // Native random CArp::step stops its previous note before the
            // new note-on, including different pitches under full gate.
            if (previous == note || (nativeRandomTiming && previous >= 0)) stop (ch, c, time, emit);
            emit (::synth2::audio::MidiMessage::noteOn (c + 1, note, nativeRandomTiming ? nativeVelocity : candidates[index].velocity), time);
            if (previous >= 0 && previous != note && !nativeRandomTiming) emit (::synth2::audio::MidiMessage::noteOff (c + 1, previous), time);
            ch.sounding = note;
            ch.off = p.gate >= 127 ? infinity : ch.next + std::max (1.0, gateSamples);
            ch.next += interval;
            ++ch.step;
        }
        std::array<Channel, 16> channels {};
        double sampleRate = 44100, clock = 0, interval = 0;
        bool enabled = false, nativeClockTiming = false, nativeIdleTiming = false;
        bool nativeRandomTiming=false;
        dsp::CrtRandom* nativeRandomSource=nullptr;
        int idleGate=0,idleGateFrames=0;
    };
}
