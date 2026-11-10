#pragma once

#include "FilterEnvelope.h"
#include "CompatibleControlCurve.h"
#include "FractionalDial.h"
#include <array>

namespace synth2::dsp
{
    // Win64 VCO depth setter 0x3e260 and pitch helper 0x3b940.
    // OSC2 pitch, FM depth and pulse-width destinations share the compatible envelope.
    struct ModulationEnvelope
    {
        static float amount(int dial) noexcept
        {
            dial=std::clamp(dial,0,127);
            return dial>64 ? static_cast<float>(dial-64)*(1.0f/63.0f)
                          : static_cast<float>(dial)*(1.0f/64.0f)-1.0f;
        }
        static int peak(int dial,int destination=0) noexcept
        {
            const int range=destination==1 ? 512 : destination==2 ? 128 : 640;
            return static_cast<int>(std::abs(amount(dial))*static_cast<float>(range));
        }
        static float peak(float dial,int destination=0) noexcept
        {
            return compatibleControlCurve(std::clamp(static_cast<float>(dial),0.0f,127.0f),
                [&](int value) {return peak(value,destination);});
        }
        static float timeSeconds(int dial,bool attack) noexcept
        {
            const float normalized=static_cast<float>(std::clamp(dial,0,127))*(1.0f/127.0f);
            return static_cast<float>(std::exp(std::log(attack ? 56000.0f : 6000.0f)
                       *static_cast<float>(normalized))*(attack ? .0005f : .01f));
        }
        static float timeSeconds(float dial,bool attack) noexcept
        {return interpolateDial(dial,[&](int value){return timeSeconds(value,attack);});}
        static int pulseWidthBins(float base,int control) noexcept
        {
            // Win64 0x3c10c: positive depth approaches 2, negative approaches 0.
            // Preserve float arithmetic before the Float32 clamp/table scaling.
            const float modulation=static_cast<float>(control)*(1.0f/128.0f);
            const float width=control>0 ? base+(2.0f-base)*modulation : base*(1.0f+modulation);
            return static_cast<int>(std::clamp(static_cast<float>(width),.01f,1.99f)*1024.0f);
        }
        static float pitchFrequency(float frequency,float control) noexcept
        {
            control=std::clamp(control,-1280.0f,1280.0f);
            return compatibleControlCurve(control,[&](int value) {
                const int quotient=value/128,remainder=value%128;
                const float ratio=fineTable()[static_cast<std::size_t>(remainder+128)]*std::ldexp(1.0f,quotient);
                return std::min(frequency*ratio,22000.0f);
            });
        }
        static float pulseWidthBins(float base,float control) noexcept
        {return compatibleControlCurve(control,[&](int value){return pulseWidthBins(base,value);});}
        // Source-dial extension: first resolve the compatible whole-width knots,
        // then interpolate width and modulation independently without re-rounding
        // fractional edits. The fixed-base helpers above evaluate those knots.
        static float pulseWidthForDial(float dial,float control) noexcept
        {
            return compatibleControlCurve(std::clamp(dial,0.0f,127.0f),[&](int source) {
                return pulseWidthBins(static_cast<float>(source)*(1.0f/127.0f),control);
            });
        }
        static void prepare() noexcept { (void)fineTable(); }
    private:
        static const std::array<float,257>& fineTable() noexcept
        {
            static const auto table=[]
            {
                std::array<float,257> values {};
                for(int i=-128;i<=128;++i)
                    values[static_cast<std::size_t>(i+128)]=static_cast<float>(
                        std::exp(std::log(2.0f)*static_cast<float>(i*(1.0f/128.0f))));
                return values;
            }();
            return table;
        }
    };
}
