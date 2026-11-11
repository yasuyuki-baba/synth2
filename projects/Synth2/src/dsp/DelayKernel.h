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
        bool configure(int rate,int type,float seconds,float spread,int feedback,int mix,int toneDial=64) noexcept
        {
            if(rate<10000 || rate>384000 || (type<0 || type>2) || !std::isfinite(seconds)
                || !std::isfinite(spread) || seconds<0 || seconds>100 || std::abs(spread)>1)return false;
            kind=type;
            sampleRate=rate;timeSeconds=seconds;spreadSeconds=spread;
            slewCoefficient=static_cast<float>(1.0-std::pow(.5,1.0/static_cast<double>(static_cast<float>(rate)*.15f)));
            updateTargets();
            feedbackGain=std::min(.999f,std::max(0.0f,feedback*(1.0f/127.0f)));
            mix=std::clamp(mix,0,127);
            dryGain=mix<=64 ? 1.0f : (127-mix)*(1.0f/63.0f);
            wetGain=mix>64 ? 1.0f : (std::max(mix,1)-1)*(1.0f/63.0f);
            updateTail();
            configureTone(rate,toneDial);
            return true;
        }
        void reset() noexcept
        {
            ring.fill(0);write=0;previousInput.fill(0);previousOutput.fill(0);
            currentLeft=targetLeft;currentRight=targetRight;distanceLeft=distanceRight=0;slewing=false;silentFrames=wrap32(static_cast<uint32_t>(tailFrames)+1u);
        }
        bool setTimeSeconds(float seconds) noexcept
        {
            if(!std::isfinite(seconds) || seconds<0 || seconds>100)return false;
            timeSeconds=seconds;updateTargets();updateTail();return true;
        }
        bool setSpreadSeconds(float seconds) noexcept
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
            updateSlew();
            updateTail();return true;
        }
        double getLeftFramesContinuous() const noexcept {return targetLeft;}
        double getRightFramesContinuous() const noexcept {return targetRight;}
        void setFeedbackDial(float dial) noexcept
        {
            feedbackGain=std::min(.999f,std::clamp(dial,0.0f,127.0f)*(1.0f/127.0f));updateTail();
        }
        bool setType(int value) noexcept
        {
            if(value<0 || value>2)return false;kind=value;updateTail();return true;
        }
        void setMixDial(float dial) noexcept
        {
            dial=std::clamp(dial,0.0f,127.0f);
            dryGain=dial<=64 ? 1.0f : (127-dial)*(1.0f/63.0f);
            wetGain=dial>64 ? 1.0f : (std::max(dial,1.0f)-1)*(1.0f/63.0f);
        }
        void setToneDial(float dial) noexcept
        {
            dial=std::clamp(dial,0.0f,127.0f);
            const int low=static_cast<int>(dial);
            configureTone(sampleRate,low);
            if(dial==low)return;
            const std::array<float,3> first{toneB0,toneB1,toneFeedback};
            configureTone(sampleRate,low+1);
            const float fraction=dial-low;
            toneB0=first[0]+(toneB0-first[0])*fraction;
            toneB1=first[1]+(toneB1-first[1])*fraction;
            toneFeedback=first[2]+(toneFeedback-first[2])*fraction;
            toneBypassed=toneB0==1.0f && toneB1==0.0f && toneFeedback==0.0f;
        }
        void setEnabled(bool value) noexcept
        {
            if(value && !enabled)
            {
                ring.fill(0);write=0;slewing=false;currentLeft=targetLeft;currentRight=targetRight;distanceLeft=distanceRight=0;
            }
            enabled=value;
        }
        // Returns send validity. When disabled, dry and the stale send are unchanged;
        // consumers must ignore the invalid send. Enabled output is dry plus a
        // distinct send for the downstream Chorus to modulate or add on bypass.
        // Optional silent mirrors the upstream flag: active tails clear it; an
        // expired tail leaves it set and preserves both buffers and the ring.
        bool process(float* dry,float* send,int frames,int* silent=nullptr) noexcept
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
                *silent=0;std::fill_n(dry,frames*2,0.0f);
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
            const int nominal=static_cast<int>(static_cast<float>(sampleRate)*timeSeconds);
            const int aligned=nominal-nominal%4;
            left=aligned+(spreadSeconds>=0 ? static_cast<int>(static_cast<float>(sampleRate)*spreadSeconds) : 0);
            right=aligned+(spreadSeconds<0 ? static_cast<int>(static_cast<float>(sampleRate)*-spreadSeconds) : 0);
            const int minimum=static_cast<int>(static_cast<float>(sampleRate)*.0001f);
            left=std::clamp(left,minimum,131072-256);right=std::clamp(right,minimum,131072-256);
            targetLeft=left;targetRight=right;
            updateSlew();
        }
        void updateSlew() noexcept
        {
            if(silentFrames<=tailFrames && enabled)
            {
                distanceLeft=currentLeft-targetLeft;distanceRight=currentRight-targetRight;slewing=true;
            }
            else
            {
                currentLeft=targetLeft;currentRight=targetRight;distanceLeft=distanceRight=0;
            }
        }
        float continuousTap(double position,int channel) const noexcept
        {
            // Bounded delay coordinates are less than one ring away from write.
            if(position<0)position+=131072.0;
            const int whole=static_cast<int>(position);
            // Negative positions near zero can round to the ring length.
            const int at=whole&mask,next=(at+1)&mask;
            const float fraction=static_cast<float>(position-whole);
            const float a=kind==2 ? ring[(channel==0 ? 0 : second)+at] : ring[2*at+channel];
            if(fraction==0)return a;
            const float b=kind==2 ? ring[(channel==0 ? 0 : second)+next] : ring[2*next+channel];
            return a+(b-a)*fraction;
        }
        void processFrames(float* dry,float* send,int frames) noexcept
        {
            for(int i=0;i<frames;++i)
            {
                if(slewing)
                {
                    // The target-relative state keeps shrinking even when an
                    // absolute-frame increment would round back to itself.
                    distanceLeft-=distanceLeft*static_cast<double>(slewCoefficient);
                    distanceRight-=distanceRight*static_cast<double>(slewCoefficient);
                    if(std::abs(distanceLeft)<.1f)distanceLeft=0;
                    if(std::abs(distanceRight)<.1f)distanceRight=0;
                    currentLeft=targetLeft+distanceLeft;currentRight=targetRight+distanceRight;
                    if(distanceLeft==0 && distanceRight==0)slewing=false;
                }
                float taps[]{continuousTap(write-currentLeft,0),continuousTap(write-currentRight,1)};
                if(!toneBypassed)for(int channel=0;channel<2;++channel)
                {
                    const float input=taps[channel];
                    const float output=input*toneB0+previousInput[channel]*toneB1+previousOutput[channel]*toneFeedback;
                    previousInput[channel]=input;previousOutput[channel]=output;taps[channel]=output;
                }
                const int at=2*i;
                if(kind==2)
                {
                    const float input=(dry[at+1]+dry[at])*.5f;
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
            const float normalized=tone<64 ? tone*(1.0f/63.0f) : (tone-65)*(1.0f/62.0f);
            const float cutoff=static_cast<float>(tone<64 ? std::pow(110.0,normalized)*200.0
                : std::pow(800.0,normalized)*10.0);
            const float frequency=static_cast<float>(static_cast<double>(cutoff*2.0f)*3.14159265358979323846);
            const float twiceRate=static_cast<float>(static_cast<double>(rate)*2.0);
            const float inverse=static_cast<float>(1.0/static_cast<double>(frequency+twiceRate));
            toneB0=(tone<64 ? frequency : twiceRate)*inverse;
            toneB1=tone<64 ? toneB0 : -toneB0;
            toneFeedback=(twiceRate-frequency)*inverse;
        }
        static constexpr int mask=131071;
        static constexpr int second=131076; // native second allocation view: +0x80010 bytes
        std::array<float,262148> ring {};
        std::array<float,2> previousInput {},previousOutput {};
        int sampleRate{44100};
        float timeSeconds{},spreadSeconds{},slewCoefficient{};
        double currentLeft{256},currentRight{256};
        double distanceLeft{},distanceRight{};
        bool enabled{true},slewing{};
        double targetLeft{256},targetRight{256};
        int tailFrames{1},silentFrames{99999999};
        float toneFeedback{},toneB0{1},toneB1{};
        bool toneBypassed{true};
        int left{256},right{256},kind{},write{};
        float feedbackGain{},wetGain{},dryGain{1};
    };
}
