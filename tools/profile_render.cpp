// Non-gating offline profiling driver. DSP uses the actual ProcessorBridge.
#include "host_scenarios.h"
#include "RenderControls.h"
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#if defined(__linux__)
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#endif

namespace
{
double cpuMicros()
{
#if defined(__linux__)
    timespec t{};
    if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t)!=0)throw std::runtime_error("Thread clock unavailable");
    return t.tv_sec*1e6+t.tv_nsec*.001;
#else
    return std::clock()*(1e6/CLOCKS_PER_SEC);
#endif
}
// perf record --delay=-1 --control fifo:CONTROL,ACK. All I/O stays outside callbacks.
class PerfControl
{
#if defined(__linux__)
    int control{-1},ack{-1};
#endif
public:
    PerfControl()
    {
        const auto* c=std::getenv("SYNTH2_PERF_CONTROL");
        const auto* a=std::getenv("SYNTH2_PERF_ACK");
        if(!c && !a)return;
#if defined(__linux__)
        if(!c || !a)throw std::runtime_error("Both perf FIFO paths are required");
        control=open(c,O_RDWR|O_CLOEXEC);ack=open(a,O_RDWR|O_CLOEXEC);
        if(control<0 || ack<0)throw std::runtime_error("Cannot open perf control FIFOs");
#else
        throw std::runtime_error("perf control requires Linux");
#endif
    }
    ~PerfControl()
    {
#if defined(__linux__)
        if(control>=0)close(control);
        if(ack>=0)close(ack);
#endif
    }
    void set(bool enabled)
    {
#if defined(__linux__)
        if(control<0)return;
        const std::string command=enabled ? "enable\n" : "disable\n";
        if(write(control,command.data(),command.size())!=static_cast<ssize_t>(command.size()))
            throw std::runtime_error("Cannot send perf command");
        std::string reply;
        for(;;)
        {
            pollfd p{ack,POLLIN,0};
            if(poll(&p,1,10000)!=1)throw std::runtime_error("perf acknowledgement timed out");
            char ch{};if(read(ack,&ch,1)!=1)throw std::runtime_error("Cannot read perf acknowledgement");
            if(ch=='\0')continue; // perf versions can include the string terminator in acknowledgements.
            if(ch=='\n')break;
            if(reply.size()>16)throw std::runtime_error("Invalid perf acknowledgement");
            reply+=ch;
        }
        if(reply!="ack")throw std::runtime_error("Unexpected perf acknowledgement");
#endif
    }
};
struct Case {int mode,resonance,notes,unison;bool lfo;};
std::string stimulus(const Case& c,int version)
{
    // Append explicit source controls to the self-authored dry host stimulus.
    auto text=hostvalidation::patch(c.unison);
    text.replace(text.find("ver=113"),7,"ver="+std::to_string(version));
    return text+"14,"+std::to_string(c.mode)+"\n19,80\n20,"+std::to_string(c.resonance)
        +"\n21,63\n22,0\n57,"+(c.lfo ? "1" : "0")+"\n41,3\n42,5\n43,64\n44,32\n";
}
int integer(const char* text,int low,int high)
{
    size_t used{};const std::string s(text);const int v=std::stoi(s,&used);
    if(used!=s.size() || v<low || v>high)throw std::runtime_error("Argument out of range: "+s);
    return v;
}
}
int main(int argc,char** argv) try
{
    if(argc<2)throw std::runtime_error("Usage: profile output.csv [--blocks N] [--rounds N] [--source-version 1|113] [--case mode resonance notes unison static|lfo]");
    const std::filesystem::path output=argv[1];
    if(std::filesystem::exists(output))throw std::runtime_error("Output must be fresh");
    int blocks=4096,rounds=1,version=113;std::vector<Case> cases;
    for(int i=2;i<argc;++i)
    {
        const std::string option=argv[i];
        if(option=="--blocks" && i+1<argc)blocks=integer(argv[++i],32,1000000);
        else if(option=="--rounds" && i+1<argc)rounds=integer(argv[++i],1,20);
        else if(option=="--source-version" && i+1<argc)
        {
            version=integer(argv[++i],1,113);
            if(version!=1 && version!=113)throw std::runtime_error("Profile source version must be 1 or 113");
        }
        else if(option=="--case" && i+5<argc && cases.empty())
        {
            const int mode=integer(argv[++i],0,4),resonance=integer(argv[++i],0,127);
            const int notes=integer(argv[++i],1,32),unison=integer(argv[++i],1,8);
            const std::string source=argv[++i];
            if(source!="static" && source!="lfo")throw std::runtime_error("Expected static or lfo");
            cases.push_back({mode,resonance,notes,unison,source=="lfo"});
        }
        else throw std::runtime_error("Unknown or incomplete option: "+option);
    }
    if(cases.empty())for(int mode=0;mode<5;++mode)for(int resonance:{0,120})
        for(auto voices:{std::pair{1,1},std::pair{32,1},std::pair{4,8}})
            for(bool lfo:{false,true})
                if(version!=1 || voices.second==1)
                    cases.push_back({mode,resonance,voices.first,voices.second,lfo});
    PerfControl control;
    std::ofstream csv(output);if(!csv)throw std::runtime_error("Cannot open output");
    csv<<std::setprecision(12)<<"round,source_version,mode,resonance,notes,unison,lfo,cores,blocks,cpu_mean_us,wall_mean_us,pitch_lfo,filter_lfo,fm_lfo,pulse_lfo,amplitude_lfo,pan_lfo,peak,final_block_hash\n";
    for(int round=0;round<rounds;++round)for(size_t at=0;at<cases.size();++at)
    {
        const auto& c=cases[(at+round*17)%cases.size()];
        auto p=hostvalidation::processor();hostvalidation::load(*p,stimulus(c,version));p->prepare(48000,64);
        synth2::audio::AudioBuffer<float> audio(2,64);synth2::audio::MidiBuffer midi;hostvalidation::notes(midi,c.notes);
        hostvalidation::process(*p,audio,midi);midi.clear();
        for(int i=0;i<128;++i)hostvalidation::process(*p,audio,midi);
        synth2::RenderControls mapped;
        if(!synth2::readMappedSynthParams(*p,mapped) || mapped.filterType!=c.mode
            || std::abs(mapped.resonanceNorm-c.resonance/127.f)>1e-6f || mapped.unisonVoices!=c.unison
            || mapped.lfo1.on!=c.lfo || (c.lfo && mapped.lfo1.dest!=2))throw std::runtime_error("Unexpected mapped controls");
        const int cores=hostvalidation::activeCores(*p);
        if(cores!=std::min(32,c.notes*c.unison))throw std::runtime_error("Unexpected core count");
        control.set(true);
        const auto wallStart=std::chrono::steady_clock::now();const auto cpuStart=cpuMicros();
        for(int i=0;i<blocks;++i)hostvalidation::process(*p,audio,midi);
        const auto cpu=cpuMicros()-cpuStart;
        const auto wall=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-wallStart).count();
        control.set(false);
        if(!hostvalidation::finite(audio))throw std::runtime_error("Nonfinite final output");
        const float peak=audio.getMagnitude(0,64);
        if(peak<1e-9f)throw std::runtime_error("Unexpected silent final block");
        uint64_t hash=14695981039346656037uLL;
        for(int ch=0;ch<2;++ch)for(int i=0;i<64;++i)
        {uint32_t word{};const float value=audio.getSample(ch,i);std::memcpy(&word,&value,4);hash=(hash^word)*1099511628211uLL;}
        csv<<round<<','<<version<<','<<c.mode<<','<<c.resonance<<','<<c.notes<<','<<c.unison<<','<<c.lfo<<','<<cores<<','<<blocks<<','<<cpu/blocks<<','<<wall/blocks<<','
            <<mapped.hasPitchLfo()<<','<<mapped.hasLfo(2)<<','<<mapped.hasLfo(5)<<','<<mapped.hasLfo(4)<<','
            <<mapped.hasLfo(3)<<','<<mapped.hasLfo(6)<<','<<peak<<','<<hash<<'\n';
        csv.flush();if(!csv)throw std::runtime_error("Cannot finish profile CSV");
    }
    return 0;
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
