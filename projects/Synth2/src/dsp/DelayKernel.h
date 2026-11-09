#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace synth2::dsp
{
    // Independent Win64 delay types 0/1/2, first-order tone, live time/spread
    // enable control and upstream silence/tail retirement, routed by EffectPair.
    // Fractional tone uses adjacent native coefficient knots.
    class DelayKernel
    {
    public:
        bool configure(int rate,int type,double seconds,double spread,int feedback,int mix,int toneDial=64) noexcept
        {
            if(rate<10000 || rate>384000 || (type<0 || type>2) || !std::isfinite(seconds)
                || !std::isfinite(spread) || seconds<0 || seconds>100 || std::abs(spread)>1)return false;
            kind=type;
            sampleRate=rate;timeSeconds=seconds;spreadSeconds=spread;
            slewCoefficient=static_cast<double>(1.0-std::pow(.5,1.0/static_cast<double>(static_cast<double>(rate)*.15)));
            updateTargets();
            feedbackGain=std::min(.999,std::max(0.0,feedback*(1.0/127.0)));
            mix=std::clamp(mix,0,127);
            dryGain=mix<=64 ? 1.0 : (127-mix)*(1.0/63.0);
            wetGain=mix>64 ? 1.0 : (std::max(mix,1)-1)*(1.0/63.0);
            updateTail();
            configureTone(rate,toneDial);
            return true;
        }
        void reset() noexcept
        {
            ring.fill(0);write=0;previousInput.fill(0);previousOutput.fill(0);
            currentLeft=targetLeft;currentRight=targetRight;slewing=false;silentFrames=wrap32(static_cast<uint32_t>(tailFrames)+1u);
        }
        bool setTimeSeconds(double seconds) noexcept
        {
            if(!std::isfinite(seconds) || seconds<0 || seconds>100)return false;
            timeSeconds=seconds;updateTargets();updateTail();return true;
        }
        bool setSpreadSeconds(double seconds) noexcept
        {
            if(!std::isfinite(seconds) || std::abs(seconds)>1)return false;
            spreadSeconds=seconds;updateTargets();updateTail();return true;
        }
        // Control adapters interpolate compatible integer-setting frame targets
        // before entering this kernel. Keep their fractional coordinates intact.
        bool setFrameTargets(double leftFrames,double rightFrames) noexcept
        {
            if(!std::isfinite(leftFrames) || !std::isfinite(rightFrames))return false;
            const double minimum=static_cast<int>(static_cast<double>(sampleRate)*.0001);
            targetLeft=std::clamp(leftFrames,minimum,131072.0-256.0);
            targetRight=std::clamp(rightFrames,minimum,131072.0-256.0);
            left=static_cast<int>(targetLeft);right=static_cast<int>(targetRight);
            if(silentFrames<=tailFrames && enabled)slewing=true;
            else {currentLeft=targetLeft;currentRight=targetRight;}
            updateTail();return true;
        }
        double getLeftFramesContinuous() const noexcept {return targetLeft;}
        double getRightFramesContinuous() const noexcept {return targetRight;}
        void setFeedbackDial(double dial) noexcept
        {
            feedbackGain=std::min(.999,std::clamp(dial,0.0,127.0)*(1.0/127.0));updateTail();
        }
        bool setType(int value) noexcept
        {
            if(value<0 || value>2)return false;kind=value;updateTail();return true;
        }
        void setMixDial(double dial) noexcept
        {
            dial=std::clamp(dial,0.0,127.0);
            dryGain=dial<=64 ? 1.0 : (127-dial)*(1.0/63.0);
            wetGain=dial>64 ? 1.0 : (std::max(dial,1.0)-1)*(1.0/63.0);
        }
        void setToneDial(double dial) noexcept
        {
            dial=std::clamp(dial,0.0,127.0);
            const int low=static_cast<int>(dial);
            configureTone(sampleRate,low);
            if(dial==low)return;
            const std::array<double,3> first{toneB0,toneB1,toneFeedback};
            configureTone(sampleRate,low+1);
            const double fraction=dial-low;
            toneB0=first[0]+(toneB0-first[0])*fraction;
            toneB1=first[1]+(toneB1-first[1])*fraction;
            toneFeedback=first[2]+(toneFeedback-first[2])*fraction;
            toneBypassed=toneB0==1.0 && toneB1==0.0 && toneFeedback==0.0;
        }
        void setEnabled(bool value) noexcept
        {
            if(value && !enabled)
            {
                ring.fill(0);write=0;slewing=false;currentLeft=targetLeft;currentRight=targetRight;
            }
            enabled=value;
        }
        // Returns send validity. When disabled, dry and the stale send are unchanged;
        // consumers must ignore the invalid send. Enabled output is dry plus a
        // distinct send for the downstream Chorus to modulate or add on bypass.
        // Optional silent mirrors the upstream flag: active tails clear it; an
        // expired tail leaves it set and preserves both buffers and the ring.
        bool process(double* dry,double* send,int frames,int* silent=nullptr) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            if(!enabled)return false;
            if(silent && *silent)
            {
                if(!slewing)
                {
                    if(silentFrames>tailFrames)return false;
                    silentFrames=wrap32(static_cast<uint32_t>(silentFrames)+static_cast<uint32_t>(frames));
                }
                *silent=0;std::fill_n(dry,frames*2,0.0);
            }
            else silentFrames=0;
            processFrames(dry,send,frames);
            return true;
        }
        int getLeftFrames() const noexcept {return left;}
        int getRightFrames() const noexcept {return right;}
        double getCurrentLeft() const noexcept {return currentLeft;}
        double getCurrentRight() const noexcept {return currentRight;}
        bool isSlewing() const noexcept {return slewing;}
        int getTailFrames() const noexcept {return tailFrames;}
        int getSilentFrames() const noexcept {return silentFrames;}
        int getWritePosition() const noexcept {return write;}
    private:
        void updateTargets() noexcept
        {
            const int nominal=static_cast<int>(static_cast<double>(sampleRate)*timeSeconds);
            const int aligned=nominal-nominal%4;
            left=aligned+(spreadSeconds>=0 ? static_cast<int>(static_cast<double>(sampleRate)*spreadSeconds) : 0);
            right=aligned+(spreadSeconds<0 ? static_cast<int>(static_cast<double>(sampleRate)*-spreadSeconds) : 0);
            const int minimum=static_cast<int>(static_cast<double>(sampleRate)*.0001);
            left=std::clamp(left,minimum,131072-256);right=std::clamp(right,minimum,131072-256);
            targetLeft=left;targetRight=right;
            if(silentFrames<=tailFrames && enabled)slewing=true;
            else {currentLeft=left;currentRight=right;}
        }
        double continuousTap(double position,int channel) const noexcept
        {
            position=std::fmod(position,131072.0);if(position<0)position+=131072.0;
            const int at=static_cast<int>(position),next=(at+1)&mask;
            const double fraction=static_cast<double>(position-at);
            const double a=kind==2 ? ring[(channel==0 ? 0 : second)+at] : ring[2*at+channel];
            if(fraction==0)return a;
            const double b=kind==2 ? ring[(channel==0 ? 0 : second)+next] : ring[2*next+channel];
            return a+(b-a)*fraction;
        }
        void processFrames(double* dry,double* send,int frames) noexcept
        {
            for(int i=0;i<frames;++i)
            {
                if(slewing)
                {
                    currentLeft+=(targetLeft-currentLeft)*static_cast<double>(slewCoefficient);
                    currentRight+=(targetRight-currentRight)*static_cast<double>(slewCoefficient);
                    if(std::abs(currentLeft-targetLeft)<.1)currentLeft=targetLeft;
                    if(std::abs(currentRight-targetRight)<.1)currentRight=targetRight;
                    if(currentLeft==targetLeft && currentRight==targetRight)slewing=false;
                }
                double taps[]{continuousTap(write-currentLeft,0),continuousTap(write-currentRight,1)};
                if(!toneBypassed)for(int channel=0;channel<2;++channel)
                {
                    const double input=taps[channel];
                    const double output=input*toneB0+previousInput[channel]*toneB1+previousOutput[channel]*toneFeedback;
                    previousInput[channel]=input;previousOutput[channel]=output;taps[channel]=output;
                }
                const int at=2*i;
                if(kind==2)
                {
                    const double input=(dry[at+1]+dry[at])*.5;
                    ring[write]=input+taps[1]*feedbackGain;ring[second+write]=taps[0];
                }
                else
                {
                    ring[2*write]=dry[at+(kind==0 ? 0 : 1)]+taps[kind==0 ? 0 : 1]*feedbackGain;
                    ring[2*write+1]=dry[at+(kind==0 ? 1 : 0)]+taps[kind==0 ? 1 : 0]*feedbackGain;
                }
                dry[at]*=dryGain;dry[at+1]*=dryGain;
                send[at]=taps[0]*wetGain;send[at+1]=taps[1]*wetGain;
                write=(write+1)&mask;
            }
        }
        static int wrap32(uint32_t bits) noexcept
        {
            int result;std::memcpy(&result,&bits,4);return result;
        }
        void updateTail() noexcept
        {
            const int old=tailFrames;
            const int repeats=feedbackGain==0 ? 0
                : static_cast<int>(std::log(1.0/32768.0)/std::log(static_cast<double>(feedbackGain)));
            // Native imul keeps the low 32 bits, including overflow for a long
            // type-2 delay with high feedback. Avoid signed-overflow UB here.
            const uint32_t bits=static_cast<uint32_t>(repeats+2)*static_cast<uint32_t>(std::max(left,right))
                *static_cast<uint32_t>(kind==2 ? 2 : 1);
            tailFrames=wrap32(bits);
            if(silentFrames>old)silentFrames=wrap32(static_cast<uint32_t>(tailFrames)+1u);
        }
        // Integer dials provide compatible coefficient knots. Every tone setting
        // uses the same recurrence and retains the same input/output history.
        void configureTone(int rate,int dial) noexcept
        {
#if defined(__clang__)
#pragma clang fp contract(off)
#endif
            const int tone=std::clamp(dial,0,127);
            toneBypassed=tone==64;
            if(toneBypassed){toneB0=1; toneB1=toneFeedback=0;return;}
            const double normalized=tone<64 ? tone*(1.0/63.0) : (tone-65)*(1.0/62.0);
            const double cutoff=static_cast<double>(tone<64 ? std::pow(110.0,normalized)*200.0
                : std::pow(800.0,normalized)*10.0);
            const double frequency=static_cast<double>(static_cast<double>(cutoff*2.0)*3.14159265358979323846);
            const double twiceRate=static_cast<double>(static_cast<double>(rate)*2.0);
            const double inverse=static_cast<double>(1.0/static_cast<double>(frequency+twiceRate));
            toneB0=(tone<64 ? frequency : twiceRate)*inverse;
            toneB1=tone<64 ? toneB0 : -toneB0;
            toneFeedback=(twiceRate-frequency)*inverse;
        }
        static constexpr int mask=131071;
        static constexpr int second=131076; // native second allocation view: +0x80010 bytes
        std::array<double,262148> ring {};
        std::array<double,2> previousInput {},previousOutput {};
        int sampleRate{44100};
        double timeSeconds{},spreadSeconds{},slewCoefficient{};
        double currentLeft{256},currentRight{256};
        bool enabled{true},slewing{};
        double targetLeft{256},targetRight{256};
        int tailFrames{1},silentFrames{99999999};
        double toneFeedback{},toneB0{1},toneB1{};
        bool toneBypassed{true};
        int left{256},right{256},kind{},write{};
        double feedbackGain{},wetGain{},dryGain{1};
    };
}
