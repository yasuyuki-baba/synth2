#include "support/StateDocument.h"
#include <gtest/gtest.h>
#include "host/ProcessorBridge.h"
#include "../tools/host_scenarios.h"
#include <limits>
#include <map>
#include <cstring>

using synth2::host::ProcessorBridge;
namespace
{
void processAtTempo(ProcessorBridge& p,synth2::audio::AudioBuffer<float>& audio,const synth2::audio::MidiBuffer& midi,double tempo) {
    for(const auto& e:midi){const auto* bytes=e.getMessage().getRawData();ASSERT_TRUE(p.enqueueMidi(e.samplePosition,bytes[0],bytes[1],bytes[2]));}
    ASSERT_TRUE(p.process(audio.getWritePointer(0),audio.getWritePointer(1),audio.getNumSamples(),tempo));
}

void forward(ProcessorBridge& bridge, const synth2::audio::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const auto* bytes = message.getRawData();
        ASSERT_TRUE(bridge.enqueueMidi(metadata.samplePosition, bytes[0], bytes[1],
                                      message.getRawDataSize() == 3 ? bytes[2] : 0));
    }
}
void equal(const synth2::audio::AudioBuffer<float>& direct, const synth2::audio::AudioBuffer<float>& bridged)
{
    for (int channel=0; channel<2; ++channel)
        EXPECT_EQ(std::memcmp(direct.getReadPointer(channel), bridged.getReadPointer(channel),
                             direct.getNumSamples()*sizeof(float)), 0);
}
}





TEST(ProcessorBridge, RecallAndSameValueEditsRetainLoadingHistory)
{
    auto donor=hostvalidation::processor(); hostvalidation::load(*donor,hostvalidation::patch());
    const auto saved=hostvalidation::save(*donor);
    auto direct=hostvalidation::processor(); hostvalidation::restore(*direct,saved);
    ProcessorBridge bridge; bridge.restoreState(saved.data(),static_cast<int>(saved.size()));
    direct->prepare(48000,257); bridge.prepare(48000,257);
    for (int frames : {1,17,64,257,63})
    {
        for (int index : {41,42,43,44,9,86,128})
        {
            direct->setNormalizedValue(index,direct->normalizedValue(index));bridge.setNormalizedValue(index,bridge.normalizedValue(index));
        }
        synth2::audio::AudioBuffer<float> a(2,frames),b(2,frames); synth2::audio::MidiBuffer midi;
        midi.addEvent(synth2::audio::MidiMessage::noteOn(1,60,static_cast<uint8_t>(100)),0);
        midi.addEvent(synth2::audio::MidiMessage::pitchWheel(1,10000),frames-1);
        forward(bridge,midi); hostvalidation::process(*direct,a,midi);
        ASSERT_TRUE(bridge.process(b.getWritePointer(0),b.getWritePointer(1),frames,120)); equal(a,b);
    }
}

TEST(ProcessorBridge, DoubleOutputWidensFloatAndTempoIsForwarded)
{
    auto direct=hostvalidation::processor();
    ProcessorBridge bridge; std::string error;
    const auto text=hostvalidation::patch()+"59,1\n31,2\n57,64\n58,64\n";
    hostvalidation::load(*direct,text); ASSERT_TRUE(bridge.loadPatch(text,error));
    direct->prepare(48000,64); bridge.prepare(48000,64);
    for (int block=0; block<100; ++block)
    {
        synth2::audio::AudioBuffer<float> a(2,64); synth2::audio::MidiBuffer midi;
        if (block==0) hostvalidation::notes(midi,3);
        forward(bridge,midi); processAtTempo(*direct,a,midi,137.);
        std::array<double,64> left{},right{};
        ASSERT_TRUE(bridge.process(left.data(),right.data(),64,137));
        for (int i=0; i<64; ++i) { EXPECT_EQ(left[i],static_cast<double>(a.getSample(0,i))); EXPECT_EQ(right[i],static_cast<double>(a.getSample(1,i))); }
    }
}

