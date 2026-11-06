#include <gtest/gtest.h>
#include "dsp/OutputTone.h"
#include <algorithm>
#include <cmath>

TEST(OutputTonePrecision, FractionalEditsApproachIntegralSettingsContinuously)
{
    for(double rate:{22050.,44100.,48000.,96000.})for(int dial=1;dial<127;++dial)
    {
        synth2::dsp::OutputTone seed;seed.prepare(rate);seed.setDial(dial);
        for(int i=0;i<64;++i)seed.process(static_cast<float>(std::sin(i*.13)),i%2);
        auto exact=seed,lower=seed,upper=seed;
        lower.setFractionalDial(dial-.00001f);upper.setFractionalDial(dial+.00001f);
        float maximum=0;
        for(int i=0;i<128;++i)
        {
            const float input=static_cast<float>(std::cos(i*.17))*.5f;
            const float reference=exact.process(input,i%2);
            maximum=std::max({maximum,std::abs(lower.process(input,i%2)-reference),
                             std::abs(upper.process(input,i%2)-reference)});
        }
        EXPECT_LT(maximum,.00003f)<<"rate="<<rate<<" dial="<<dial;
    }
}

TEST(OutputTonePrecision, BypassRetainsHistoryWithoutConsumingInput)
{
    synth2::dsp::OutputTone tone;tone.prepare(22050);tone.setDial(0);
    for(int i=0;i<16;++i)tone.process(i==0 ? 1.f : 0.f,0);
    auto retained=tone;
    tone.setDial(64);ASSERT_TRUE(tone.isBypassed());
    for(int i=0;i<128;++i)EXPECT_EQ(tone.process(.25f,0),.25f);
    tone.setDial(0);
    for(int i=0;i<128;++i)EXPECT_EQ(tone.process(0,0),retained.process(0,0));
}

TEST(OutputTonePrecision, PreparePreservesFractionalSettingAndOptionalHistory)
{
    for(double rate:{22050.,44100.,96000.})for(float dial:{0.f,32.25f,63.875f,64.f,64.125f,96.5f,127.f})
    {
        synth2::dsp::OutputTone tone;tone.prepare(rate);tone.setFractionalDial(dial);
        for(int i=0;i<64;++i)tone.process(i==0 ? .5f : 0.f,i%2);
        auto retained=tone;tone.prepare(rate,true);
        for(int i=0;i<128;++i)EXPECT_EQ(tone.process(.1f,i%2),retained.process(.1f,i%2));
        tone.prepare(rate);
        synth2::dsp::OutputTone fresh;fresh.prepare(rate);fresh.setFractionalDial(dial);
        for(int i=0;i<128;++i)EXPECT_EQ(tone.process(i==0 ? 1.f : 0.f,i%2),fresh.process(i==0 ? 1.f : 0.f,i%2));
    }
}
