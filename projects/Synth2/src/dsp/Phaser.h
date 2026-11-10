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
        void prepare(float rate,bool preserve=false) noexcept
        {
            sampleRate=static_cast<int>(rate);frequencyScale=4.0f/static_cast<float>(sampleRate);
            silenceGate.prepare(rate,.025f,preserve);
            if(!preserve)
            {
                stages={};feedbackState={};frequency=200;factor=0;minimum=200;maximum=10000;speed=.5f;
                feedback=.7f;pairs=1;depthDial=rateDial=feedbackDial=-1;
            }
            updateFactor();
        }
        void setDepth(float dial) noexcept
        {
            depthDial=std::clamp(dial,0.0f,127.0f);
            maximum=interpolateDial(depthDial,maximumForDial);
            minimum=interpolateDial(depthDial,minimumForDial);
            if(frequency==0)frequency=minimum;
            updateFactor();
        }
        void setRate(float dial) noexcept
        {
            rateDial=std::clamp(dial,0.0f,127.0f);
            speed=interpolateDial(rateDial,speedForDial);
            updateFactor();
        }
        void setFeedback(float dial) noexcept
        {
            feedbackDial=std::clamp(dial,0.0f,127.0f);feedback=static_cast<float>(feedbackDial-64)*(1.0f/64.0f)*.99f;
        }
        void setPairs(int value) noexcept {pairs=std::clamp(value,1,6);}
        void setDials(float depth,float rate,float fb,int count) noexcept
        {
            if(depth!=depthDial)setDepth(depth);
            if(rate!=rateDial)setRate(rate);
            if(fb!=feedbackDial)setFeedback(fb);
            setPairs(count);
        }
        void process(float* left,float* right,int frames) noexcept
        {
            bool silent=false;process(left,right,frames,silent);
        }
        // The upstream graph supplies a semantic silence flag. Zero-valued
        // samples alone do not establish silence while a voice is rendering.
        void process(float* left,float* right,int frames,bool& silent) noexcept
        {
            if(!silenceGate.process(left,right,frames,silent,0.0f))return;
            processActive(left,right,frames);
        }
        std::array<int,2> silenceSnapshot() const noexcept {return silenceGate.snapshot();}
        std::array<float,8> snapshot() const noexcept {return {minimum,maximum,speed,feedback,frequencyScale,frequency,factor,feedbackState[0]};}
    private:
        void processActive(float* left,float* right,int frames) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            if((frequency>maximum && factor>1.0f) || (frequency<minimum && factor<1.0f))factor=sweepFactor(factor<1.0f);
            for(int i=0;i<frames;++i)
            {
                frequency*=factor;
                const float f=frequency*frequencyScale,coefficient=(1.0f-f)/(f+1.0f);
                float values[] {feedbackState[0]*feedback+left[i],feedbackState[1]*feedback+(right ? right[i] : left[i])};
                for(int pair=0;pair<pairs;++pair)for(int channel=0;channel<2;++channel)
                {
                    auto& state=stages[static_cast<size_t>(channel)][static_cast<size_t>(pair)];
                    const float first=state[0]-values[channel]*coefficient;
                    const float product=first*coefficient;
                    state[0]=product+values[channel];
                    values[channel]=state[1]-product;
                    state[1]=values[channel]*coefficient+first;
                }
                feedbackState={values[0],values[1]};
                left[i]=left[i]*.5f+values[0]*.5f;
                if(right)right[i]=right[i]*.5f+values[1]*.5f;
            }
        }
        // The sweep's multiplicative steps are calibrated control words. Keep
        // their integer knots before interpolation; audio histories remain Float32.
        static float maximumForDial(int dial) noexcept
        {return std::pow(10.0f,dial*(1.0f/127.0f))*1000.0f;}
        static float minimumForDial(int dial) noexcept
        {return std::pow(10.0f,(127-dial)*(1.0f/127.0f))*100.0f;}
        static float speedForDial(int dial) noexcept
        {return dial==0 ? .01f : static_cast<float>(std::pow(1000.0f,static_cast<float>(dial*(1.0f/127.0f)))*.02f);}
        float factorWord(float low,float high,float rate,bool rising) const noexcept
        {
            const float logarithm=std::log(static_cast<float>(high)/static_cast<float>(low));
            const float halfPeriod=(static_cast<float>(sampleRate)/static_cast<float>(rate))*.5f;
            const float next=std::exp(logarithm/halfPeriod);
            return rising ? next : 1.0f/next;
        }
        float sweepFactor(bool rising) const noexcept
        {
            if(depthDial<0 || rateDial<0)
                return factorWord(minimum,maximum,speed,rising);
            return interpolateDial(depthDial,[&](int depth) {
                return interpolateDial(rateDial,[&](int rate) {
                    return factorWord(minimumForDial(depth),maximumForDial(depth),speedForDial(rate),rising);
                });
            });
        }
        void updateFactor() noexcept {factor=sweepFactor(factor>1.0f);}
        std::array<std::array<std::array<float,2>,6>,2> stages {};
        std::array<float,2> feedbackState {};
        SilenceGate silenceGate;
        int sampleRate=44100,pairs=1;
        float depthDial=-1,rateDial=-1,feedbackDial=-1;
        float minimum=200,maximum=10000,speed=.5f,feedback=.7f,frequencyScale=0,frequency=200,factor=0;
    };
}