TEST(ProcessorBridge, RejectsInvalidBlocksWithoutSplittingAndKeepsZeroFrameMidi)
{
    ProcessorBridge bridge;
    std::array<float,65> left,right; left.fill(1); right.fill(1);
    EXPECT_FALSE(bridge.process(left.data(),right.data(),64,120));
    EXPECT_EQ(left[0],0); EXPECT_EQ(right[63],0);
    bridge.prepare(44100,64);
    ASSERT_TRUE(bridge.enqueueMidi(0,0x90,60,100));
    ASSERT_TRUE(bridge.process(static_cast<float*>(nullptr),static_cast<float*>(nullptr),0,120));
    ASSERT_TRUE(bridge.process(left.data(),right.data(),0,120));
    ASSERT_TRUE(bridge.process(left.data(),right.data(),64,120));
    EXPECT_GT(*std::max_element(left.begin(),left.begin()+64),0);
    EXPECT_FALSE(bridge.process(left.data(),right.data(),65,120));
    EXPECT_EQ(left[64],0);
    EXPECT_FALSE(bridge.enqueueMidi(-1,0x90,60,100));
    EXPECT_FALSE(bridge.enqueueMidi(0,0xf0,0,0));
    EXPECT_FALSE(bridge.enqueueMidi(0,0x90,128,0));
    EXPECT_EQ(bridge.droppedMidiMessages(),3u);
}

TEST(ProcessorBridge, MidiOverflowReleasesHeldNotesAndRemainsBounded)
{
    ProcessorBridge bridge; std::string error;
    ASSERT_TRUE(bridge.loadPatch(hostvalidation::patch(),error));
    bridge.prepare(44100,64); std::array<float,64> left{},right{};
    ASSERT_TRUE(bridge.enqueueMidi(0,0x90,60,100));
    ASSERT_TRUE(bridge.process(left.data(),right.data(),64,120));
    for (std::size_t i=0; i<ProcessorBridge::midiCapacity; ++i) ASSERT_TRUE(bridge.enqueueMidi(0,0xb0,1,0));
    EXPECT_FALSE(bridge.enqueueMidi(0,0x80,60,0));
    EXPECT_EQ(bridge.droppedMidiMessages(),1u);
    for (int block=0; block<1500; ++block) ASSERT_TRUE(bridge.process(left.data(),right.data(),64,120));
    for (float sample:left) EXPECT_EQ(sample,0);
    for (float sample:right) EXPECT_EQ(sample,0);
}

TEST(ProcessorBridge, PublicParameterIdentityAndSourceBounds)
{
    ProcessorBridge bridge;
    EXPECT_EQ(bridge.parameter(9).minimum,-24);
    EXPECT_EQ(bridge.parameter(86).maximum,65535);
    EXPECT_EQ(bridge.parameter(128).id,"masterVolume");
}

TEST(ProcessorBridge, DelayTimingTelemetryUsesTheProcessedHostTempo)
{
    ProcessorBridge p;
    EXPECT_FLOAT_EQ(p.delayTiming().bpm,0);
    const auto edit=[&](int index,double value) {
        const auto& range=p.parameter(index);
        p.setNormalizedValue(index,(value-range.minimum)/(range.maximum-range.minimum));
    };
    edit(35,7);edit(83,64);edit(98,64);edit(65,1);
    p.prepare(48000,64);
    float left[64]{},right[64]{};
    ASSERT_TRUE(p.process(left,right,64,120));
    const auto timing=p.delayTiming();
    EXPECT_FLOAT_EQ(timing.bpm,120);
    EXPECT_NEAR(timing.leftMilliseconds,187.5f,.01f);
    EXPECT_NEAR(timing.rightMilliseconds,187.5f,.01f);
    ASSERT_TRUE(p.process(left,right,64,60));EXPECT_FLOAT_EQ(p.delayTiming().bpm,60);
    ASSERT_TRUE(p.process(left,right,64,std::numeric_limits<double>::quiet_NaN()));
    EXPECT_FLOAT_EQ(p.delayTiming().bpm,120);
}
