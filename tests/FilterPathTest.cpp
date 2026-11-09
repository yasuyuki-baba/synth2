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
            const double dial=110.+static_cast<double>(frame%512)/128.;
            const double envelope=static_cast<double>(std::sin(frame*.031))*.25;
            const double input=static_cast<double>(std::sin(frame*.07))*.01;
            for(auto* filter:{&selected,&reference})filter->setParams(mode,400.25,dial/127.,.25,.5,.125);
            const double index=std::clamp(400.25+1023.*envelope*.25+.5,0.,1023.);
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
        seed.setParams(mode,400.25,112./127,0,0,0);
        for(int frame=0;frame<64;++frame)seed.processSample(frame==0 ? .5 : 0.,0);
        auto exact=seed,lower=seed,upper=seed;
        lower.setParams(mode,400.25,111.9999/127,0,0,0);
        upper.setParams(mode,400.25,112.0001/127,0,0,0);
        const double expected=exact.processSample(.01,0);
        EXPECT_NEAR(lower.processSample(.01,0),expected,.00001);
        EXPECT_NEAR(upper.processSample(.01,0),expected,.00001);
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
            const double cutoff=400.25+(frame%64)*.125;
            const double resonance=(frame%128)/127.;
            for(auto* filter:{&alternating,&reference})filter->setParams(mode,cutoff,resonance,0,0);
            const double input=frame%37==0 ? .01 : 0.;
            const double actual=frame%2 ? alternating.processNativeIndex(input,cutoff) : alternating.processSample(input,0);
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
    for(double resonance:{0.,.5,1.})
    {
        synth2::dsp::Filter filter;filter.prepare({rate,64,1});
        for(int frame=0;frame<12000;++frame)
        {
            const double index=(frame%2048)*(1023./2047.);
            filter.setParams(mode,index,resonance,0,0);
            const double output=filter.processSample(frame%97==0 ? .5 : 0.,0);
            ASSERT_TRUE(std::isfinite(output)) << rate << " " << mode << " " << resonance;
            ASSERT_LT(std::abs(output),10.) << rate << " " << mode << " " << resonance;
            if(frame%64==63)filter.finishNativeBlock();
        }
        filter.reset();
        for(int frame=0;frame<64;++frame)EXPECT_EQ(filter.processSample(0,0),0);
    }
}
