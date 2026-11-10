#include <gtest/gtest.h>
#include "dsp/Equalizer.h"
#include <cmath>

namespace
{
    float response (int tone, int level = 64, float frequency = 440.0f)
    {
        synth2::dsp::Equalizer eq;
        eq.prepare (44100.0f);
        synth2::EqualizerSettings p;
        p.tone = tone / 127.0f; p.level = level / 127.0f;
        p.freq = p.q = 64.0f / 127.0f;
        eq.setParams (p);
        float sum = 0.0f;
        for (int i = 0; i < 22050; ++i)
        {
            const float input = 0.001f * static_cast<float> (std::sin (synth2::audio::MathConstants<float>::twoPi * frequency * i / 44100.0f));
            const float output = eq.processSample (input);
            if (i >= 11025) sum += output * output;
        }
        return std::sqrt (sum / 11025) / (0.001f / std::sqrt (2.0f));
    }
}

TEST(Equalizer, StereoChannelsKeepIndependentFilterHistory)
{
    synth2::dsp::Equalizer eq;
    eq.prepare ({44100.0f, 256, 2});
    synth2::EqualizerSettings p; p.tone = 0.0f; p.level = 64.0f / 127.0f;
    eq.setParams (p);
    synth2::audio::AudioBuffer<float> buffer (2, 256); buffer.clear();
    buffer.setSample (0, 0, 1.0f);
    synth2::audio::AudioBlock<float> block (buffer);
    synth2::audio::ProcessContextReplacing<float> context (block);
    eq.process (context);
    EXPECT_GT(buffer.getMagnitude (0, 0, 256), 0.001f);
    EXPECT_FLOAT_EQ(buffer.getMagnitude (1, 0, 256), 0.0f);
    eq.reset(); buffer.clear();
    eq.process (context);
    EXPECT_FLOAT_EQ(buffer.getMagnitude (0, 256), 0.0f);
}

TEST(Equalizer, ParametricBandAndToneHaveIndependentControls)
{
    const float neutral = response (64, 64, 915.0f);
    EXPECT_NEAR(neutral, 1.0f, 0.002f);
    EXPECT_LT(neutral, 1.0f);
    EXPECT_GT(response (64, 96, 915.0f), neutral * 2.0f);
    EXPECT_LT(response (64, 32, 915.0f), neutral * 0.5f);
    EXPECT_LT(response (0, 64, 915.0f), neutral * 0.3f);
}

TEST(Equalizer, ContextBypassPreservesDryAudioAndHistory)
{
    synth2::dsp::Equalizer eq;
    eq.prepare ({48000.0f, 64, 2});
    synth2::EqualizerSettings p; p.tone = 0.0f; eq.setParams (p);
    synth2::audio::AudioBuffer<float> audio (2, 64);
    for (int channel = 0; channel < 2; ++channel)
    for (int frame = 0; frame < 64; ++frame) audio.setSample (channel, frame, frame * 0.001f);
    const synth2::audio::AudioBuffer<float> original (audio);
    synth2::audio::AudioBlock<float> block (audio);
    synth2::audio::ProcessContextReplacing<float> context (block);
    context.isBypassed = true; eq.process (context);
    for (int channel = 0; channel < 2; ++channel)
    for (int frame = 0; frame < 64; ++frame)
        EXPECT_FLOAT_EQ(audio.getSample(channel, frame), original.getSample(channel, frame));
    synth2::dsp::Equalizer fresh; fresh.prepare({48000.0f,64,2}); fresh.setParams(p);
    context.isBypassed=false; eq.process(context);
    for(int channel=0;channel<2;++channel)for(int frame=0;frame<64;++frame)
        EXPECT_EQ(audio.getSample(channel,frame),fresh.processSample(original.getSample(channel,frame),channel));
}

TEST(Equalizer, ParameterEndpointsRemainFiniteAcrossSampleRates)
{
    for (float rate : {22050.0f, 44100.0f, 48000.0f, 96000.0f})
    for (int tone : {0, 32, 63, 64, 65, 96, 127})
    for (int level : {0, 64, 127})
    {
        synth2::dsp::Equalizer eq; eq.prepare (rate);
        synth2::EqualizerSettings p; p.tone = tone / 127.0f;
        p.level = level / 127.0f; p.freq = p.q = 1.0f; eq.setParams (p);
        for (int i = 0; i < 1000; ++i)
        {
            const float output = eq.processSample (i == 0 ? 0.1f : 0.0f);
            ASSERT_TRUE(std::isfinite (output));
            ASSERT_LT(std::abs (output), 100.0f);
        }
    }
}

