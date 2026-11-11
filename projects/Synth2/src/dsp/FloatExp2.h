#pragma once
#include "FloatMathTables.h"
#include <cmath>

namespace synth2::dsp
{
    // One Float32 scaled exponential, rounding its product once. Prepared
    // high/residual coefficient words and FMA avoid rounding a ratio before
    // multiplying by the frequency. Inputs and all runtime arithmetic are float.
    inline float scaledExp2(float exponent,float scale=1.0f) noexcept
    {
        const int octave=static_cast<int>(std::round(exponent));
        const float fraction=exponent-static_cast<float>(octave);
        const int index=static_cast<int>(fraction*4096.0f);
        const float delta=fraction-static_cast<float>(index)*(1.0f/4096.0f);
        const auto words=exp2Words[static_cast<std::size_t>(index+2048)];
        constexpr float logTwo=0.693147182464599609375f;
        constexpr float logTwoResidual=-1.904654299957768e-9f;
        const float linear=delta*logTwo;
        const float remainder=std::fma(delta,logTwo,-linear)+delta*logTwoResidual
            +delta*delta*(0.2402265069591007f+delta*(0.05550410866482158f+delta*0.009618129107628477f));
        const float product=scale*words[0];
        const float productResidual=std::fma(scale,words[1],std::fma(scale,words[0],-product));
        const float result=std::fma(product,linear,product);
        float residual=std::fma(product,linear,product-result);
        residual=std::fma(product,remainder,residual);
        residual=std::fma(productResidual,linear,residual+productResidual);
        residual=std::fma(productResidual,remainder,residual);
        return std::ldexp(result+residual,octave);
    }
}
