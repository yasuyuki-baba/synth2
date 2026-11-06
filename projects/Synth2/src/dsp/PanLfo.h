#pragma once
#include <algorithm>
#include <cmath>
#include "StereoBalance.h"

namespace synth2::dsp
{
    // CBal 0x15e10: cubic modulation and a two-millisecond half-life.
    class PanLfo
    {
    public:
        void prepare(double rate,bool preserve=false) noexcept
        {
            const float interval=static_cast<float>(static_cast<int>(rate))*0.0020000000949949026f;
            coefficient=static_cast<float>(1.0-std::pow(0.5,1.0/static_cast<double>(interval)));
            if(!preserve)left=right=0;
        }
        static float curve(float value) noexcept
        {
            const float x=value+(value<0 ? 1.0f : -1.0f);
            const float cubic=(x*x)*x;
            return value<0 ? cubic : static_cast<float>(static_cast<double>(cubic)*0.5+1.5);
        }
        void process(float& l,float& r,double control,float pan) noexcept
        {
            const float x=static_cast<float>(control)*(1.0f/64.0f);
            const auto balance=StereoBalance::gains(pan);
            const float targetL=curve(x)*balance.left;
            const float targetR=curve(-x)*balance.right;
            left+=(targetL-left)*coefficient;right+=(targetR-right)*coefficient;
            l*=left;r*=right;
        }
        float left {},right {},coefficient {};
    };
}
