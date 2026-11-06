#include "WaveFile.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <memory>
static void log(const std::string& s){std::cout<<s<<'\n';}
template<class T> static std::string number(T v,int precision=-1){std::ostringstream s;if(precision>=0)s<<std::fixed<<std::setprecision(precision);else s<<std::setprecision(9);s<<v;return s.str();}
int main(int argc,char** argv) try {
    std::filesystem::path refFile,testFile;int searchRange=10000;
    for(int i=1;i<argc;++i){std::string arg=argv[i];if(arg=="--window" && i+1<argc)searchRange=std::stoi(argv[++i]);else if(refFile.empty())refFile=arg;else if(testFile.empty())testFile=arg;else throw std::runtime_error("Unexpected argument");}
    if(refFile.empty() || testFile.empty())throw std::runtime_error("Usage: CompareWaves ref.wav test.wav [--window samples]");
    log("[CompareWaves] ref = "+refFile.string());log("[CompareWaves] test = "+testFile.string());
    auto refReader=std::make_unique<synth2::offline::WaveFile>(refFile);
    auto testReader=std::make_unique<synth2::offline::WaveFile>(testFile);
    if (std::abs (refReader->sampleRate - testReader->sampleRate) > 1.0)
    {
        log ("[CompareWaves] WARNING: sampleRate mismatch: ref=" + number (refReader->sampleRate)
             + ", test=" + number (testReader->sampleRate));
    }

    const int64_t numSamplesRef  = refReader->lengthInSamples;
    const int64_t numSamplesTest = testReader->lengthInSamples;
    const int64_t numSamples     = std::min (numSamplesRef, numSamplesTest);

    if (numSamples <= 0)
    {
        log ("[CompareWaves] ERROR: no samples to compare");
        return 1;
    }

    const int numChannelsRef  = (int) refReader->numChannels;
    const int numChannelsTest = (int) testReader->numChannels;

    const int numChannels = std::min (numChannelsRef, numChannelsTest);

    const auto& refBuffer=refReader->samples;
    const auto& testBuffer=testReader->samples;

    // Per-channel peak check before mono mix
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* xr = refBuffer.getReadPointer (ch);
        const float* yt = testBuffer.getReadPointer (ch);

        float peakRefCh  = 0.0f;
        float peakTestCh = 0.0f;

        for (int64_t i = 0; i < numSamples; ++i)
        {
            peakRefCh  = std::max (peakRefCh,  std::abs (xr[i]));
            peakTestCh = std::max (peakTestCh, std::abs (yt[i]));
        }

        log ("[CompareWaves] ref ch" + number (ch) + " peak = " + number (peakRefCh));
        log ("[CompareWaves] test ch" + number (ch) + " peak = " + number (peakTestCh));
    }

    // Mix to mono for analysis
    synth2::audio::AudioBuffer<float> refMono  (1, (int) numSamplesRef);
    synth2::audio::AudioBuffer<float> testMono (1, (int) numSamplesTest);

    refMono.clear();
    testMono.clear();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        refMono.addFrom (0, 0, refBuffer.getReadPointer(ch), (int) numSamplesRef, 1.0f / (float) numChannels);
        testMono.addFrom (0, 0, testBuffer.getReadPointer(ch), (int) numSamplesTest, 1.0f / (float) numChannels);
    }

    const float* x = refMono.getReadPointer (0);
    const float* y = testMono.getReadPointer (0);

    // Simple peak check on mono signals
    float refPeak = 0.0f;
    float testPeak = 0.0f;
    for (int64_t i = 0; i < numSamplesRef; ++i) refPeak  = std::max (refPeak,  std::abs (x[i]));
    for (int64_t i = 0; i < numSamplesTest; ++i) testPeak = std::max (testPeak, std::abs (y[i]));

    log ("[CompareWaves] ref mono peak |x| = " + number (refPeak));
    log ("[CompareWaves] test mono peak |y| = " + number (testPeak));

    // -------------------------------------------------------------------------
    // Alignment search (cross-correlation)
    // -------------------------------------------------------------------------
    // We search for a best offset O such that ref[i + O] ~= test[i].
    // Note: O can be negative if the 'test' trigger happened *before* 'ref'.
    int bestOffset = 0;
    long double maxCorr = -1.0L;

    const int maxSearchSamples = searchRange;
    log ("[CompareWaves] searching for best alignment in range ±" + number(maxSearchSamples) + " samples...");

    if (maxSearchSamples < 0)
    {
        log ("[CompareWaves] ERROR: Alignment window must be nonnegative");
        return 1;
    }
    const auto search = [&] (int testStart, int compareLen)
    {
      for (int offset = -maxSearchSamples; offset <= maxSearchSamples; ++offset)
      {
        long double curSumXY = 0.0L;
        long double curSumX2 = 0.0L;
        long double curSumY2 = 0.0L;
        
        const int start = std::max (testStart, -offset);
        const int end = std::min ({testStart + compareLen, static_cast<int> (numSamplesTest),
                                   static_cast<int> (numSamplesRef) - offset});
        if (end <= start) continue;
        for (int i = start; i < end; ++i)
        {
            const long double yv = (long double)y[i];
            const long double xv = (long double)x[i + offset];
            curSumXY += xv * yv;
            curSumX2 += xv * xv;
            curSumY2 += yv * yv;
        }
        
        if (curSumX2 > 1e-12L && curSumY2 > 1e-12L)
        {
            long double curCorr = curSumXY / std::sqrt(curSumX2 * curSumY2);
            if (std::abs(curCorr) > maxCorr)
            {
                maxCorr = std::abs(curCorr);
                bestOffset = offset;
            }
        }
      }
    };
    // Preserve the historical search on sustained signals. Short percussion
    // can be completely silent there; retry near the onset rather than report
    // the -1 initializer as a perfect anti-correlation.
    search (static_cast<int> (numSamples) / 4, static_cast<int> (numSamples) / 2);
    if (maxCorr < 0.0L)
    {
        log ("[CompareWaves] alignment_window = onset_fallback");
        search (0, static_cast<int> (numSamples) / 2);
    }
    const bool alignmentValid = maxCorr >= 0.0L;
    if (! alignmentValid) maxCorr = 0.0L;
    log ("[CompareWaves] alignment_valid = " + number (alignmentValid ? 1 : 0));
    
    log ("[CompareWaves] best offset found: " + number(bestOffset) + " samples (abs_corr=" + number((double)maxCorr, 6) + ")");

    long double corr = maxCorr;

    // Compare using the best offset
    long double sumX2 = 0.0L;
    long double sumY2 = 0.0L;
    long double sumXY = 0.0L;
    long double sumDiff2 = 0.0L;
    
    // Calculate effective comparable range
    int startIdx = std::max(0, -bestOffset);
    int endIdx   = std::min((int)numSamplesTest, (int)numSamplesRef - bestOffset);
    int effectiveNumSamples = endIdx - startIdx;

    if (effectiveNumSamples <= 0) {
        log("[CompareWaves] ERROR: No overlapping samples after alignment!");
        return 1;
    }

    const int xStart = bestOffset;

    for (int i = startIdx; i < endIdx; ++i)
    {
        const long double xv = (long double)x[xStart + i];
        const long double yv = (long double)y[i];
        const long double d  = xv - yv;

        if (i - startIdx < 10) {
            log ("[CompareWaves_DEBUG] sample " + number(i) + ": ref=" + number((double)xv, 6) + " test=" + number((double)yv, 6));
        }

        sumX2    += xv * xv;
        sumY2    += yv * yv;
        sumXY    += xv * yv;
        sumDiff2 += d * d;
    }

    const long double n = (long double) effectiveNumSamples;

    const long double rmsX    = (n > 0) ? std::sqrt (sumX2 / n) : 0.0L;
    const long double rmsY    = (n > 0) ? std::sqrt (sumY2 / n) : 0.0L;
    const long double rmsDiff = (n > 0) ? std::sqrt (sumDiff2 / n) : 0.0L;

    long double bestGain = 1.0L;
    if (sumY2 > 0.0L)
        bestGain = sumXY / sumY2;

    long double sumDiff2Scaled = 0.0L;
    for (int i = startIdx; i < endIdx; ++i)
    {
        const long double xv = x[xStart + i];
        const long double yv = y[i] * bestGain;
        const long double d  = xv - yv;
        sumDiff2Scaled += d * d;
    }

    const long double rmsDiffScaled = (n > 0) ? std::sqrt (sumDiff2Scaled / n) : 0.0L;

    log ("[CompareWaves] effectiveSamples = " + number (effectiveNumSamples));
    log ("[CompareWaves] RMS ref   = " + number ((double) rmsX));
    log ("[CompareWaves] RMS test  = " + number ((double) rmsY));
    log ("[CompareWaves] RMS diff  = " + number ((double) rmsDiff));
    log ("[CompareWaves] bestGain  = " + number ((double) bestGain));
    log ("[CompareWaves] RMS diff (scaled) = " + number ((double) rmsDiffScaled));
    log ("[CompareWaves] corr      = " + number ((double) corr));
    log ("[CompareWaves] abs_corr  = " + number ((double) std::abs(corr)));

    return 0;
}

catch(const std::exception& e){log(std::string("[CompareWaves] ERROR: ")+e.what());return 1;}
