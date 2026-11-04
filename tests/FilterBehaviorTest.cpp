#include <gtest/gtest.h>
#include "dsp/Filter.h"
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
        filter.prepare ({rate, 64, 1});
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
        filter.prepare ({rate, 127, 1});
        for (int frame = 0; frame < 12000; ++frame)
        {
            const float sweep = static_cast<float> (frame % 2048) / 2047.0f;
            filter.setParams (mode, sweep * 1023.0f, resonance, 1.0f, 0.0f);
            const float output = filter.processSample (frame % 97 == 0 ? 0.5f : 0.0f, sweep);
            ASSERT_TRUE(std::isfinite (output)) << mode << " " << rate << " " << resonance;
            ASSERT_LT(std::abs(output), 10.0f) << mode << " " << rate << " " << resonance;
        }
        filter.reset();
        for (int frame = 0; frame < 32; ++frame)
            EXPECT_FLOAT_EQ(filter.processSample (0.0f, 0.0f), 0.0f);
    }
}
