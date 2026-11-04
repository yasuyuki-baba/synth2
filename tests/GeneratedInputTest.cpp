#include <gtest/gtest.h>
#include "../tools/host_scenarios.h"
#include <array>
#include <random>
#include <string>
#include <vector>

namespace
{
void setDial(std::string& text,int index,int value)
{
    const auto key="\n"+std::to_string(index)+",";
    const auto start=text.find(key);
    if(start==std::string::npos) throw std::runtime_error("Missing generated source field");
    const auto end=text.find('\n',start+1);
    text.replace(start+1,end-start-1,std::to_string(index)+","+std::to_string(value));
}
void checkAudio(const std::array<float,259>& left,const std::array<float,259>& right,int frames)
{
    EXPECT_EQ(left.front(),7); EXPECT_EQ(right.front(),7);
    for(int i=1;i<=frames;++i)
    { ASSERT_TRUE(std::isfinite(left[i])); ASSERT_TRUE(std::isfinite(right[i])); }
    for(int i=frames+1;i<259;++i) { EXPECT_EQ(left[i],7); EXPECT_EQ(right[i],7); }
}
}

TEST(GeneratedInput, BoundedPatchAndMidiCombinationsRemainFiniteAndPreserveState)
{
    // Fixed input generation makes failures reproducible; it does not choose or
    // alter the engine RNG stream. This is a bounded matrix, not exhaustive fuzzing.
    std::mt19937 inputs(20261027);
    for(double rate:{44100.,48000.25,96000.}) for(int scenario=0;scenario<12;++scenario)
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(scenario);
        auto text=hostvalidation::patch(1+scenario%4,scenario%3);
        setDial(text,0,inputs()%5); setDial(text,1,inputs()%4);
        for(int index:{2,3,5,14,15,21,22,23,24,25,29}) setDial(text,index,inputs()%128);
        // Select valid filter choices; envelopes keep the helper's short release.
        setDial(text,14,inputs()%4);
        synth2::host::ProcessorBridge processor;
        std::string error;
        ASSERT_TRUE(processor.loadPatch(text,error))<<error;
        processor.prepare(rate,257);
        const auto state=processor.saveState();
        for(int block=0;block<40;++block)
        {
            const int frames=std::array<int,5>{1,63,64,127,257}[block%5];
            std::array<float,259> left,right; left.fill(7); right.fill(7);
            const int key=36+inputs()%49;
            const uint8_t channel=inputs()%16;
            ASSERT_TRUE(processor.enqueueMidi(0,0x90|channel,key,1+inputs()%127));
            ASSERT_TRUE(processor.enqueueMidi(frames-1,0xe0|channel,inputs()%128,inputs()%128));
            ASSERT_TRUE(processor.enqueueMidi(frames-1,0xb0|channel,64,(block%2)*127));
            ASSERT_TRUE(processor.enqueueMidi(frames-1,0x80|channel,key,0));
            ASSERT_TRUE(processor.process(left.data()+1,right.data()+1,frames,137));
            checkAudio(left,right,frames);
            EXPECT_EQ(processor.saveState(),state);
        }
        // Native input is omni: channel stops must clear the shared keyboard.
        for(int c=0;c<16;++c) ASSERT_TRUE(processor.enqueueMidi(0,0xb0|c,123,0));
        double finalEnergy=0;
        for(int block=0;block<static_cast<int>(rate*2/257)+1;++block)
        {
            std::array<float,259> left,right; left.fill(7); right.fill(7);
            ASSERT_TRUE(processor.process(left.data()+1,right.data()+1,257,120));
            checkAudio(left,right,257);
            finalEnergy=0;
            for(int i=1;i<=257;++i) finalEnergy+=double(left[i])*left[i]+double(right[i])*right[i];
        }
        EXPECT_LT(std::sqrt(finalEnergy/(2*257)),1e-5);
    }
}

TEST(GeneratedInput, GeneratedMalformedSourceTokensCannotReplaceValidProgram)
{
    synth2::host::ProcessorBridge processor;
    std::string error;
    ASSERT_TRUE(processor.loadProgram(hostvalidation::patch(),5,error));
    const auto before=processor.saveState();
    for(int i=0;i<64;++i)
    {
        SCOPED_TRACE(i);
        const auto token=std::to_string(i)+"broken";
        for(const auto& entry:{"29,"+token,token+",1","29,999999999999999999999"+token})
        {
            EXPECT_FALSE(processor.loadProgram("Malformed\nver=113\n"+entry+"\n",3,error));
            EXPECT_EQ(processor.saveState(),before);
            EXPECT_EQ(processor.currentProgram(),5);
        }
    }
}

TEST(GeneratedInput, TruncatedStatePayloadsCannotPartiallyReplaceParameters)
{
    synth2::host::ProcessorBridge processor,donor;
    std::string error;
    ASSERT_TRUE(processor.loadProgram(hostvalidation::patch(),5,error));
    ASSERT_TRUE(donor.loadProgram("Different source\nver=101\n19,33\n29,12\n",2,error));
    const auto before=processor.saveState(),incoming=donor.saveState();
    // Exclude omission of only the optional trailing NUL: it is not truncation
    // of the length-delimited XML payload.
    for(size_t size=0;size+1<incoming.size();++size)
    {
        SCOPED_TRACE(size);
        processor.restoreState(incoming.data(),static_cast<int>(size));
        EXPECT_EQ(processor.saveState(),before);
        EXPECT_EQ(processor.currentProgram(),5);
    }
}
