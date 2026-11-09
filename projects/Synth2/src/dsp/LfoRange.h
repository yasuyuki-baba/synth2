#pragma once
#include <cmath>
#include <algorithm>
#include <utility>

namespace synth2::dsp
{
    // CLFO 0x15080, native destination numbers and admitted integer depth0..127.
    // Range recalculation does not redraw the random target or move its history.
    inline std::pair<int,int> lfoDepthRange(int destination,int depth) noexcept
    {
        double amount;
        if(destination==1 || destination==2)
        {
            const double normalized=static_cast<double>(depth)*(1.0/127.0);
            amount=static_cast<double>((std::pow(10.0,static_cast<double>(normalized))-1.0)*(1.0/9.0));
        }
        else amount=static_cast<double>(static_cast<double>(depth)*(1.0/127.0));
        switch(destination)
        {
            case 1:case 2:
            {
                const double positive=amount*5.0,negative=amount*-5.0;
                return {static_cast<int>(negative*128.0),static_cast<int>(positive*128.0)};
            }
            case 3:return {0,static_cast<int>(amount*1024.0)};
            case 4:return {static_cast<int>(amount*2048.0),0};
            case 5:return {static_cast<int>(amount*-128.0),static_cast<int>(amount*128.0)};
            case 6:return {0,static_cast<int>(amount*512.0)};
            case 7:return {static_cast<int>(amount*-64.0),static_cast<int>(amount*64.0)};
            default:return {0,0};
        }
    }

}
