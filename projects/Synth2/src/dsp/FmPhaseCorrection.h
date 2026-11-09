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
        Advance advance(double depth,double source,uint32_t increment,uint32_t phase,bool enabled) noexcept
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
            const double product=compatibleControlCurve(depth,[&](int value){
                return static_cast<int32_t>(static_cast<float>(value)*static_cast<float>(source));
            });
            const double nextError=(phaseError+product-native)*(31.0/32.0);
            const double value=static_cast<double>(native)+nextError-phaseError+phaseCarry;
            const int32_t audible=static_cast<int32_t>(value);
            phaseCarry=value-audible;phaseError=nextError;
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
        double phaseError {},phaseCarry {};
    };
}
