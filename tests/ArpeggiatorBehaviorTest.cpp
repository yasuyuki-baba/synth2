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
        synth2::dsp::CrtRandom arpRandom;synth2::midi::Arpeggiator arp(arpRandom);
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
    synth2::dsp::CrtRandom arpRandom;synth2::midi::Arpeggiator arp(arpRandom); arp.prepare (1000);
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
    synth2::dsp::CrtRandom arpRandom;synth2::midi::Arpeggiator arp(arpRandom); arp.prepare (1000);
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

TEST(Arpeggiator, OrderedFullGateKeepsIdlePeriodsWithoutRandomDraws)
{
    using synth2::audio::MidiMessage;
    // At 1 kHz and 120 BPM, a sixteenth-note period is 125 frames.
    for(int type:{1,2,3})for(int block:{1,64,257})for(int release:{0,1,2})
    {
        auto p=defaults();p.type=type;p.gate=127;p.range=0;
        auto input=held({60});
        if(release==1)input.addEvent(MidiMessage::controllerEvent(1,64,127),10);
        input.addEvent(MidiMessage::noteOff(1,60),37);
        if(release==1)input.addEvent(MidiMessage::controllerEvent(1,64,0),87);
        if(release==2)input.addEvent(MidiMessage::allSoundOff(1),87);
        const int emptyAt=release==0 ? 37 : 87;
        synth2::dsp::CrtRandom random;random.state=12345;
        synth2::midi::Arpeggiator arp(random);arp.prepare(1000);
        std::vector<int> idle;Events events;
        for(int pos=0;pos<500;pos+=block)
        {
            const int count=std::min(block,500-pos);constexpr int prefix=7;
            synth2::audio::MidiBuffer part;
            for(auto event:input)if(event.samplePosition>=pos && event.samplePosition<pos+count)
                part.addEvent(event.getMessage(),event.samplePosition-pos+prefix);
            arp.process(part,prefix,count,p,120,[&](const auto& message,int frame){capture(events,message,pos+frame-prefix);},
                [&](int frame){const int absolute=pos+frame-prefix;if(absolute>emptyAt)idle.push_back(absolute);});
        }
        std::vector<int> expected;for(int frame=emptyAt+125;frame<500;frame+=125)expected.push_back(frame);
        EXPECT_EQ(idle,expected);EXPECT_EQ(random.state,12345u);
        for(const auto& event:notes(events))EXPECT_LT(std::get<0>(event),emptyAt);
        p.enabled=false;synth2::audio::MidiBuffer empty;idle.clear();
        arp.process(empty,0,500,p,120,[](const auto&,int){},[&](int frame){idle.push_back(frame);});
        EXPECT_TRUE(idle.empty());
    }
}

TEST(Arpeggiator, FractionalAndHighRatesUseWholeSampleGateBoundaries)
{
    // Quarter notes at 120 BPM; gate 64/127 truncates within each integer period.
    struct Case { double rate; int period,gate; };
    for(const auto c:{Case{8000.25,4000,2015},Case{48000.25,24000,12094},Case{384000,192000,96755}})
    {
        SCOPED_TRACE(c.rate);
        auto p=defaults();p.beat=7;p.range=0;
        const auto input=held({60},13);
        const auto expected=render(p,input,64,c.period*3,c.rate);
        const auto on=notes(expected),off=notes(expected,false);
        ASSERT_EQ(on.size(),3u);ASSERT_EQ(off.size(),3u);
        for(int n=0;n<3;++n)
        {
            EXPECT_EQ(std::get<0>(on[n]),13+n*c.period);
            EXPECT_EQ(std::get<0>(off[n]),13+n*c.period+c.gate);
        }
        for(int block:{63,127,512})EXPECT_EQ(render(p,input,block,c.period*3,c.rate),expected);
    }
}

TEST(Arpeggiator, SlowTempoKeepsHeldNotesAndFullGateWithinFrameRange)
{
    for(double rate:{8000.25,48000.,384000.})for(int gate:{64,127})
    {
        SCOPED_TRACE(::testing::Message()<<rate<<" "<<gate);
        auto p=defaults();p.beat=0;p.gate=gate;
        auto input=held({60},13);
        input.addEvent(synth2::audio::MidiMessage::noteOff(1,60),997);
        const auto events=render(p,input,64,2048,rate,1e-40);
        const auto on=notes(events),off=notes(events,false);
        ASSERT_EQ(on.size(),1u);ASSERT_EQ(off.size(),1u);
        EXPECT_EQ(std::get<0>(on[0]),13);EXPECT_EQ(std::get<0>(off[0]),997);
    }
}

TEST(Arpeggiator, LongRunningFrameClockPreservesSubBlockNoteOffsets)
{
    synth2::dsp::CrtRandom random;synth2::midi::Arpeggiator arp(random);arp.prepare(48000);
    auto settings=defaults();settings.enabled=false;
    synth2::audio::MidiBuffer empty;
    arp.process(empty,0,1<<25,settings,120,[](const auto&,int){});
    settings.enabled=true;settings.beat=0;settings.gate=127;
    auto input=held({60},13);input.addEvent(synth2::audio::MidiMessage::noteOff(1,60),37);
    Events events;
    arp.process(input,0,64,settings,120,[&](const auto& message,int frame){capture(events,message,frame);});
    const auto on=notes(events),off=notes(events,false);
    ASSERT_EQ(on.size(),1u);ASSERT_EQ(off.size(),1u);
    EXPECT_EQ(std::get<0>(on[0]),13);EXPECT_EQ(std::get<0>(off[0]),37);
}

TEST(Arpeggiator, RandomConsumesCallerStreamAndKeepsFirstKeyVelocity)
{
    synth2::dsp::CrtRandom random;
    synth2::midi::Arpeggiator arp(random);arp.prepare(1000);
    auto p=defaults();p.type=4;p.range=1;p.gate=127;
    synth2::audio::MidiBuffer input;
    input.addEvent(synth2::audio::MidiMessage(0x90,60,31),0);
    input.addEvent(synth2::audio::MidiMessage(0x90,64,121),0);
    auto expected=random;expected.next();expected.next();
    Events events;
    arp.process(input,0,1,p,120,[&](const auto& msg,int frame){capture(events,msg,frame);});
    EXPECT_EQ(random.state,expected.state);
    synth2::audio::MidiBuffer empty;
    arp.process(empty,0,1000,p,120,[&](const auto& msg,int frame){capture(events,msg,frame+1);});
    const auto on=notes(events);ASSERT_GT(on.size(),4u);
    for(const auto& event:on)EXPECT_EQ(std::get<3>(event),31);
}