namespace
{
std::array<float,1024> eqImpulse(synth2::EqualizerSettings p,float rate)
{
    synth2::dsp::Equalizer eq;eq.prepare(rate);eq.setParams(p);
    std::array<float,1024> output{};
    bool silent=false;
    output[0]=.1f;
    for(int at=0;at<1024;at+=64)eq.processNative(output.data()+at,nullptr,64,silent);
    return output;
}
}

TEST(Equalizer, EquivalentIntegralNormalizationsProduceTheSameNativeResponse)
{
    for(float rate:{22050.f,44100.f,96000.f})for(int dial=0;dial<=127;++dial)
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(dial);
        synth2::EqualizerSettings p;
        p.freq=p.q=p.level=p.tone=dial/127.0f;
        const auto original=eqImpulse(p,rate);
        p.freq=p.q=p.level=p.tone=dial*(1.f/127.f);
        EXPECT_EQ(original,eqImpulse(p,rate));
    }
}

TEST(Equalizer, FractionalNativeControlsChangeAudioWithoutRoundingToAnotherDial)
{
    using P=synth2::EqualizerSettings;
    for(float P::* field:{&P::freq,&P::level,&P::q,&P::tone})
    {
        P p;
        p.freq=p.q=p.tone=64.0f/127;p.level=80.0f/127;
        p.*field=64.125f/127;const auto a=eqImpulse(p,44100);
        p.*field=64.375f/127;const auto b=eqImpulse(p,44100);
        EXPECT_NE(a,b);
        for(float sample:b)ASSERT_TRUE(std::isfinite(sample));
    }
}

TEST(Equalizer, FractionalPeakDialsUseTheDeclaredFrequencyGainAndQualityScales)
{
    synth2::dsp::PeakEq eq;eq.prepare(44100);
    eq.setDials(63.5f,64.5f,63.5f);
    const auto settings=eq.snapshot();
    EXPECT_NEAR(settings[0],50*std::sqrt(320.0f),.001f);
    EXPECT_NEAR(settings[1],25.0f/127,1e-6f);
    EXPECT_NEAR(settings[2],std::sqrt(10.0f),1e-6f);
    for(float rate:{22050.f,44100.f,96000.f})for(float dial:{.125f,63.875f,64.125f,126.875f})
    {
        synth2::EqualizerSettings p;
        p.freq=p.level=p.q=p.tone=dial/127;
        for(float sample:eqImpulse(p,rate))
        {ASSERT_TRUE(std::isfinite(sample));ASSERT_LT(std::abs(sample),1000);}
    }
}

TEST(Equalizer, SampleContextAndStereoGraphUseTheSameFractionalKernels)
{
    for(float rate:{22050.f,44100.f,96000.f})for(float dial:{0.f,32.125f,64.f,64.375f,127.f})
    {
        synth2::dsp::Equalizer sample,context,graph;
        synth2::EqualizerSettings p;p.freq=64.125f/127;p.q=32.375f/127;p.tone=dial/127;p.level=(dial==64 ? 64.f : 80.25f)/127;
        for(auto* eq:{&sample,&context,&graph}){eq->prepare(rate);eq->setParams(p);}
        for(int block=0;block<4;++block)
        {
            synth2::audio::AudioBuffer<float> a(2,64),b(2,64),c(2,64);
            for(int channel=0;channel<2;++channel)for(int i=0;i<64;++i)
            {
                const float value=.1f*std::sin((block*64+i+channel*13)*.17f);
                a.setSample(channel,i,sample.processSample(value,channel));b.setSample(channel,i,value);c.setSample(channel,i,value);
            }
            synth2::audio::AudioBlock<float> audio(b);synth2::audio::ProcessContextReplacing<float> processing(audio);context.process(processing);
            bool silent=false;graph.processNative(c.getWritePointer(0),c.getWritePointer(1),64,silent);
            for(int channel=0;channel<2;++channel)for(int i=0;i<64;++i)
            {EXPECT_EQ(a.getSample(channel,i),b.getSample(channel,i));EXPECT_EQ(a.getSample(channel,i),c.getSample(channel,i));}
        }
    }
}
