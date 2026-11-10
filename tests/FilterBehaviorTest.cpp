#include <gtest/gtest.h>
#include "dsp/Filter.h"
#include "dsp/FilterAudioRenderer.h"
#include <cstring>
#include "SynthEngine.h"
#include <vector>
#include "patch/Synth1PatchMapper.h"
#include <array>
#include <cmath>

namespace
{
    // Small-signal steady-state response, independent of the provisional native
    // cutoff curve. Ratios test filter shape rather than guessed Synth1 gain.
    double response (int mode, double rate, double frequency, float cutoff = 400.0f,
                     float amount = 0.0f, float envelope = 0.0f, float tracking = 0.0f)
    {
        synth2::dsp::Filter filter;
        filter.prepare ({static_cast<float>(rate), 64, 1});
        filter.setParams (mode, cutoff, 0.0f, amount, tracking);
        double energy = 0.0;
        const int frames = static_cast<int> (rate / 2);
        for (int frame = 0; frame < frames; ++frame)
        {
            const float input = static_cast<float> (0.001 * std::sin (synth2::audio::MathConstants<double>::twoPi * frequency * frame / rate));
            const float output = filter.processSample (input, envelope);
            if (frame >= frames / 2) energy += output * output;
        }
        return std::sqrt (energy / (frames - frames / 2)) / (0.001 / std::sqrt (2.0));
    }
}

TEST(FilterResponse, HighPassRejectsLowFrequencies)
{
    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        const double low = response (2, rate, 35.0);
        const double high = response (2, rate, 3500.0);
        EXPECT_GT(high, 0.5) << rate;
        EXPECT_LT(low, high * 0.03) << rate;
    }
}

TEST(FilterResponse, LP24HasFourPoleStopBandAndUnityLowFrequencyGain)
{
    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        EXPECT_GT(response (1, rate, 35.0), 0.95);
        const double high = response (1, rate, 3500.0);
        EXPECT_LT(response (1, rate, 7000.0), high * 0.1) << rate;
        EXPECT_GT(high, 1e-5);
    }
}

TEST(FilterResponse, ZeroSaturationPreservesLinearInputScaling)
{
    synth2::dsp::Filter quiet, loud;
    quiet.prepare ({44100.0, 64, 1}); loud.prepare ({44100.0, 64, 1});
    quiet.setParams (0, 1023.0f, 0.0f, 0.0f, 0.0f);
    loud.setParams (0, 1023.0f, 0.0f, 0.0f, 0.0f);
    double energy = 0.0, error = 0.0;
    for (int i = 0; i < 22050; ++i)
    {
        const float x = static_cast<float> (std::sin (synth2::audio::MathConstants<double>::twoPi * 440.0 * i / 44100.0));
        const float a = quiet.processSample (x * 0.001f, 0.0f) * 1000.0f;
        const float b = loud.processSample (x, 0.0f);
        energy += b * b; error += (a - b) * (a - b);
    }
    EXPECT_LT(std::sqrt(error / energy), 1e-5);
}

TEST(FilterResponse, BandPassRejectsBothSides)
{
    for (double rate : {44100.0, 48000.0, 96000.0})
    {
        const double centre = response (3, rate, 350.0);
        EXPECT_GT(centre, 0.2) << rate;
        EXPECT_LT(response (3, rate, 35.0), centre * 0.3) << rate;
        EXPECT_LT(response (3, rate, 3500.0), centre * 0.3) << rate;
    }
}

TEST(FilterResponse, HighPassAndBandPassRejectDC)
{
    for (int mode : {2, 3})
    {
        synth2::dsp::Filter filter;
        filter.prepare ({48000.0, 64, 1});
        filter.setParams (mode, 400.0f, 0.0f, 0.0f, 0.0f);
        float output = 0.0f;
        for (int frame = 0; frame < 48000; ++frame)
            output = filter.processSample (0.001f, 0.0f);
        EXPECT_NEAR(output, 0.0f, 1e-7f) << mode;
    }
}

