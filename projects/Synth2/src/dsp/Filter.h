#pragma once

#include "audio/AudioTypes.h"
#include "Saturation.h"
#include "Lp24Kernel.h"
#include "DiodeKernel.h"
#include "StateVariableFilter.h"
#include "FilterLimiter.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace synth2::dsp
{
    // One filter family for every source route. Coordinate adapters share the
    // same compatible mode kernels, limiter and saturation state.
    class Filter
    {
    public:
        enum class Mode { LP12 = 0, LP24 = 1, HP12 = 2, BP12 = 3, Diode = 4 };

        void setSampleRate (double sr) noexcept { sampleRate = sr; nativeLimiter.prepare(sr,true); nativeLp24.prepare(static_cast<int>(sr),true); }
        void prepare (const ::synth2::audio::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            nativeLimiter.prepare(sampleRate);
            nativeLp24.prepare(static_cast<int>(sampleRate));
            reset();
        }
        void reset() noexcept
        {
            nativeStateVariable.reset();
            nativeLp24.reset();
            nativeDiode.reset();
            nativeLimiter.reset();
        }
        void setParams (int type, float cutoff, float resonance, float amount,
                        float tracking, float drive = 0.0f) noexcept
        {
            mode = static_cast<Mode> (std::clamp (type, 0, 4));
            cutoffIndex = std::clamp (cutoff, 0.0f, 1023.0f);
            resonanceNorm = std::clamp (resonance, 0.0f, 1.0f);
            envelopeAmount = std::clamp (amount, -1.0f, 1.0f);
            keyboardTracking = tracking;
            const float newSaturation = std::clamp (drive, 0.0f, 1.0f);
            if (newSaturation != saturation)
            {
                saturation = newSaturation;
                saturationCurve = saturationStrength (saturation);
            }
        }
        void finishNativeBlock() noexcept { nativeLimiter.finish(); }
        // SVF recurrence, limiter and LP24 state, for stopped/offline inspection.
        std::array<float,17> readNativeMemory() const noexcept
        {
            std::array<float,17> values{nativeStateVariable.getLow(),nativeStateVariable.getBand(),nativeLimiter.value()};
            nativeLp24.copyState(values.data()+3);return values;
        }
        float processNativeIndex (float input,double index) noexcept
        {
            static const bool bypass = std::getenv ("SYNTH2_BYPASS_FILTER") != nullptr;
            if(bypass)return input;
            float filtered;
            // The fifth serialized mode has its own nonlinear diode kernel.
            if(mode==Mode::Diode)
            {
                nativeDiode.configure(static_cast<int>(sampleRate),index,resonanceNorm);filtered=nativeDiode.process(input);
            }
            else if(mode==Mode::LP24)
            {
                nativeLp24.configure(index,resonanceNorm);filtered=nativeLp24.process(input);
            }
            else
            {
                nativeStateVariable.configure(sampleRate,index,resonanceNorm);
                filtered=nativeStateVariable.process(input,static_cast<int>(mode));
            }
            const float output=nativeLimiter.process(filtered);
            return saturationCurve>0 ? saturate(output,saturationCurve) : output;
        }
        float processSample (float input, float envelope) noexcept
        {
            static const bool bypass = std::getenv ("SYNTH2_BYPASS_FILTER") != nullptr;
            if (bypass) return input;
            // Native LP24 inversions give 127/64 serialized cutoff steps per
            // signed amount step at full envelope, independently of base cutoff.
            const float index = std::clamp (cutoffIndex + 1023.0f
                * envelope * envelopeAmount + keyboardTracking, 0.0f, 1023.0f);
            return processNativeIndex(input,index);
        }
    private:
        Mode mode {Mode::LP12};
        double sampleRate {44100.0};
        float cutoffIndex {1023.0f}, resonanceNorm {0.0f};
        float envelopeAmount {0.0f}, keyboardTracking {0.0f}, saturation {0.0f}, saturationCurve {0.0f};
        StateVariableFilter nativeStateVariable;
        Lp24Kernel nativeLp24;
        DiodeKernel nativeDiode;
        FilterLimiter nativeLimiter;
    };
}
