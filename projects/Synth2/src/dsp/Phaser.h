#pragma once
#include "SilenceGate.h"
#include <algorithm>
#include <array>
#include <cmath>
#include "FractionalDial.h"

namespace synth2::dsp
{
    // Win64 CPhaser stereo wrapper/kernel 0x16960/0x16a20.
    class Phaser
    {
    public:
        void prepare(double rate,bool preserve=false) noexcept
        {
            sampleRate=static_cast<int>(rate);frequencyScale=4.0/static_cast<double>(sampleRate);
            silenceGate.prepare(rate,.025,preserve);
            if(!preserve)
            {
                stages={};feedbackState={};frequency=200;factor=0;minimum=200;maximum=10000;speed=.5;
                feedback=.7;pairs=1;depthDial=rateDial=feedbackDial=-1;
            }
            updateFactor();
        }
        void setDepth(double dial) noexcept
        {
            depthDial=std::clamp(dial,0.0,127.0);
            const double n=static_cast<double>(depthDial)*(1.0/127.0);
            maximum=std::pow(10.0,n)*1000.0;
            minimum=std::pow(10.0,static_cast<double>(127-depthDial)*(1.0/127.0))*100.0;
            if(frequency==0)frequency=minimum;
            updateFactor();
        }
        void setRate(double dial) noexcept
        {
            rateDial=std::clamp(dial,0.0,127.0);
            speed=interpolateDial(rateDial,[](int value){return value==0 ? .01 : static_cast<double>(std::pow(1000.0,static_cast<double>(value*(1.0/127.0)))*.02);});
            updateFactor();
        }
        void setFeedback(double dial) noexcept
        {
            feedbackDial=std::clamp(dial,0.0,127.0);feedback=static_cast<double>(feedbackDial-64)*(1.0/64.0)*.99;
        }
        void setPairs(int value) noexcept {pairs=std::clamp(value,1,6);}
        void setDials(double depth,double rate,double fb,int count) noexcept
        {
            if(depth!=depthDial)setDepth(depth);
            if(rate!=rateDial)setRate(rate);
            if(fb!=feedbackDial)setFeedback(fb);
            setPairs(count);
        }
        void process(double* left,double* right,int frames) noexcept
        {
            bool silent=false;process(left,right,frames,silent);
        }
        // The upstream graph supplies a semantic silence flag. Zero-valued
        // samples alone do not establish silence while a voice is rendering.
        void process(double* left,double* right,int frames,bool& silent) noexcept
        {
            if(!silenceGate.process(left,right,frames,silent,0.0))return;
            processActive(left,right,frames);
        }
        std::array<int,2> silenceSnapshot() const noexcept {return silenceGate.snapshot();}
        std::array<double,8> snapshot() const noexcept {return {minimum,maximum,speed,feedback,frequencyScale,frequency,factor,feedbackState[0]};}
    private:
        void processActive(double* left,double* right,int frames) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            if((frequency>maximum && factor>1.0) || (frequency<minimum && factor<1.0))factor=1.0/factor;
            for(int i=0;i<frames;++i)
            {
                frequency*=factor;
                const double f=frequency*frequencyScale,coefficient=(1.0-f)/(f+1.0);
                double values[] {feedbackState[0]*feedback+left[i],feedbackState[1]*feedback+(right ? right[i] : left[i])};
                for(int pair=0;pair<pairs;++pair)for(int channel=0;channel<2;++channel)
                {
                    auto& state=stages[static_cast<size_t>(channel)][static_cast<size_t>(pair)];
                    const double first=state[0]-values[channel]*coefficient;
                    const double product=first*coefficient;
                    state[0]=product+values[channel];
                    values[channel]=state[1]-product;
                    state[1]=values[channel]*coefficient+first;
                }
                feedbackState={values[0],values[1]};
                left[i]=left[i]*.5+values[0]*.5;
                if(right)right[i]=right[i]*.5+values[1]*.5;
            }
        }
        void updateFactor() noexcept
        {
            const double logarithm=std::log(maximum/minimum);
            const double halfPeriod=(static_cast<double>(sampleRate)/speed)*.5;
            const double next=std::exp(logarithm/halfPeriod);
            factor=factor>1.0 ? next : 1.0/next;
        }
        std::array<std::array<std::array<double,2>,6>,2> stages {};
        std::array<double,2> feedbackState {};
        SilenceGate silenceGate;
        int sampleRate=44100,pairs=1;
        double depthDial=-1,rateDial=-1,feedbackDial=-1;
        double minimum=200,maximum=10000,speed=.5,feedback=.7,frequencyScale=0,frequency=200,factor=0;
    };
}
