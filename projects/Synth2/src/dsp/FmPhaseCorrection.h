#pragma once
#include <cstdint>
#include "CompatibleControlCurve.h"

namespace synth2::dsp
{
    // Common FM product correction; no coefficient filter or waveform policy.
    // The 32-frame feedback horizon retains proximity to compatible integer FM.
    // This is a compatibility constraint, not unbiased fractional integration.
    class FmPhaseCorrection
    {
    public:
        struct Advance
        {
            uint32_t audibleOffset {},referenceOffset {},referenceBefore {},referenceAfter {};
        };
        void reset() noexcept { ready=false; }
        Advance advance(float depth,float source,uint32_t increment,uint32_t phase,bool enabled) noexcept
        {
            constexpr uint32_t mask=0x07ffffffu;
            // Only the compatible Sync/noise reference retains the float product.
            const int32_t native=static_cast<int32_t>(static_cast<float>(depth)*static_cast<float>(source));
            if(!enabled)
            {
                ready=false;
                const auto offset=static_cast<uint32_t>(native);
                return {offset,offset,phase,(phase+increment+offset)&mask};
            }
            if(!ready || phase!=lastPhase)
            {
                ready=true;reference=phase;phaseError=phaseCarry=0;
            }
            // Preserve compatible integer depth knots, interpolating real depth
            // through the same mapping rather than changing the phase policy.
            // Interpolate differences from the native word: absolute phase
            // offsets can exceed Float32's exact integer range.
            const float difference=compatibleControlCurve(depth,[&](int value){
                return static_cast<int32_t>(static_cast<float>(value)*source)-native;
            });
            const float nextError=(phaseError+difference)*(31.0f/32.0f);
            const float adjustment=nextError-phaseError+phaseCarry;
            const int32_t whole=static_cast<int32_t>(adjustment);
            int32_t audible=native+whole;
            phaseCarry=adjustment-static_cast<float>(whole);
            // Truncation applies to the signed sum, without promoting the
            // large integer word to Float32 and losing its low phase bits.
            if(audible>0 && phaseCarry<0){--audible;phaseCarry+=1;}
            else if(audible<0 && phaseCarry>0){++audible;phaseCarry-=1;}
            phaseError=nextError;
            const uint32_t before=reference;
            // Sync/slave/noise follows the compatible online reference, separately
            // from the corrected audible oscillator phase. No recorded input.
            reference=(reference+increment+static_cast<uint32_t>(native))&mask;
            lastPhase=(phase+increment+static_cast<uint32_t>(audible))&mask;
            return {static_cast<uint32_t>(audible),static_cast<uint32_t>(native),before,reference};
        }
    private:
        bool ready {};
        uint32_t reference {},lastPhase {};
        float phaseError {},phaseCarry {};
    };
}
