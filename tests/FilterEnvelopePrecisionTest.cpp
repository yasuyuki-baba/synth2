#include <gtest/gtest.h>
#include "dsp/VoiceEnvelopeConfigurator.h"
#include <cmath>

namespace {
synth2::dsp::FilterEnvelope attacking()
{
    synth2::dsp::FilterEnvelope envelope;
    envelope.configure(100,1000,100,0,10000);
    envelope.noteOn();envelope.render(10,[](float,int){});
    return envelope;
}
}

TEST(FilterEnvelopePrecision, PeakRescaleIsContinuousAcrossIntegerCoordinates)
{
    for(float peak:{103-1e-9f,103.0f,103+1e-9f})
    {
        auto envelope=attacking();
        ASSERT_EQ(envelope.getLevel(),10);
        const auto phase=envelope.getPhase();const auto counter=envelope.getCounter();
        envelope.configure(100,1000,peak,0,10000);
        EXPECT_NEAR(envelope.getLevel()/peak,.1f,1e-14f);
        EXPECT_EQ(envelope.getPhase(),phase);EXPECT_EQ(envelope.getCounter(),counter);
    }
}

TEST(FilterEnvelopePrecision, ExactRatiosAndSamePeakKeepState)
{
    auto envelope=attacking();
    envelope.configure(100,1000,200,0,10000);EXPECT_EQ(envelope.getLevel(),20);
    const auto counter=envelope.getCounter();
    envelope.configure(100,1000,200,0,10000);
    EXPECT_EQ(envelope.getLevel(),20);EXPECT_EQ(envelope.getCounter(),counter);
    envelope.configure(100,1000,100,0,10000);EXPECT_EQ(envelope.getLevel(),10);
}

TEST(FilterEnvelopePrecision, ReleasePeakEditsKeepTheCapturedRealTrajectory)
{
    using Phase=synth2::dsp::FilterEnvelope::Phase;
    for(float peak:{.5f,50.f,103.f-1e-9f,103.f,103.f+1e-9f,200.25f})
    {
        SCOPED_TRACE(peak);
        auto reference=attacking();
        // Capture a fractional level at note-off, independently of the edit.
        reference.configure(100,1000,103.25f,0,10000);reference.noteOff();
        auto edited=reference;
        edited.configure(100,1000,peak,0,10000);
        EXPECT_NEAR(edited.getLevel()/peak,.1f,1e-14f);
        for(int count:{1,17,64,127})
        {
            reference.render(count,[](float,int){});edited.render(count,[](float,int){});
            EXPECT_DOUBLE_EQ(edited.getLevel(),reference.getLevel());
            EXPECT_EQ(edited.getCounter(),reference.getCounter());
            EXPECT_EQ(edited.getBoundary(),reference.getBoundary());
            EXPECT_EQ(edited.getStep(),reference.getStep());
            EXPECT_EQ(edited.getPhase(),Phase::Release);
        }
    }
}

TEST(FilterEnvelopePrecision, ZeroPeakCannotRestoreAnOldReleaseLevel)
{
    auto envelope=attacking();envelope.noteOff();
    envelope.configure(100,1000,0,0,10000);
    EXPECT_EQ(envelope.getLevel(),0);
    envelope.configure(100,1000,200,0,10000);
    envelope.render(1,[](float,int){});
    EXPECT_EQ(envelope.getLevel(),0);EXPECT_TRUE(std::isfinite(envelope.getLevel()));
}

TEST(FilterEnvelopePrecision, SustainEditsUseTheNewSustainWithoutRescaling)
{
    synth2::dsp::FilterEnvelope envelope;
    envelope.configure(0,0,100,40,1000);envelope.noteOn();envelope.render(1,[](float,int){});
    ASSERT_EQ(envelope.getPhase(),synth2::dsp::FilterEnvelope::Phase::Sustain);
    envelope.configure(0,0,103.25f,41.125f,1000);EXPECT_EQ(envelope.getLevel(),41.125f);
}

TEST(FilterEnvelopePrecision, CommonConfiguratorPreservesIntegralSourceNormalization)
{
    using namespace synth2::dsp;
    for(int amount:{0,32,63,64,79,95,127})for(int dial=0;dial<=127;++dial)
        for(bool multiply:{false,true})
        {
            FilterEnvelopeSettings settings;settings.cutoffIndex=512;
            settings.amount=(amount-63.0f)/64.0f;
            settings.sustain=multiply ? dial*(1.0f/127.0f) : dial/127.0f;
            // With zero Attack/Decay the first sample exposes the configured sustain.
            const float depth=std::abs(std::clamp(settings.amount,-.5f,.5f))*1024.0f;
            const float expected=std::trunc(depth*settings.sustain);
            FilterEnvelope envelope;float keyboard=0;
            VoiceEnvelopeConfigurator::filter(envelope,keyboard,0,settings,48,1,44100);
            envelope.noteOn();envelope.render(1,[](float,int){});
            EXPECT_EQ(keyboard,0);EXPECT_EQ(envelope.getLevel(),expected);
        }
}

TEST(FilterEnvelopePrecision, RetriggerKeepsRealLevelAndUsesDiscreteRestartAnchor)
{
    for(bool release:{false,true})for(float peak:{103.f,103.25f})
    {
        auto envelope=attacking();envelope.configure(100,1000,peak,0,10000);
        if(release)envelope.noteOff();
        const float level=envelope.getLevel();ASSERT_GT(level,std::trunc(level));
        envelope.noteOn();EXPECT_EQ(envelope.getLevel(),level);
        EXPECT_EQ(envelope.getCounter(),static_cast<int>(100*std::trunc(level)/peak));
        envelope.configure(100,1000,0,0,10000);envelope.noteOn();
        EXPECT_EQ(envelope.getCounter(),0);EXPECT_EQ(envelope.getLevel(),0);
    }
}
