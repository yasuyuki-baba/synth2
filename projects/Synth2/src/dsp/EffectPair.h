#pragma once
#include "DelayKernel.h"
#include "ChorusKernel.h"
#include "EffectControls.h"
#include "FractionalDial.h"
#include <memory>

namespace synth2::dsp
{
    // Canonical planar adapter for native dry/send routing. The optional
    // semantic silence flag preserves device tails and retired state.
    class EffectPair
    {
    public:
        void prepare(double rate) noexcept {sampleRate=static_cast<int>(std::clamp(rate,1.0,384000.0));reset();}
        void reset() noexcept {delay->reset();chorus.reset();initialized=false;}
        template<class DelaySettings,class ChorusSettings>
        void setParams(const DelaySettings& d,const ChorusSettings& c,double bpm) noexcept
        {
            std::array<double,12> next={fractionalDial(d.time),fractionalDial(d.feedback),fractionalDial(d.level),static_cast<double>(std::clamp(d.type,0,2)),fractionalDial(d.spread),fractionalDial(d.tone),
                static_cast<double>(c.type==1 || c.type==4 ? c.type : 2),fractionalDial(c.time),fractionalDial(c.depth),fractionalDial(c.rate),fractionalDial(c.feedback),fractionalDial(c.level)};
            if(!std::isfinite(bpm) || bpm<=0)bpm=120;
            auto seconds=[&]{return interpolateDial(next[0],[&](int value){return EffectControls::delaySeconds(value,bpm);});};
            auto spread=[&]{return interpolateDial(next[4],[](int value){return EffectControls::spreadSeconds(value);});};
            auto integer=[](double value){return static_cast<int>(std::round(value));};
            if(!initialized)
            {
                delay->configure(sampleRate,integer(next[3]),seconds(),spread(),integer(next[1]),integer(next[2]),integer(next[5]));
                if(next[1]!=integer(next[1]))delay->setFeedbackDial(next[1]);
                if(next[2]!=integer(next[2]))delay->setMixDial(next[2]);
                if(next[5]!=integer(next[5]))delay->setToneDial(next[5]);
                {const auto frames=delayFrames(next[0],next[4],bpm);delay->setFrameTargets(frames[0],frames[1]);}
                delay->reset();
                chorus.configure(sampleRate,integer(next[6]),integer(next[7]),integer(next[8]),integer(next[9]),integer(next[10]),integer(next[11]));
                if(next[7]!=integer(next[7]))chorus.setTimeDial(next[7]);
                if(next[8]!=integer(next[8]))chorus.setDepthDial(next[8]);
                if(next[9]!=integer(next[9]))chorus.setRateDial(next[9]);
                if(next[10]!=integer(next[10]))chorus.setFeedbackDial(next[10]);
                if(next[11]!=integer(next[11]))chorus.setLevelDial(next[11]);
                chorus.reset();
            }
            else
            {
                if(next[0]!=previous[0] || bpm!=tempo)delay->setTimeSeconds(seconds());
                if(next[1]!=previous[1])delay->setFeedbackDial(next[1]);
                if(next[2]!=previous[2])delay->setMixDial(next[2]);
                if(next[3]!=previous[3])delay->setType(next[3]);
                if(next[4]!=previous[4])delay->setSpreadSeconds(spread());
                if(next[5]!=previous[5])delay->setToneDial(next[5]);
                if(next[6]!=previous[6])chorus.setVoices(next[6]);
                if(next[7]!=previous[7])chorus.setTimeDial(next[7]);
                if(next[8]!=previous[8])chorus.setDepthDial(next[8]);
                if(next[9]!=previous[9])chorus.setRateDial(next[9]);
                if(next[10]!=previous[10])chorus.setFeedbackDial(next[10]);
                if(next[11]!=previous[11])chorus.setLevelDial(next[11]);
            }
            if(initialized && (next[0]!=previous[0] || next[4]!=previous[4] || bpm!=tempo))
            {const auto frames=delayFrames(next[0],next[4],bpm);delay->setFrameTargets(frames[0],frames[1]);}
            delay->setEnabled(d.enable);chorus.setEnabled(c.enabled);
            previous=next;tempo=bpm;initialized=true;
        }
        void process(double* left,double* right,int frames) noexcept
        {
            processInternal(left,right,frames,nullptr);
        }
        void process(double* left,double* right,int frames,bool& silent) noexcept
        {
            processInternal(left,right,frames,&silent);
        }
        std::array<uint32_t,9> snapshot() const noexcept
        {
            std::array<uint32_t,9> words {static_cast<uint32_t>(delay->getSilentFrames()),static_cast<uint32_t>(delay->getTailFrames()),
                static_cast<uint32_t>(delay->getWritePosition()),static_cast<uint32_t>(chorus.getSilentFrames()),static_cast<uint32_t>(chorus.getTailFrames()),
                static_cast<uint32_t>(chorus.getReadPosition()),static_cast<uint32_t>(chorus.getWritePosition())};
            const float sine=static_cast<float>(chorus.getPhaseSin()),cosine=static_cast<float>(chorus.getPhaseCos());
            std::memcpy(&words[7],&sine,4);std::memcpy(&words[8],&cosine,4);return words;
        }
    private:
        void processInternal(double* left,double* right,int frames,bool* silent) noexcept
        {
            const bool inputSilent=silent && *silent;bool allSilent=true;
            for(int offset=0;offset<frames;)
            {
                const int count=std::min(256,frames-offset);
                for(int i=0;i<count;++i){dry[2*i]=left[offset+i];dry[2*i+1]=right ? right[offset+i] : left[offset+i];}
                int flag=inputSilent;
                const bool valid=delay->process(dry.data(),send.data(),count,silent ? &flag : nullptr);
                chorus.process(dry.data(),valid ? send.data() : nullptr,count,silent ? &flag : nullptr);
                allSilent=allSilent && flag!=0;
                for(int i=0;i<count;++i)
                {
                    left[offset+i]=right ? dry[2*i] : (dry[2*i]+dry[2*i+1])*.5;
                    if(right)right[offset+i]=dry[2*i+1];
                }
                offset+=count;
            }
            if(silent)*silent=allSilent;
        }
        std::array<double,2> delayFrames(double time,double spread,double bpm) const noexcept
        {
            const int t0=static_cast<int>(time),t1=std::min(127,t0+1);
            const int s0=static_cast<int>(spread),s1=std::min(127,s0+1);
            auto anchor=[&](int t,int s){
                const int nominal=static_cast<int>(static_cast<float>(sampleRate)*EffectControls::delaySeconds(t,static_cast<float>(bpm)));
                const int aligned=nominal-nominal%4;
                const float offset=EffectControls::spreadSeconds(s);
                const int l=aligned+(offset>=0 ? static_cast<int>(static_cast<float>(sampleRate)*offset) : 0);
                const int r=aligned+(offset<0 ? static_cast<int>(static_cast<float>(sampleRate)*-offset) : 0);
                const int minimum=static_cast<int>(static_cast<double>(sampleRate)*.0001);
                return std::array<double,2>{static_cast<double>(std::clamp(l,minimum,131072-256)),static_cast<double>(std::clamp(r,minimum,131072-256))};
            };
            const auto a=anchor(t0,s0),b=anchor(t0,s1),c=anchor(t1,s0),d=anchor(t1,s1);
            std::array<double,2> result{};
            for(int channel=0;channel<2;++channel)
            {
                const double low=a[channel]+(b[channel]-a[channel])*(spread-s0);
                const double high=c[channel]+(d[channel]-c[channel])*(spread-s0);
                result[channel]=low+(high-low)*(time-t0);
            }
            return result;
        }
        std::unique_ptr<DelayKernel> delay=std::make_unique<DelayKernel>();
        ChorusKernel chorus;
        std::array<double,512> dry {},send {};
        std::array<double,12> previous {};
        int sampleRate{44100};double tempo{120};bool initialized{};
    };
}
