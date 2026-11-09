#include <gtest/gtest.h>
#include "dsp/VoiceEnvelopeConfigurator.h"
#include <cmath>

namespace {
synth2::dsp::FilterEnvelope attacking()
{
    synth2::dsp::FilterEnvelope envelope;
    envelope.configure(100,1000,100,0,10000);
    envelope.noteOn();envelope.render(10,[](double,int){});
    return envelope;
}
}

TEST(FilterEnvelopePrecision, PeakRescaleIsContinuousAcrossIntegerCoordinates)
{
    for(double peak:{103-1e-9,103.0,103+1e-9})
    {
        auto envelope=attacking();
        ASSERT_EQ(envelope.getLevel(),10);
        const auto phase=envelope.getPhase();const auto counter=envelope.getCounter();
        envelope.configure(100,1000,peak,0,10000);
        EXPECT_NEAR(envelope.getLevel()/peak,.1,1e-14);
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
    for(double peak:{.5,50.,103.-1e-9,103.,103.+1e-9,200.25})
    {
        SCOPED_TRACE(peak);
        auto reference=attacking();
        // Capture a fractional level at note-off, independently of the edit.
        reference.configure(100,1000,103.25,0,10000);reference.noteOff();
        auto edited=reference;
        edited.configure(100,1000,peak,0,10000);
        EXPECT_NEAR(edited.getLevel()/peak,.1,1e-14);
        for(int count:{1,17,64,127})
        {
            reference.render(count,[](double,int){});edited.render(count,[](double,int){});
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
    envelope.render(1,[](double,int){});
    EXPECT_EQ(envelope.getLevel(),0);EXPECT_TRUE(std::isfinite(envelope.getLevel()));
}

TEST(FilterEnvelopePrecision, SustainEditsUseTheNewSustainWithoutRescaling)
{
    synth2::dsp::FilterEnvelope envelope;
    envelope.configure(0,0,100,40,1000);envelope.noteOn();envelope.render(1,[](double,int){});
    ASSERT_EQ(envelope.getPhase(),synth2::dsp::FilterEnvelope::Phase::Sustain);
    envelope.configure(0,0,103.25,41.125,1000);EXPECT_EQ(envelope.getLevel(),41.125);
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
            const float depth=std::abs(std::clamp(settings.amount,-.5,.5))*1024.0f;
            const double expected=std::trunc(depth*settings.sustain);
            FilterEnvelope envelope;double keyboard=0;
            VoiceEnvelopeConfigurator::filter(envelope,keyboard,0,settings,48,1,44100);
            envelope.noteOn();envelope.render(1,[](double,int){});
            EXPECT_EQ(keyboard,0);EXPECT_EQ(envelope.getLevel(),expected);
        }
}

TEST(FilterEnvelopePrecision, RetriggerKeepsRealLevelAndUsesDiscreteRestartAnchor)
{
    for(bool release:{false,true})for(double peak:{103.,103.25})
    {
        auto envelope=attacking();envelope.configure(100,1000,peak,0,10000);
        if(release)envelope.noteOff();
        const double level=envelope.getLevel();ASSERT_GT(level,std::trunc(level));
        envelope.noteOn();EXPECT_EQ(envelope.getLevel(),level);
        EXPECT_EQ(envelope.getCounter(),static_cast<int>(100*std::trunc(level)/peak));
        envelope.configure(100,1000,0,0,10000);envelope.noteOn();
        EXPECT_EQ(envelope.getCounter(),0);EXPECT_EQ(envelope.getLevel(),0);
    }
}
