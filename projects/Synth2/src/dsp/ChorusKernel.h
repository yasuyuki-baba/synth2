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
            const double milliseconds=normalized>=.5f ? static_cast<double>(normalized*30.0f)
                : std::pow(300.0,std::pow(static_cast<double>(normalized)*2.0,.6))*.05;
            return static_cast<float>(milliseconds*.001);
        }
        static float depthRatio(int dial) noexcept
        {
            return static_cast<float>((std::pow(100.0,static_cast<double>(std::clamp(dial,0,127)*(1.0f/127.0f)))-1.0)*(1.0/99.0));
        }
        static float rateHz(int dial) noexcept
        {
            return dial==0 ? .01f : static_cast<float>(std::pow(20000.0,static_cast<double>(std::clamp(dial,0,127)*(1.0f/127.0f)))*.02);
        }
        static double timeSeconds(double dial) noexcept
        {return interpolateDial(dial,[](int value){return timeSeconds(value);});}
        static double depthRatio(double dial) noexcept
        {return interpolateDial(dial,[](int value){return depthRatio(value);});}
        static double rateHz(double dial) noexcept
        {return interpolateDial(dial,[](int value){return rateHz(value);});}
        bool configure(int rate,int voices,int time,int depth,int speed,int feedback,int level) noexcept
        {
            if(rate<=0 || (voices!=1 && voices!=2 && voices!=4))return false;
            sampleRate=rate;type=voices;depthDial=depth;timeDial=time;rateDial=speed;
            delayFrames=static_cast<int>(timeSeconds(time)*static_cast<float>(rate));
            depthFrames=static_cast<int>(static_cast<float>(delayFrames)*depthRatio(depth));
            period=static_cast<int>(static_cast<float>(rate)/rateHz(speed));
            coefficient=static_cast<double>(std::sin(static_cast<double>(rateHz(speed))*pi/static_cast<double>(rate))*2.0);
            amplitudeScale=period>=2.0*depthFrames*pi ? 1.0 : static_cast<double>(period/(2.0*depthFrames*pi));
            depthExtent=static_cast<double>(depthFrames);
            updateContinuousCoordinates();
            setFeedbackDial(feedback);
            wet=static_cast<double>(level)*(1.0/127.0);
            write=static_cast<int>((read+delayFrames)&mask);
            return true;
        }
        void reset() noexcept
        {
            ring.fill(0);read=0;write=delayFrames&mask;phaseSin=0;phaseCos=.9999;silentFrames=tailFrames+1;
        }
        void setTimeDial(double dial) noexcept
        {
            timeDial=std::clamp(dial,0.0,127.0);
            delayExtent=interpolateFrames(timeDial,[&](int value){return static_cast<int>(timeSeconds(value)*static_cast<float>(sampleRate));});
            delayFrames=static_cast<int>(delayExtent);
            setDepthDial(depthDial);write=(read+delayFrames)&mask;setFeedbackDial(feedbackDial);
        }
        void setDepthDial(double dial) noexcept
        {
            depthDial=std::clamp(dial,0.0,127.0);
            depthExtent=interpolateDial(depthDial,[&](int value){return static_cast<double>(static_cast<int>(static_cast<float>(delayFrames)*depthRatio(value)));});
            depthFrames=static_cast<int>(depthExtent);
            updateScale();updateContinuousCoordinates();
        }
        void setRateDial(double dial) noexcept
        {
            rateDial=std::clamp(dial,0.0,127.0);
            periodExtent=interpolateFrames(rateDial,[&](int value){return static_cast<int>(static_cast<float>(sampleRate)/rateHz(value));});
            period=static_cast<int>(periodExtent);
            coefficient=interpolateDial(rateDial,[&](int value){return static_cast<double>(std::sin(static_cast<double>(rateHz(value))*pi/static_cast<double>(sampleRate))*2.0);});
            updateScale();updateContinuousCoordinates();
        }
        void setLevelDial(double dial) noexcept {wet=std::clamp(dial,0.0,127.0)*(1.0/127.0);}
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
        void setFeedbackDial(double dial) noexcept
        {
            feedbackDial=std::clamp(dial,0.0,127.0);
            feedbackGain=static_cast<double>(feedbackDial-64)*(1.0/64.0)*.99;
            const int old=tailFrames;
            const int repeats=feedbackGain==0 ? 0
                : static_cast<int>(std::log(1.0/32768.0)/std::log(std::abs(static_cast<double>(feedbackGain))));
            tailFrames=(repeats+2)*delayFrames;
            silentFrames=silentFrames>old ? tailFrames+1 : 0;
        }
        // Dry is the accumulated upstream stereo output. Optional send is the
        // separate delay contribution: ring input = send + dry * chorus level.
        void process(double* dry,const double* send,int frames,int* silent=nullptr) noexcept
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
            if(upstreamSilent)std::fill_n(dry,frames*2,1.0e-9);
            if(silent)*silent=0;
            for(int i=0;i<frames;++i)
            {
                const int at=(write+i)&mask;
                ring[2*at]=send ? dry[2*i]*wet+send[2*i] : dry[2*i]*wet;
                ring[2*at+1]=send ? dry[2*i+1]*wet+send[2*i+1] : dry[2*i+1]*wet;
            }
            if(phaseCos>.9999){phaseSin=0;phaseCos=.9999;}
            for(int i=0;i<frames;++i)
            {
                phaseSin-=coefficient*phaseCos;
                phaseCos+=phaseSin*coefficient;
                const double base=read-(delayExtent-delayFrames);
                const double leftPosition=base+phaseExtent*phaseSin;
                const double rightPosition=type==1 ? leftPosition : base+phaseExtent*phaseCos;
                const double left=continuousTap(leftPosition,0);
                const double right=continuousTap(rightPosition,1);
                if(type==4)
                {
                    const double mirrorLeft=continuousTap(2.0*base-leftPosition,0);
                    const double mirrorRight=continuousTap(2.0*base-rightPosition,1);
                    const double leftCross=left*.1+mirrorLeft*.9;
                    double leftMix=left*.9+mirrorLeft*.1;leftMix+=right*.7;
                    leftMix=mirrorRight*.3+leftMix;
                    double rightMix=right*.3+leftCross;rightMix+=mirrorRight*.7;
                    dry[2*i]=leftMix+dry[2*i];dry[2*i+1]=rightMix+dry[2*i+1];
                }
                else {dry[2*i]+=left;dry[2*i+1]+=right;}
                ring[2*write]+=left*feedbackGain;ring[2*write+1]+=right*feedbackGain;
                write=(write+1)&mask;read=(read+1)&mask;
            }
        }
        double getDelayFramesContinuous() const noexcept {return delayExtent;}
        double getPeriodContinuous() const noexcept {return periodExtent;}
        int getDelayFrames() const noexcept {return delayFrames;}
        int getDepthFrames() const noexcept {return depthFrames;}
        int getPeriod() const noexcept {return period;}
        double getCoefficient() const noexcept {return coefficient;}
        double getScale() const noexcept {return amplitudeScale;}
        int getTailFrames() const noexcept {return tailFrames;}
        int getSilentFrames() const noexcept {return silentFrames;}
        int getReadPosition() const noexcept {return read;}
        int getWritePosition() const noexcept {return write;}
        int getVoices() const noexcept {return type;}
        bool isEnabled() const noexcept {return enabled;}
        double getPhaseSin() const noexcept {return phaseSin;}
        double getPhaseCos() const noexcept {return phaseCos;}
    private:
        template<class Mapping> static double interpolateFrames(double dial,Mapping mapping) noexcept
        {
            const int low=static_cast<int>(dial);const double first=mapping(low);
            if(dial==low || low==127)return first;
            return first+(static_cast<double>(mapping(low+1))-first)*(dial-low);
        }
        void updateContinuousCoordinates() noexcept
        {
            delayExtent=interpolateFrames(timeDial,[&](int value){return static_cast<int>(timeSeconds(value)*static_cast<float>(sampleRate));});
            periodExtent=interpolateFrames(rateDial,[&](int value){return static_cast<int>(static_cast<float>(sampleRate)/rateHz(value));});
            const int t0=static_cast<int>(timeDial),d0=static_cast<int>(depthDial),r0=static_cast<int>(rateDial);
            const double fractions[]{timeDial-t0,depthDial-d0,rateDial-r0};
            phaseExtent=0;
            for(int t=0;t<2;++t)for(int d=0;d<2;++d)for(int r=0;r<2;++r)
            {
                const int time=std::min(127,t0+t),depth=std::min(127,d0+d),speed=std::min(127,r0+r);
                const int delay=static_cast<int>(timeSeconds(time)*static_cast<double>(sampleRate));
                const int extent=static_cast<int>(static_cast<double>(delay)*depthRatio(depth));
                const int cycle=static_cast<int>(static_cast<double>(sampleRate)/rateHz(speed));
                const double scale=cycle>=2.0*extent*pi ? 1.0 : static_cast<double>(cycle/(2.0*extent*pi));
                const int amplitude=static_cast<int>(static_cast<double>(extent)*scale);
                const double weight=(t ? fractions[0] : 1.0-fractions[0])*(d ? fractions[1] : 1.0-fractions[1])*(r ? fractions[2] : 1.0-fractions[2]);
                phaseExtent+=amplitude*weight;
            }
        }
        double continuousTap(double position,int channel) const noexcept
        {
            position=std::fmod(position,8192.0);if(position<0)position+=8192.0;
            const int index=static_cast<int>(position),next=(index+1)&mask;
            const double fraction=static_cast<double>(position-index);
            const double a=ring[2*index+channel],b=ring[2*next+channel];
            return a+(b-a)*fraction;
        }
        void updateScale() noexcept
        {
            const double extent=depthExtent;
            amplitudeScale=period>=2.0*extent*pi ? 1.0 : static_cast<double>(period/(2.0*extent*pi));
        }
        static constexpr int mask=8191;
        static constexpr double pi=3.14159265358979323846;
        std::array<double,16384> ring {};
        int sampleRate{44100},type{1},delayFrames{},depthFrames{},period{},read{},write{};
        double depthDial{},feedbackDial{64},depthExtent{},timeDial{},rateDial{};
        double delayExtent{},periodExtent{},phaseExtent{};
        int tailFrames{1},silentFrames{99999999};
        bool enabled{true};
        double coefficient{},amplitudeScale{1},feedbackGain{},wet{},phaseSin{},phaseCos{.9999};
    };
}
