#pragma once
#include <array>

namespace synth2::dsp
{
    // Native type0 child0x6cb0/0x7f60: three biased, feedback-coupled
    // positive waveshapers with retained one-pole and DC-blocking state.
    class BiasedOverdriveKernel
    {
        struct Stage
        {
            float bias=0,high=0,previous=0,low=0,shaped=0;
        };
    public:
        static float shape(float value) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            if(value<0)return 0;
            float denominator=value+0.4f;
            denominator*=value;denominator*=value;denominator*=2.0f;denominator+=1.0f;
            return -1.0f/denominator+1.0f;
        }
        void prepare(int rate,bool preserve) noexcept
        {
            if(!preserve)
            {
                stages={};configure(44100);
                // The native constructor settles both channels for600 frames
                // at44.1kHz before its later sample-rate setter changes poles.
                for(int i=0;i<600;++i){process(0,0);process(0,1);}
            }
            configure(rate);
        }
        float process(float value,int channel) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            for(int stage=0;stage<3;++stage)
            {
                auto& s=stages[channel][stage];
                float desired=1.0f-s.shaped;
                if(stage==0)desired*=5.0f;
                s.bias=(desired-s.bias)*biasCoefficient[stage]+s.bias;
                s.shaped=shape(value+s.bias);
                value=((s.shaped-s.previous)+s.high)*highCoefficient;
                s.previous=s.shaped;s.high=value;
                if(stage<2){s.low=(value-s.low)*lowCoefficient+s.low;value=s.low;}
            }
            return value;
        }
    private:
        void configure(int rate) noexcept
        {
            const float r=rate;
            biasCoefficient={static_cast<float>(540.3539364174444f/r),static_cast<float>(829.3804605477054f/r),static_cast<float>(1218.9379495928397f/r)};
            highCoefficient=static_cast<float>(1.0f/(194.77874452256717f/r+1.0f));
            lowCoefficient=static_cast<float>(41035.483241189875f/r);
        }
        std::array<std::array<Stage,3>,2> stages{};
        std::array<float,3> biasCoefficient{};
        float highCoefficient=0,lowCoefficient=0;
    };
}
