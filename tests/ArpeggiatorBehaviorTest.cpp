#include "render/SharedModulationConfiguration.h"
#include <gtest/gtest.h>
#include "midi/Arpeggiator.h"
#include "patch/Synth1PatchMapper.h"
#include "SynthEngine.h"
#include <tuple>
#include <vector>

namespace
{
    using Event = std::tuple<int, int, int, int>;
    using Events = std::vector<Event>;
    synth2::ArpeggiatorSettings defaults()
    {
        synth2::ArpeggiatorSettings p;
        p.enabled = true; p.type = 2; p.range = 2; p.beat = 14; p.gate = 64;
        return p;
    }
    void capture (Events& out, const synth2::audio::MidiMessage& msg, int frame)
    {
        const auto* raw = msg.getRawData();
        out.emplace_back (frame, raw[0], msg.getRawDataSize() > 1 ? raw[1] : 0,
                          msg.getRawDataSize() > 2 ? raw[2] : 0);
    }
    Events render (const synth2::ArpeggiatorSettings& p, const synth2::audio::MidiBuffer& input,
                   int block = 64, int length = 1000, double rate = 1000, double bpm = 120)
    {
        synth2::midi::Arpeggiator arp;
        arp.prepare (rate);
        Events result;
        for (int pos = 0; pos < length; pos += block)
        {
            constexpr int prefix = 7;
            synth2::audio::MidiBuffer part;
            const int frames = std::min (block, length - pos);
            for (auto event : input)
                if (event.samplePosition >= pos && event.samplePosition < pos + frames)
                    part.addEvent (event.getMessage(), event.samplePosition - pos + prefix);
            arp.process (part, prefix, frames, p, bpm, [&](const auto& msg, int frame)
            {
                EXPECT_GE(frame, prefix); EXPECT_LT(frame, prefix + frames);
                capture (result, msg, pos + frame - prefix);
            });
        }
        return result;
    }
    Events notes (const Events& events, bool on = true)
    {
        Events out;
        for (auto event : events)
            if ((std::get<1> (event) & 0xf0) == (on ? 0x90 : 0x80)) out.push_back (event);
        return out;
    }
    synth2::audio::MidiBuffer held (std::initializer_list<int> keys = {69}, int position = 0)
    {
        synth2::audio::MidiBuffer midi;
        for (int key : keys) midi.addEvent (synth2::audio::MidiMessage::noteOn (1, key, static_cast<uint8_t> (100)), position);
        return midi;
    }
}

TEST(Arpeggiator, TimingIsIndependentOfBlocksAndInputSubregionOffsets)
{
    auto midi = held ({60, 64, 67}, 113);
    midi.addEvent (synth2::audio::MidiMessage::noteOff (1, 64), 997);
    midi.addEvent (synth2::audio::MidiMessage::pitchWheel (1, 12288), 512);
    midi.addEvent (synth2::audio::MidiMessage::allNotesOff (1), 90001);
    auto p = defaults(); p.beat = 13;
    const auto expected = render (p, midi, 64, 100000, 44100, 137);
    ASSERT_GT(notes(expected).size(), 10u);
    for (int size : {1, 63, 127, 512, 1000}) EXPECT_EQ(render(p, midi, size, 100000, 44100, 137), expected);
}

TEST(Arpeggiator, SameSampleChordIsSortedBeforeTheFirstGeneratedNote)
{
    auto p = defaults(); p.range = 0;
    const auto events = notes (render (p, held ({67, 60, 64}), 64, 500));
    ASSERT_EQ(events.size(), 4u);
    for (int i = 0; i < 4; ++i) EXPECT_EQ(std::get<2>(events[i]), (std::vector<int>{60,64,67,60})[i]);
}

TEST(Arpeggiator, DisabledProcessingPreservesMidiAndTracksHeldKeysForEnabling)
{
    auto p = defaults(); p.enabled = false;
    auto midi = held(); midi.addEvent (synth2::audio::MidiMessage::pitchWheel(1, 9000), 20);
    Events expected;
    for (auto event : midi) capture (expected, event.getMessage(), event.samplePosition);
    EXPECT_EQ(render(p, midi, 64, 500), expected);
    synth2::midi::Arpeggiator arp; arp.prepare (1000);
    Events events;
    auto sink = [&](const auto& msg, int frame) { capture (events, msg, frame); };
    arp.process (midi, 0, 100, p, 120, sink);
    events.clear(); p.enabled = true; synth2::audio::MidiBuffer empty;
    arp.process (empty, 0, 50, p, 120, sink);
    ASSERT_EQ(notes(events).size(), 1u);
    EXPECT_EQ(std::get<2>(notes(events)[0]), 69);
    events.clear(); p.enabled = false;
    arp.process (empty, 0, 100, p, 120, sink);
    EXPECT_EQ(notes(events).size(), 1u); EXPECT_EQ(notes(events,false).size(), 1u);
}

TEST(Arpeggiator, RandomIsRepeatableAcrossBlocksAndPrepareClearsHeldKeys)
{
    auto p = defaults(); p.type = 4;
    EXPECT_EQ(render(p, held(), 1), render(p, held(), 512));
    auto on = notes (render (p, held(), 64, 10000));
    bool changed = false;
    for (auto event : on)
    {
        const int note = std::get<2>(event);
        EXPECT_TRUE(note == 69 || note == 81 || note == 93);
        changed = changed || note != std::get<2>(on.front());
    }
    EXPECT_TRUE(changed);
    synth2::midi::Arpeggiator arp; arp.prepare (1000);
    Events events; auto sink = [&](const auto& msg, int frame) { capture(events,msg,frame); };
    auto midi = held(); arp.process (midi, 0, 100, p, 120, sink);
    arp.prepare (1000); events.clear(); synth2::audio::MidiBuffer empty;
    arp.process (empty, 0, 1000, p, 120, sink); EXPECT_TRUE(events.empty());
}

TEST(Arpeggiator, ParameterMappingConnectsAllImportedControls)
{
    std::array<float,128> raw {};
    raw[31] = 3.0f/127; raw[32] = 2.0f/127; raw[33] = 14.0f/127; raw[34] = 96.0f/127; raw[59] = 1.0f/127;
    synth2::RenderControls p;
    synth2::Synth1PatchMapper (113).mapFromRawBuffer (raw.data(), p);
    EXPECT_TRUE(p.arpeggiator.enabled); EXPECT_EQ(p.arpeggiator.type,3);
    EXPECT_EQ(p.arpeggiator.range,2); EXPECT_EQ(p.arpeggiator.beat,14); EXPECT_EQ(p.arpeggiator.gate,96);
}
