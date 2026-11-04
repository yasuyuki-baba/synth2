#include <gtest/gtest.h>
#include "dsp/Filter.h"
#include <cmath>

TEST(FilterPath, SelectedCompatibleKernelKeepsTheSameStateAcrossResonance112)
{
    for(double rate:{44100.,48000.,96000.})for(int mode=0;mode<=4;++mode)
    {
        synth2::dsp::Filter selected,reference;
        selected.prepare({rate,64,1});reference.prepare({rate,64,1});
        for(int frame=0;frame<4096;++frame)
        {
            const float dial=110.f+static_cast<float>(frame%512)/128.f;
            const float envelope=static_cast<float>(std::sin(frame*.031))*.25f;
            const float input=static_cast<float>(std::sin(frame*.07))*.01f;
            for(auto* filter:{&selected,&reference})filter->setParams(mode,400.25f,dial/127.f,.25f,.5f,.125f);
            const float index=std::clamp(400.25f+1023.f*envelope*.25f+.5f,0.f,1023.f);
            EXPECT_EQ(selected.processSample(input,envelope),reference.processNativeIndex(input,index));
            if(frame%64==63){selected.finishNativeBlock();reference.finishNativeBlock();}
        }
        EXPECT_EQ(selected.readNativeMemory(),reference.readNativeMemory());
    }
}

TEST(FilterPath, CompatibleResonanceEditsAreContinuousAround112)
{
    for(double rate:{44100.,48000.,96000.})for(int mode=0;mode<=4;++mode)
    {
        synth2::dsp::Filter seed;seed.prepare({rate,64,1});
        seed.setParams(mode,400.25f,112.f/127,0,0,0);
        for(int frame=0;frame<64;++frame)seed.processSample(frame==0 ? .5f : 0.f,0);
        auto exact=seed,lower=seed,upper=seed;
        lower.setParams(mode,400.25f,111.9999f/127,0,0,0);
        upper.setParams(mode,400.25f,112.0001f/127,0,0,0);
        const float expected=exact.processSample(.01f,0);
        EXPECT_NEAR(lower.processSample(.01f,0),expected,.00001f);
        EXPECT_NEAR(upper.processSample(.01f,0),expected,.00001f);
    }
}

TEST(FilterPath, CoordinateAdaptersShareOneHistoryAcrossParameterChanges)
{
    for(int mode=0;mode<=4;++mode)
    {
        synth2::dsp::Filter alternating,reference;
        alternating.prepare({44100,64,1});reference.prepare({44100,64,1});
        for(int frame=0;frame<1024;++frame)
        {
            const float cutoff=400.25f+(frame%64)*.125f;
            const float resonance=(frame%128)/127.f;
            for(auto* filter:{&alternating,&reference})filter->setParams(mode,cutoff,resonance,0,0);
            const float input=frame%37==0 ? .01f : 0.f;
            const float actual=frame%2 ? alternating.processNativeIndex(input,cutoff) : alternating.processSample(input,0);
            EXPECT_EQ(actual,reference.processNativeIndex(input,cutoff));
        }
        EXPECT_EQ(alternating.readNativeMemory(),reference.readNativeMemory());
        alternating.reset();
        for(int frame=0;frame<64;++frame)EXPECT_EQ(alternating.processSample(0,0),0);
    }
}

TEST(FilterPath, LowRateCutoffSweepsStayFiniteWithoutChangingKernels)
{
    for(double rate:{8000.,16000.,22050.})for(int mode=0;mode<=4;++mode)
    for(float resonance:{0.f,.5f,1.f})
    {
        synth2::dsp::Filter filter;filter.prepare({rate,64,1});
        for(int frame=0;frame<12000;++frame)
        {
            const float index=(frame%2048)*(1023.f/2047.f);
            filter.setParams(mode,index,resonance,0,0);
            const float output=filter.processSample(frame%97==0 ? .5f : 0.f,0);
            ASSERT_TRUE(std::isfinite(output)) << rate << " " << mode << " " << resonance;
            ASSERT_LT(std::abs(output),10.f) << rate << " " << mode << " " << resonance;
            if(frame%64==63)filter.finishNativeBlock();
        }
        filter.reset();
        for(int frame=0;frame<64;++frame)EXPECT_EQ(filter.processSample(0,0),0);
    }
}
