#include <gtest/gtest.h>
#include "dsp/ChorusKernel.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace {
std::vector<double> render(int voices,float time,float depth,float rate,bool history=false)
{
    auto chorus=std::make_unique<synth2::dsp::ChorusKernel>();
    EXPECT_TRUE(chorus->configure(44100,voices,64,64,64,64,127));
    if(history)
    {
        chorus->setTimeDial(23.25f);chorus->setDepthDial(78.5f);chorus->setRateDial(54.75f);
        double impulse[2]{1,-.5f};chorus->process(impulse,nullptr,1);
    }
    chorus->setTimeDial(time);chorus->setDepthDial(depth);chorus->setRateDial(rate);chorus->reset();
    std::vector<double> audio(16384);
    for(size_t i=0;i<audio.size()/2;++i)
    {
        audio[2*i]=static_cast<float>(std::sin(i*.07));
        audio[2*i+1]=static_cast<float>(std::cos(i*.11))*.5f;
    }
    for(int i=0;i<8192;i+=64)chorus->process(audio.data()+2*i,nullptr,64);
    return audio;
}
}

TEST(ChorusPath, ResetMakesIntegralAndFractionalEditHistoryEquivalent)
{
    for(int voices:{1,2,4})for(float dial:{64.f,64.25f})
        EXPECT_EQ(render(voices,dial,dial,dial),render(voices,dial,dial,dial,true));
}

TEST(ChorusPath, CoordinatesRemainContinuousAcrossIntegralDials)
{
    constexpr float epsilon=.00001f;
    for(int voices:{1,2,4})for(int control=0;control<3;++control)
    {
        float lower[]{64,64,64},upper[]{64,64,64};
        lower[control]-=epsilon;upper[control]+=epsilon;
        const auto exact=render(voices,64,64,64);
        for(const auto& audio:{render(voices,lower[0],lower[1],lower[2]),render(voices,upper[0],upper[1],upper[2])})
        {
            double maximum=0;
            for(size_t i=0;i<audio.size();++i)maximum=std::max(maximum,std::abs(audio[i]-exact[i]));
            EXPECT_LT(maximum,.001f)<<"voices="<<voices<<" control="<<control;
        }
    }
}

TEST(ChorusPath, ZeroDepthKeepsExactIntegralAndFractionalImpulseDelays)
{
    for(float dial:{64.f,64.125f})
    {
        auto chorus=std::make_unique<synth2::dsp::ChorusKernel>();
        ASSERT_TRUE(chorus->configure(44100,1,64,0,64,64,127));
        chorus->setTimeDial(dial);chorus->reset();
        std::vector<double> audio(1400);audio[0]=1;
        chorus->process(audio.data(),nullptr,700);
        for(int i=1;i<700;++i)
        {
            const float expected=dial==64 ? (i==666 ? 1.f : 0.f)
                : i==667 ? .625f : i==668 ? .375f : 0.f;
            EXPECT_EQ(audio[2*i],expected);EXPECT_EQ(audio[2*i+1],0);
        }
    }
}
