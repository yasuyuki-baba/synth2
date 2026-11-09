#pragma once

#include "offline/RenderDiagnostics.h"

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
        void setParams (int type, double cutoff, double resonance, double amount,
                        double tracking, double drive = 0.0) noexcept
        {
            mode = static_cast<Mode> (std::clamp (type, 0, 4));
            cutoffIndex = std::clamp (cutoff, 0.0, 1023.0);
            resonanceNorm = std::clamp (resonance, 0.0, 1.0);
            envelopeAmount = std::clamp (amount, -1.0, 1.0);
            keyboardTracking = tracking;
            const double newSaturation = std::clamp (drive, 0.0, 1.0);
            if (newSaturation != saturation)
            {
                saturation = newSaturation;
                saturationCurve = saturationStrength (saturation);
            }
        }
        void finishNativeBlock() noexcept { nativeLimiter.finish(); }
        // SVF recurrence, limiter and LP24 state, for stopped/offline inspection.
        std::array<double,17> readNativeMemory() const noexcept
        {
            std::array<double,17> values{nativeStateVariable.getLow(),nativeStateVariable.getBand(),nativeLimiter.value()};
            nativeLp24.copyState(values.data()+3);return values;
        }
        double processNativeIndex (double input,double index) noexcept
        {
            const bool bypass = offline::bypassFilter();
            if(bypass)return input;
            double filtered;
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
            const double output=nativeLimiter.process(filtered);
            return saturationCurve>0 ? saturate(output,saturationCurve) : output;
        }
        double processSample (double input, double envelope) noexcept
        {
            const bool bypass = offline::bypassFilter();
            if (bypass) return input;
            // Native LP24 inversions give 127/64 serialized cutoff steps per
            // signed amount step at full envelope, independently of base cutoff.
            const double index = std::clamp (cutoffIndex + 1023.0
                * envelope * envelopeAmount + keyboardTracking, 0.0, 1023.0);
            return processNativeIndex(input,index);
        }
    private:
        Mode mode {Mode::LP12};
        double sampleRate {44100.0};
        double cutoffIndex {1023.0}, resonanceNorm {0.0};
        double envelopeAmount {0.0}, keyboardTracking {0.0}, saturation {0.0}, saturationCurve {0.0};
        StateVariableFilter nativeStateVariable;
        Lp24Kernel nativeLp24;
        DiodeKernel nativeDiode;
        FilterLimiter nativeLimiter;
    };
}