TEST(FilterResponse, SweepsRemainFiniteAndResetClearsHistory)
{
    for (double rate : {22050.0, 44100.0, 48000.0, 96000.0})
    for (int mode = 0; mode < 4; ++mode)
    for (float resonance : {0.0f, 0.5f, 1.0f})
    {
        synth2::dsp::Filter filter;
        filter.prepare ({static_cast<float>(rate), 127, 1});
        for (int frame = 0; frame < 12000; ++frame)
        {
            const float sweep = static_cast<float> (frame % 2048) / 2047.0f;
            filter.setParams (mode, sweep * 1023.0f, resonance, 1.0f, 0.0f);
            const float output = filter.processSample (frame % 97 == 0 ? 0.5f : 0.0f, sweep);
            ASSERT_TRUE(std::isfinite (output)) << mode << " " << rate << " " << resonance;
            ASSERT_LT(std::abs(output),10.0f) << mode << " " << rate << " " << resonance;
        }
        filter.reset();
        for (int frame = 0; frame < 32; ++frame)
            EXPECT_FLOAT_EQ(filter.processSample (0.0f, 0.0f), 0.0f);
    }
}

TEST(FilterResponse, IndexedSpansPreserveFixedSettingsBypassAndRetainedHistory)
{
    using namespace synth2::dsp;
    for(double rate:{44100.,48000.25,96000.})for(int mode=0;mode<5;++mode)
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(mode);
        Filter reference,candidate;
        reference.prepare({static_cast<float>(rate),64,1});candidate.prepare({static_cast<float>(rate),64,1});
        for(int span=0;span<4;++span)
        {
            const FilterAudioSettings config{(mode+span)%5,span%2 ? .6f : .1f,span%2 ? .4f : 0.f,span==2};
            reference.setParams(config.type,0,config.resonance,0,0,config.saturation);
            for(int sample=0;sample<64;++sample)
            {
                const float signal=sample%17==0 ? .01f : -.0001f;
                const int index=800-sample*7;
                const int expectedIndex=span==1 ? 0 : span==3 ? 1023 : index;
                const float expected=config.bypass ? signal : reference.processNativeIndex(signal,expectedIndex);
                const float actual=FilterAudioRenderer::process(candidate,config,{signal,static_cast<float>(expectedIndex)});
                EXPECT_EQ(std::memcmp(&actual,&expected,sizeof(float)),0)<<span<<" "<<sample;
            }
            EXPECT_EQ(candidate.readNativeMemory(),reference.readNativeMemory());
            candidate.finishNativeBlock();reference.finishNativeBlock();
        }
    }
}

TEST(FilterResponse, CanonicalCoefficientDomainsRemainStableBelowStandardRates)
{
    for(double rate:{8000.,11025.,22050.,32000.,44100.,48000.25,96000.})
    for(int index:{0,256,512,768,1023})for(float resonance:{0.f,.5f,1.f})
    {
        SCOPED_TRACE(rate);
        SCOPED_TRACE(index);
        SCOPED_TRACE(resonance);
        synth2::dsp::StateVariableFilter svf;
        svf.configure(rate,index,resonance);
        const double f=svf.getCoefficient();
        // Jury condition for the recovered recurrence, independent of its mapping formula.
        EXPECT_GT(f,0.0);
        EXPECT_LT(f*f+2.0*(1.0-resonance)*f,4.0);
        synth2::dsp::Lp24Kernel lp;
        lp.prepare(static_cast<int>(rate));lp.configure(index,resonance);
        EXPECT_GE(lp.getA(),-1.0f);EXPECT_LT(lp.getA(),1.0f);
    }
}

TEST(FilterResponse, SampleAndIndexInterfacesUseExactlyTheSameKernels)
{
    for(double rate:{22050.,44100.,48000.25,96000.})for(int mode=0;mode<5;++mode)
    for(int index:{0,511,1023})for(float resonance:{0.f,112.f/127.f,1.f})
    {
        synth2::dsp::Filter sampled,indexed;
        sampled.prepare({static_cast<float>(rate),64,1});indexed.prepare({static_cast<float>(rate),64,1});
        sampled.setParams(mode,static_cast<float>(index),resonance,0,0);
        indexed.setParams(mode,static_cast<float>(index),resonance,0,0);
        for(int frame=0;frame<256;++frame)
        {
            const float input=frame%31==0 ? .25f : 0.f;
            const float a=sampled.processSample(input,0);
            const float b=indexed.processNativeIndex(input,index);
            ASSERT_TRUE(std::isfinite(a));
            EXPECT_EQ(std::memcmp(&a,&b,sizeof(float)),0);
        }
    }
}
