#include <gtest/gtest.h>
#include "host/ProcessorBridge.h"
#include <array>
#include <sstream>
using synth2::host::ProcessorBridge;

TEST(ProcessorContracts, RecallEditsAndRejectedInputsPreserveState)
{
    ProcessorBridge source;std::string error;ASSERT_TRUE(source.loadPatch("Named patch\nver=113\n0,0\n19,80\n43,22\n",error));
    source.setNormalizedValue(43,.75);source.setNormalizedValue(128,.3);const auto state=source.saveState();
    ProcessorBridge restored;restored.restoreState(state.data(),static_cast<int>(state.size()));
    EXPECT_EQ(source.saveState(),restored.saveState());
    ASSERT_FALSE(restored.loadPatch("Invalid\nver=900\n",error));EXPECT_EQ(restored.saveState(),state);
    auto bad=state;bad[4]=255;bad[5]=255;bad[6]=255;bad[7]=255;
    restored.restoreState(bad.data(),static_cast<int>(bad.size()));EXPECT_EQ(restored.saveState(),state);
    restored.restoreState(nullptr,0);EXPECT_EQ(restored.saveState(),state);
}

TEST(ProcessorContracts, DoubleOutputAndMidiBoundsRemainDeterministic)
{
    ProcessorBridge a,b;a.prepare(48000,64);b.prepare(48000,64);
    ASSERT_TRUE(a.enqueueMidi(7,0x90,69,100));ASSERT_TRUE(b.enqueueMidi(7,0x90,69,100));
    std::array<float,64> l{},r{};std::array<double,64> dl{},dr{};
    for(int block=0;block<8;++block)
    {ASSERT_TRUE(a.process(l.data(),r.data(),64,90));ASSERT_TRUE(b.process(dl.data(),dr.data(),64,90));for(int i=0;i<64;++i){EXPECT_EQ(static_cast<float>(dl[i]),l[i]);EXPECT_EQ(static_cast<float>(dr[i]),r[i]);}}
    EXPECT_FALSE(a.enqueueMidi(-1,0x90,60,100));EXPECT_FALSE(a.enqueueMidi(0,0xf0,0,0));
    EXPECT_EQ(a.droppedMidiMessages(),2u);
    std::array<float,65> largeL{},largeR{};largeL.fill(1);largeR.fill(1);EXPECT_FALSE(a.process(largeL.data(),largeR.data(),65,120));
    for(float value:largeL)EXPECT_EQ(value,0);for(float value:largeR)EXPECT_EQ(value,0);
}
