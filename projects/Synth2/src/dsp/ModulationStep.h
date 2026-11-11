#pragma once
#include "OscillatorConstants.h"
#include <cmath>

namespace synth2::dsp
{
    // Resolve 2*sin(pi*frequency/rate) to its compatible Float32 step word.
    // Prepared Float32 knots and FMA retain the argument/product residuals without
    // a double control path. This runs at setup/control updates, not per sample.
    inline float modulationStep(float frequency,float rate)
    {
        const float quotient=frequency/rate;
        const float quotientResidual=std::fma(-quotient,rate,frequency)/rate;
        const int knot=static_cast<int>(std::round(quotient*1024.0f));
        const float fraction=quotient-static_cast<float>(knot)*(1.0f/1024.0f);
        constexpr float pi=3.1415927410125732421875f,piResidual=-8.742278000372475e-8f;
        const float delta=fraction*pi;
        const float deltaResidual=std::fma(fraction,pi,-delta)+fraction*piResidual
            +quotientResidual*pi+quotientResidual*piResidual
            +static_cast<float>((knot-static_cast<int>(static_cast<unsigned>(knot)&2047u))/2048)*sinePeriodResidual;
        const float squared=delta*delta;
        const float squaredResidual=std::fma(delta,delta,-squared);
        const float sineResidual=deltaResidual+delta*squared*(-1.0f/6.0f+squared*(1.0f/120.0f))
            -deltaResidual*squared*.5f;
        const float cosineCorrection=-.5f*squared;
        const float cosineResidual=-.5f*squaredResidual+squared*squared*(1.0f/24.0f)
            -delta*deltaResidual-.5f*deltaResidual*deltaResidual;
        const auto sineIndex=static_cast<unsigned>(knot)&2047u,cosineIndex=static_cast<unsigned>(knot+512)&2047u;
        const std::array<float,2> sineWords{oscillatorSine[sineIndex],oscillatorSineResidual[sineIndex]};
        const std::array<float,2> cosineWords{oscillatorSine[cosineIndex],oscillatorSineResidual[cosineIndex]};
        const float result=std::fma(cosineWords[0],delta,sineWords[0]);
        float residual=std::fma(cosineWords[0],delta,sineWords[0]-result);
        residual=std::fma(cosineWords[0],sineResidual,residual+sineWords[1]);
        residual=std::fma(sineWords[0],cosineCorrection,residual);
        residual=std::fma(sineWords[0],cosineResidual,residual);
        residual=std::fma(cosineWords[1],delta,residual);
        residual=std::fma(cosineWords[1],sineResidual,residual);
        residual=std::fma(sineWords[1],cosineCorrection,residual);
        residual=std::fma(sineWords[1],cosineResidual,residual);
        return (result+residual)*2.0f;
    }
}
