#pragma once
#include <algorithm>
#include <array>

namespace synth2::dsp
{
    // Win64 device wrappers compare the retained counter before incrementing.
    class SilenceGate
    {
    public:
        void prepare(double rate,float seconds,bool preserve=false) noexcept
        {
            budget=static_cast<int>(static_cast<float>(static_cast<int>(rate))*seconds);
            if(!preserve)counter=0;
        }
        bool process(float* left,float* right,int frames,bool& silent,float fill) noexcept
        {
            if(silent)
            {
                if(counter>budget)return false;
                counter+=frames;std::fill_n(left,frames,fill);if(right)std::fill_n(right,frames,fill);silent=false;
            }
            else counter=0;
            return true;
        }
        std::array<int,2> snapshot() const noexcept {return {counter,budget};}
    private:
        int counter=0,budget=0;
    };
}
