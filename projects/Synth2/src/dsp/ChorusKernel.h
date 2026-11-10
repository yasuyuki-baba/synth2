#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "FractionalDial.h"

namespace synth2::dsp
{
    // Win64 CChorus x1/x2/x4 recurrence used by EffectPair. Fractional dials
    // interpolate compatible control knots. All settings share real tap coordinates;
    // integer knots preserve control mapping, oscillator recurrence and tail timing.
    class ChorusKernel
    {
    public:
        static float timeSeconds(int dial) noexcept
        {
            const float normalized=std::clamp(dial,0,127)*(1.0f/127.0f);
            const float milliseconds=normalized>=.5f ? static_cast<float>(normalized*30.0f)
                : std::pow(300.0f,std::pow(static_cast<float>(normalized)*2.0f,.6f))*.05f;
            return static_cast<float>(milliseconds*.001f);
        }
        static float depthRatio(int dial) noexcept
        {
            return static_cast<float>((std::pow(100.0f,static_cast<float>(std::clamp(dial,0,127)*(1.0f/127.0f)))-1.0f)*(1.0f/99.0f));
        }
        static float rateHz(int dial) noexcept
        {
            return dial==0 ? .01f : static_cast<float>(std::pow(20000.0f,static_cast<float>(std::clamp(dial,0,127)*(1.0f/127.0f)))*.02f);
        }
        static float timeSeconds(float dial) noexcept
        {return interpolateDial(dial,[](int value){return timeSeconds(value);});}
        static float depthRatio(float dial) noexcept
        {return interpolateDial(dial,[](int value){return depthRatio(value);});}
        static float rateHz(float dial) noexcept
        {return interpolateDial(dial,[](int value){return rateHz(value);});}
        bool configure(int rate,int voices,int time,int depth,int speed,int feedback,int level) noexcept
        {
            if(rate<=0 || (voices!=1 && voices!=2 && voices!=4))return false;
            sampleRate=rate;type=voices;depthDial=depth;timeDial=time;rateDial=speed;
            delayFrames=static_cast<int>(timeSeconds(time)*static_cast<float>(rate));
            depthFrames=static_cast<int>(static_cast<float>(delayFrames)*depthRatio(depth));
            period=static_cast<int>(static_cast<float>(rate)/rateHz(speed));
            coefficient=static_cast<float>(std::sin(static_cast<float>(rateHz(speed))*pi/static_cast<float>(rate))*2.0f);
            amplitudeScale=period>=2.0f*depthFrames*pi ? 1.0f : static_cast<float>(period/(2.0f*depthFrames*pi));
            depthExtent=static_cast<float>(depthFrames);
            updateContinuousCoordinates();
            setFeedbackDial(feedback);
            wet=static_cast<float>(level)*(1.0f/127.0f);
            write=static_cast<int>((read+delayFrames)&mask);
            return true;
        }
        void reset() noexcept
        {
            ring.fill(0);read=0;write=delayFrames&mask;phaseSin=0;phaseCos=initialCosine;silentFrames=tailFrames+1;
        }
        void setTimeDial(float dial) noexcept
        {
            timeDial=std::clamp(dial,0.0f,127.0f);
            delayExtent=interpolateFrames(timeDial,[&](int value){return static_cast<int>(timeSeconds(value)*static_cast<float>(sampleRate));});
            delayFrames=static_cast<int>(delayExtent);
            setDepthDial(depthDial);write=(read+delayFrames)&mask;setFeedbackDial(feedbackDial);
        }
        void setDepthDial(float dial) noexcept
        {
            depthDial=std::clamp(dial,0.0f,127.0f);
            depthExtent=interpolateDial(depthDial,[&](int value){return static_cast<float>(static_cast<int>(static_cast<float>(delayFrames)*depthRatio(value)));});
            depthFrames=static_cast<int>(depthExtent);
            updateScale();updateContinuousCoordinates();
        }
        void setRateDial(float dial) noexcept
        {
            rateDial=std::clamp(dial,0.0f,127.0f);
            periodExtent=interpolateFrames(rateDial,[&](int value){return static_cast<int>(static_cast<float>(sampleRate)/rateHz(value));});
            period=static_cast<int>(periodExtent);
            coefficient=interpolateDial(rateDial,[&](int value){return static_cast<float>(std::sin(static_cast<float>(rateHz(value))*pi/static_cast<float>(sampleRate))*2.0f);});
            updateScale();updateContinuousCoordinates();
        }
        void setLevelDial(float dial) noexcept {wet=std::clamp(dial,0.0f,127.0f)*(1.0f/127.0f);}
        void setEnabled(bool value) noexcept
        {
            if(value && !enabled)ring.fill(0);
            enabled=value;
        }
        bool setVoices(int voices) noexcept
        {
            if(voices!=1 && voices!=2 && voices!=4)return false;
            type=voices;return true;
        }
        void setFeedbackDial(float dial) noexcept
        {
            feedbackDial=std::clamp(dial,0.0f,127.0f);
            feedbackGain=static_cast<float>(feedbackDial-64)*(1.0f/64.0f)*.99f;
            const int old=tailFrames;
            const int repeats=feedbackGain==0 ? 0
                : static_cast<int>(std::log(1.0f/32768.0f)/std::log(std::abs(static_cast<float>(feedbackGain))));
            tailFrames=(repeats+2)*delayFrames;
            silentFrames=silentFrames>old ? tailFrames+1 : 0;
        }
        // Dry is the accumulated upstream stereo output. Optional send is the
        // separate delay contribution: ring input = send + dry * chorus level.
        void process(float* dry,const float* send,int frames,int* silent=nullptr) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            const bool upstreamSilent=silent && *silent;
            if(!enabled)
            {
                if(send)
                {
                    if(upstreamSilent)std::copy_n(send,frames*2,dry);
                    else for(int i=0;i<frames*2;++i)dry[i]+=send[i];
                    if(silent)*silent=0;
                }
                return;
            }
            if(upstreamSilent && !send)
            {
                if(silentFrames>tailFrames)return;
                silentFrames+=frames;
            }
            else silentFrames=0;
            if(upstreamSilent)std::fill_n(dry,frames*2,1.0e-9f);
            if(silent)*silent=0;
            for(int i=0;i<frames;++i)
            {
                const int at=(write+i)&mask;
                ring[2*at]=send ? dry[2*i]*wet+send[2*i] : dry[2*i]*wet;
                ring[2*at+1]=send ? dry[2*i+1]*wet+send[2*i+1] : dry[2*i+1]*wet;
            }
            if(phaseCos>initialCosine){phaseSin=0;phaseCos=initialCosine;}
            for(int i=0;i<frames;++i)
            {
                const float clockCoefficient=static_cast<float>(coefficient);
                phaseSin-=clockCoefficient*phaseCos;
                phaseCos+=phaseSin*clockCoefficient;
                const float base=read-(delayExtent-delayFrames);
                const float leftPosition=base+phaseExtent*phaseSin;
                const float rightPosition=type==1 ? leftPosition : base+phaseExtent*phaseCos;
                const float left=continuousTap(leftPosition,0);
                const float right=continuousTap(rightPosition,1);
                if(type==4)
                {
                    const float mirrorLeft=continuousTap(2.0f*base-leftPosition,0);
                    const float mirrorRight=continuousTap(2.0f*base-rightPosition,1);
                    const float leftCross=left*.1f+mirrorLeft*.9f;
                    float leftMix=left*.9f+mirrorLeft*.1f;leftMix+=right*.7f;
                    leftMix=mirrorRight*.3f+leftMix;
                    float rightMix=right*.3f+leftCross;rightMix+=mirrorRight*.7f;
                    dry[2*i]=leftMix+dry[2*i];dry[2*i+1]=rightMix+dry[2*i+1];
                }
                else {dry[2*i]+=left;dry[2*i+1]+=right;}
                ring[2*write]+=left*feedbackGain;ring[2*write+1]+=right*feedbackGain;
                write=(write+1)&mask;read=(read+1)&mask;
            }
        }
        float getDelayFramesContinuous() const noexcept {return delayExtent;}
        float getPeriodContinuous() const noexcept {return periodExtent;}
        int getDelayFrames() const noexcept {return delayFrames;}
        int getDepthFrames() const noexcept {return depthFrames;}
        int getPeriod() const noexcept {return period;}
        float getCoefficient() const noexcept {return coefficient;}
        float getScale() const noexcept {return amplitudeScale;}
        int getTailFrames() const noexcept {return tailFrames;}
        int getSilentFrames() const noexcept {return silentFrames;}
        int getReadPosition() const noexcept {return read;}
        int getWritePosition() const noexcept {return write;}
        int getVoices() const noexcept {return type;}
        bool isEnabled() const noexcept {return enabled;}
        float getPhaseSin() const noexcept {return phaseSin;}
        float getPhaseCos() const noexcept {return phaseCos;}
    private:
        template<class Mapping> static float interpolateFrames(float dial,Mapping mapping) noexcept
        {
            const int low=static_cast<int>(dial);const float first=mapping(low);
            if(dial==low || low==127)return first;
            return first+(static_cast<float>(mapping(low+1))-first)*(dial-low);
        }
        void updateContinuousCoordinates() noexcept
        {
            delayExtent=interpolateFrames(timeDial,[&](int value){return static_cast<int>(timeSeconds(value)*static_cast<float>(sampleRate));});
            periodExtent=interpolateFrames(rateDial,[&](int value){return static_cast<int>(static_cast<float>(sampleRate)/rateHz(value));});
            const int t0=static_cast<int>(timeDial),d0=static_cast<int>(depthDial),r0=static_cast<int>(rateDial);
            const float fractions[]{timeDial-t0,depthDial-d0,rateDial-r0};
            phaseExtent=0;
            for(int t=0;t<2;++t)for(int d=0;d<2;++d)for(int r=0;r<2;++r)
            {
                const int time=std::min(127,t0+t),depth=std::min(127,d0+d),speed=std::min(127,r0+r);
                const int delay=static_cast<int>(timeSeconds(time)*static_cast<float>(sampleRate));
                const int extent=static_cast<int>(static_cast<float>(delay)*depthRatio(depth));
                const int cycle=static_cast<int>(static_cast<float>(sampleRate)/rateHz(speed));
                const float scale=cycle>=2.0f*extent*pi ? 1.0f : static_cast<float>(cycle/(2.0f*extent*pi));
                const int amplitude=static_cast<int>(static_cast<float>(extent)*scale);
                const float weight=(t ? fractions[0] : 1.0f-fractions[0])*(d ? fractions[1] : 1.0f-fractions[1])*(r ? fractions[2] : 1.0f-fractions[2]);
                phaseExtent+=amplitude*weight;
            }
        }
        float continuousTap(float position,int channel) const noexcept
        {
            position=std::fmod(position,8192.0f);if(position<0)position+=8192.0f;
            const int index=static_cast<int>(position),next=(index+1)&mask;
            const float fraction=static_cast<float>(position-index);
            const float a=ring[2*index+channel],b=ring[2*next+channel];
            return a+(b-a)*fraction;
        }
        void updateScale() noexcept
        {
            const float extent=depthExtent;
            amplitudeScale=period>=2.0f*extent*pi ? 1.0f : static_cast<float>(period/(2.0f*extent*pi));
        }
        static constexpr float initialCosine=.9999f;
        static constexpr int mask=8191;
        static constexpr float pi=3.14159265358979323846f;
        std::array<float,16384> ring {};
        int sampleRate{44100},type{1},delayFrames{},depthFrames{},period{},read{},write{};
        float depthDial{},feedbackDial{64},depthExtent{},timeDial{},rateDial{};
        float delayExtent{},periodExtent{},phaseExtent{};
        int tailFrames{1},silentFrames{99999999};
        bool enabled{true};
        float coefficient{},amplitudeScale{1},feedbackGain{},wet{};
        // The recursive modulation clock and its block reset compare native
        // phase words. They are not audio samples or delay-line storage.
        float phaseSin{},phaseCos{initialCosine};
    };
}
