#include <gtest/gtest.h>
#include "dsp/Filter.h"
#include <cmath>

TEST(FilterPath, SelectedCompatibleKernelKeepsTheSameStateAcrossResonance112)
{
    for(double rate:{44100.,48000.,96000.})for(int mode=0;mode<=4;++mode)
    {
        synth2::dsp::Filter selected,reference;
        selected.prepare({static_cast<float>(rate),64,1});reference.prepare({static_cast<float>(rate),64,1});
        for(int frame=0;frame<4096;++frame)
        {
            const float dial=110.+static_cast<double>(frame%512)/128.;
            const float envelope=static_cast<float>(std::sin(frame*.031))*.25f;
            const float input=static_cast<float>(std::sin(frame*.07))*.01f;
            for(auto* filter:{&selected,&reference})filter->setParams(mode,400.25,dial/127.,.25,.5,.125);
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
        synth2::dsp::Filter seed;seed.prepare({static_cast<float>(rate),64,1});
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
        synth2::dsp::Filter filter;filter.prepare({static_cast<float>(rate),64,1});
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

TEST(FilterPath, CoefficientPreparationPreservesAnalyticPrecisionBeforeFloatStorage)
{
    for(int rate:{8000,44100,48000,96000})for(float index:{0.f,63.5f,333.25f,511.f,900.f,1023.f})
    {
        SCOPED_TRACE(::testing::Message()<<"rate="<<rate<<" index="<<index);
        const float frequency=static_cast<float>(15.L*std::exp(std::log(1066.6666666666667L)*(index/1024.L)));
        const float f=std::min(frequency*2.f/rate,1.f);
        const float limit=std::nextafter(1.f,0.f);
        const float expectedPole=std::clamp(static_cast<float>(3.6L*f-1.6L*f*f-1.L),-limit,limit);
        const float square=(f*2.5f)*f;
        const float expectedFeedback=static_cast<float>(std::exp(1.386249L*(1.L-(4.3L*f-square)/2.L)));
        synth2::dsp::Lp24Kernel lp;
        lp.prepare(rate);lp.configure(index,1.f);
        EXPECT_EQ(lp.getA(),expectedPole);
        EXPECT_EQ(lp.getFeedback(),expectedFeedback);
        if(index<=511)
        {
            synth2::dsp::StateVariableFilter svf;
            for(float resonance:{0.f,.125f,.7f,1.f})
            {
                svf.configure(static_cast<float>(rate),index,resonance);
                const float expectedStep=static_cast<float>(2.L*std::sin(static_cast<long double>(frequency)*3.1415926535897932384626433832795L/(rate*2.L)));
                EXPECT_EQ(svf.getCoefficient(),expectedStep);
                EXPECT_EQ(svf.getGain(),static_cast<float>(1.L-.7L*resonance));
            }
        }
    }
}

TEST(FilterPath, DiodeRetainsSubFloatImpulseResponseState)
{
    synth2::dsp::DiodeKernel diode;
    diode.configure(44100,128,.5f);
    // A normal Float32 input produces smaller internal ladder stages.
    diode.process(std::numeric_limits<float>::min());
    bool retained=false;
    for(auto value:diode.readState())
    {
        EXPECT_TRUE(std::isfinite(value));
        retained|=std::abs(value)>0 && std::abs(value)<static_cast<double>(std::numeric_limits<float>::denorm_min());
    }
    EXPECT_TRUE(retained);
}
