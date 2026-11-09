#pragma once

#include <algorithm>
#include <cmath>
#include <vector>
#include "WaveTableBank.h"

namespace synth2::dsp
{
    // Win64 DLL RVA 0x3cdf0: 23 quarter-octave bands, then harmonic counts 14..1.
    inline std::vector<float> nativeTriangleBorders()
    {
        std::vector<float> borders;
        int previous = -1, harmonics = 1;
        for (int i = 0; i < 36; ++i)
        {
            const float x = static_cast<float> (i) * (1.0f / 36.0f);
            const float frequency = static_cast<float> (std::exp (std::log (512.0) * x) * 27.5);
            harmonics = static_cast<int> (std::max (22050.0f / frequency,1.0f));
            if (harmonics < 16) break;
            if (harmonics != previous) borders.push_back (frequency);
            previous = harmonics;
        }
        for (int h = harmonics; h >= 1; --h) borders.push_back (22050.0f / static_cast<float> (h));
        return borders;
    }

    // Native shapes 1/2: float sum of sine harmonics, then 2/pi.
    inline ShapeTables buildNativeSaw (const std::vector<double>& sine)
    {
        ShapeTables result;
        constexpr double inversePi = 0.3183098861837907;
        for (float border : nativeTriangleBorders())
        {
            const int limit = static_cast<int> (22050.0 / border);
            const auto offset = result.data.size();
            for (int n = 0; n < 2048; ++n)
            {
                double sum = 0;
                for (int k = 1; k <= limit; ++k)
                    sum += sine[static_cast<std::size_t>((n*k)&2047)] / static_cast<double>(k);
                result.data.push_back (static_cast<double> (static_cast<double> (sum*2.0)*inversePi));
            }
            result.infos.push_back ({offset,2048,border});
        }
        return result;
    }

    // Native shape 3: alternating odd sine harmonics, float accumulation,
    // 8/pi^2 gain. No window, peak normalization, or sample-rate rescaling.
    inline ShapeTables buildNativeTriangle (const std::vector<double>& sine)
    {
        ShapeTables result;
        constexpr double inversePi = 0.3183098861837907;
        for (float border : nativeTriangleBorders())
        {
            const int limit = static_cast<int> (22050.0 / border);
            const auto offset = result.data.size();
            for (int n = 0; n < 2048; ++n)
            {
                double sum = 0;
                for (int k = 1; k <= limit; k += 2)
                {
                    const double term = sine[static_cast<std::size_t> ((n*k) & 2047)] / static_cast<double> (k*k);
                    if ((k & 3) == 1) sum += term;
                    else sum -= term;
                }
                result.data.push_back (static_cast<double> (static_cast<double> (sum * 8.0) * inversePi * inversePi));
            }
            result.infos.push_back ({offset,2048,border});
        }
        return result;
    }
}
