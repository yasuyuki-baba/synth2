#pragma once

#include <algorithm>
#include <cmath>
#include <vector>
#include "WaveTableBank.h"
#include "OscillatorConstants.h"

namespace synth2::dsp
{
    // Win64 DLL RVA 0x3cdf0: 23 quarter-octave bands, then harmonic counts 14..1.
    inline std::vector<float> nativeTriangleBorders()
    {
        return {oscillatorBorders.begin(),oscillatorBorders.end()};
    }

    // Native shapes 1/2: float sum of sine harmonics, then 2/pi.
    inline ShapeTables buildNativeSaw (const std::vector<float>& sine)
    {
        ShapeTables result;
        for (float border : nativeTriangleBorders())
        {
            const int limit = static_cast<int> (22050.0f / border);
            const auto offset = result.data.size();
            for (int n = 0; n < 2048; ++n)
            {
                float sum = 0;
                for (int k = 1; k <= limit; ++k)
                    sum += sine[static_cast<std::size_t>((n*k)&2047)] / static_cast<float>(k);
                result.data.push_back (std::fma(sum*2.0f,inversePiHigh,(sum*2.0f)*inversePiLow));
            }
            result.infos.push_back ({offset,2048,border});
        }
        return result;
    }

    // Native shape 3: alternating odd sine harmonics, float accumulation,
    // 8/pi^2 gain. No window, peak normalization, or sample-rate rescaling.
    inline ShapeTables buildNativeTriangle (const std::vector<float>& sine)
    {
        ShapeTables result;
        for (float border : nativeTriangleBorders())
        {
            const int limit = static_cast<int> (22050.0f / border);
            const auto offset = result.data.size();
            for (int n = 0; n < 2048; ++n)
            {
                float sum = 0;
                for (int k = 1; k <= limit; k += 2)
                {
                    const float term = sine[static_cast<std::size_t> ((n*k) & 2047)] / static_cast<float> (k*k);
                    if ((k & 3) == 1) sum += term;
                    else sum -= term;
                }
                result.data.push_back (std::fma(sum*8.0f,inversePiSquaredHigh,(sum*8.0f)*inversePiSquaredLow));
            }
            result.infos.push_back ({offset,2048,border});
        }
        return result;
    }
}
