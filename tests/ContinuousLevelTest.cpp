#include <gtest/gtest.h>
#include "dsp/AmplitudeEnvelope.h"
#include "dsp/ReleaseRamp.h"
#include "dsp/Filter.h"
#include "dsp/FrequencyModulation.h"
#include <array>
#include <cmath>

TEST(ContinuousLevel, AttackRetainsSubLevelPrecisionAtWholeSettings)
{
    synth2::dsp::AmplitudeEnvelope envelope;
    envelope.setParameters(1000,1000,512,4410);envelope.noteOn();
    std::array<float,64> gains{};envelope.render(gains.data(),64);
    EXPECT_EQ(envelope.getLevel(),131.072f);
    EXPECT_NE(envelope.getLevel(),std::trunc(envelope.getLevel()));
    EXPECT_NEAR(gains.back(),.064f,1e-6f);
    envelope.render(gains.data(),64);EXPECT_GT(envelope.getLevel(),131.072);
}

TEST(ContinuousLevel, CompletedAttackPreservesReleaseTimingAndFinalSilence)
{
    synth2::dsp::AmplitudeEnvelope envelope;
    envelope.setParameters(1000,0,2048,4410);envelope.noteOn();
    std::array<float,1000> attack{};envelope.render(attack.data(),1000);
    EXPECT_EQ(envelope.getLevel(),2048);envelope.noteOff();
    std::array<float,64> gains{};
    for(int frames=0;envelope.isActive() && frames<4410+64;frames+=64)envelope.render(gains.data(),64);
    EXPECT_FALSE(envelope.isActive());EXPECT_EQ(envelope.getCounter(),3456);
    envelope.render(gains.data(),64);for(float sample:gains)EXPECT_EQ(sample,0);
}

TEST(ContinuousLevel, ReleaseKeepsTheExistingActiveWindow)
{
    // Existing 64-frame window endpoints, recorded from the preceding public
    // implementation before changing the level calculation.
    for(const auto [duration,end]:{std::pair{1000,832},std::pair{4410,3456},std::pair{16075,12416}})
    {
        SCOPED_TRACE(duration);
        synth2::dsp::ReleaseRamp release;release.start(duration,2048);
        for(int frames=0;release.isActive() && frames<duration+64;frames+=64)
            release.advance(64,[&](double level,int count) {
                EXPECT_GT(count,0);EXPECT_GE(level,0);EXPECT_LE(level,2048);
            });
        EXPECT_FALSE(release.isActive());
        EXPECT_EQ(release.getCounter(),end);EXPECT_EQ(release.getLevel(),0);
        int calls=0;release.advance(64,[&](double,int){++calls;});EXPECT_EQ(calls,0);
    }
}

TEST(ContinuousLevel, StaticFilterPassesFractionalCutoffToTheExistingNativeKernels)
{
    for(int mode=0;mode<5;++mode)
    {
        SCOPED_TRACE(mode);
        synth2::dsp::Filter wrapped,direct,neighbor;
        for(auto* filter:{&wrapped,&direct,&neighbor})filter->prepare({44100,64,2});
        wrapped.setParams(mode,512.25f,.25f,0,0,0);
        direct.setParams(mode,512.25f,.25f,0,0,0);
        neighbor.setParams(mode,512.5f,.25f,0,0,0);
        bool differs=false;
        for(int i=0;i<512;++i)
        {
            const float input=i==0 ? 1.0f : 0.0f;
            const float actual=wrapped.processSample(input,0);
            const float expected=direct.processNativeIndex(input,512.25);
            const float other=neighbor.processSample(input,0);
            EXPECT_TRUE(std::isfinite(actual));EXPECT_TRUE(std::isfinite(other));
            EXPECT_FLOAT_EQ(actual,expected);differs|=actual!=other;
        }
        EXPECT_TRUE(differs);
    }
}

TEST(ContinuousLevel, PhysicalFmSlewRetainsRealLevelsAndCompletesAtTheTarget)
{
    synth2::dsp::FrequencyModulation slew;
    slew.render(.125,1,1000,[](double){});
    EXPECT_DOUBLE_EQ(slew.current,.125);
    for(double target:{.375,-.25,.0625})
    {
        slew.setBase(1000);
        for(int frame=0;frame<101;++frame)
            slew.render(target,1,1000,[&](double value){EXPECT_TRUE(std::isfinite(value));});
        EXPECT_DOUBLE_EQ(slew.current,target);EXPECT_EQ(slew.remaining,0);
    }
}

TEST(ContinuousLevel, RepeatedAttackUsesWholeLevelRestartAnchorsWithoutRoundingAudio)
{
    for(bool vectorRamp:{false,true})
    {
        SCOPED_TRACE(vectorRamp);
        synth2::dsp::AmplitudeEnvelope envelope(vectorRamp);
        envelope.setParameters(100000,1000,512,1000);envelope.noteOn();
        alignas(16) std::array<float,64> gains{};
        // floor(100000*n/2048) for successive whole restart levels 1..6.
        for(int expectedCounter:{48,97,146,195,244,292})
        {
            envelope.render(gains.data(),64);
            const double level=envelope.getLevel();
            ASSERT_NE(level,std::trunc(level));
            const auto gainState=envelope.readGainState();
            envelope.noteOn();
            EXPECT_EQ(envelope.getCounter(),expectedCounter);
            EXPECT_DOUBLE_EQ(envelope.getLevel(),level);
            EXPECT_EQ(envelope.readGainState(),gainState);
        }
        envelope.reset();envelope.noteOn();
        EXPECT_EQ(envelope.getCounter(),0);EXPECT_EQ(envelope.getLevel(),0);
    }
}

TEST(ContinuousLevel, ReleaseRetriggerKeepsRealLevelAndUsesCompatibleSamplePosition)
{
    synth2::dsp::AmplitudeEnvelope envelope;
    envelope.setParameters(0,0,512.25,1000);envelope.noteOn();
    std::array<float,64> gains{};envelope.render(gains.data(),64);
    envelope.noteOff();envelope.render(gains.data(),1);
    ASSERT_DOUBLE_EQ(envelope.getLevel(),507.25);
    envelope.setParameters(100000,0,512.25,1000);
    envelope.noteOn();
    EXPECT_EQ(envelope.getCounter(),24755); // floor(100000 * whole level 507 / 2048).
    EXPECT_DOUBLE_EQ(envelope.getLevel(),507.25);
    envelope.render(gains.data(),1);
    EXPECT_FLOAT_EQ(envelope.getLevel(),507.00288f);
    EXPECT_TRUE(envelope.isActive());
}
