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
            const double interval=static_cast<double>(static_cast<int>(rate))*0.0020000000949949026;
            coefficient=static_cast<double>(1.0-std::pow(0.5,1.0/static_cast<double>(interval)));
            if(!preserve)left=right=0;
        }
        static double curve(double value) noexcept
        {
            const double x=value+(value<0 ? 1.0 : -1.0);
            const double cubic=(x*x)*x;
            return value<0 ? cubic : static_cast<double>(static_cast<double>(cubic)*0.5+1.5);
        }
        void process(double& l,double& r,double control,double pan) noexcept
        {
            const double x=static_cast<double>(control)*(1.0/64.0);
            const auto balance=StereoBalance::gains(pan);
            const double targetL=curve(x)*balance.left;
            const double targetR=curve(-x)*balance.right;
            left+=(targetL-left)*coefficient;right+=(targetR-right)*coefficient;
            l*=left;r*=right;
        }
        double left {},right {},coefficient {};
    };
}
