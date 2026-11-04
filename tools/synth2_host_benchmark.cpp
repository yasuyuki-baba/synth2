// Offline callback timing; deadline overruns are observations, not hardware xruns.
#include "host_scenarios.h"
#include "RenderControls.h"
#if defined(SYNTH2_HOST_PROFILING)
#include "HostCallbackProfile.h"
#endif
#include <cstring>
#include <ctime>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <vector>

using Clock=std::chrono::steady_clock;
using namespace hostvalidation;
namespace {
std::string replaceText(std::string text,const std::string& from,const std::string& to){size_t pos=0;while((pos=text.find(from,pos))!=text.npos){text.replace(pos,from.size(),to);pos+=to.size();}return text;}

double cpuMicros()
{
   #if defined(__linux__)
    timespec value{};if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&value)!=0)throw std::runtime_error("Cannot read thread CPU clock");
    return value.tv_sec*1e6+value.tv_nsec*.001;
   #else
    return std::clock()*(1e6/CLOCKS_PER_SEC);
   #endif
}
template<typename F> double timed(F&& action)
{
    const auto start=Clock::now();action();return std::chrono::duration<double,std::micro>(Clock::now()-start).count();
}
int currentRound=0;
uint64_t audioHash=0;
void row(std::ostream& out,const char* profile,const char* operation,int rate,int block,int notes,int unison,int cores,
         std::vector<double> times,double deadline,float peak,std::vector<double> cpu={})
{
    std::sort(times.begin(),times.end());
    const auto percentile=[&](double fraction){return times[static_cast<size_t>(std::ceil(fraction*times.size()))-1];};
    const auto missed=deadline>0 ? std::count_if(times.begin(),times.end(),[&](double t){return t>deadline;}) : 0;
    out<<currentRound<<','<<profile<<','<<operation<<','<<rate<<','<<block<<','<<notes<<','<<unison<<','<<cores<<','<<times.size()<<','
       <<std::accumulate(times.begin(),times.end(),0.0)/times.size()<<','<<percentile(.5)<<','
       <<percentile(.95)<<','<<percentile(.99)<<','<<times.back()<<','<<deadline<<','<<missed<<','<<peak<<',';
    if(!cpu.empty())
    {
        std::sort(cpu.begin(),cpu.end());
        out<<std::accumulate(cpu.begin(),cpu.end(),0.0)/cpu.size()<<','<<cpu[static_cast<size_t>(std::ceil(.95*cpu.size()))-1];
    }
    else out<<',';
    out<<','<<audioHash<<'\n';
}
}
int main(int argc,char** argv) try
{
    if(argc<2)throw std::runtime_error("Usage: benchmark output.csv [blocks=256] [--quick] [--case rate block notes unison dry|chorus-delay] [--rounds N]");
    const auto integer=[](const std::string& text,int low,int high) {
        size_t used=0;const int value=std::stoi(text,&used);
        if(used!=text.size() || value<low || value>high)throw std::runtime_error("Invalid numeric argument: "+text);
        return value;
    };
    const int blocks=argc>=3 ? integer(argv[2],32,100000) : 256;
    bool quick=false,selected=false;int rounds=1;
    std::vector<int> rates{44100,48000,96000},sizes{32,64,256,1024},counts{1,8,16,32,48},unisons{1,8};
    std::vector<std::string> profiles{"dry","chorus-delay"};
    for(int i=3;i<argc;++i)
    {
        const std::string option=argv[i];
        if(option=="--quick")quick=true;
        else if(option=="--rounds" && i+1<argc)rounds=integer(argv[++i],1,20);
        else if(option=="--case" && i+5<argc && !selected)
        {
            selected=true;rates={integer(argv[++i],10000,384000)};sizes={integer(argv[++i],1,4096)};
            counts={integer(argv[++i],1,92)};unisons={integer(argv[++i],1,8)};profiles={argv[++i]};
            if(profiles[0]!="dry" && profiles[0]!="chorus-delay")throw std::runtime_error("Unknown profile");
        }
        else throw std::runtime_error("Unknown/incomplete option: "+option);
    }
    if(quick && selected)throw std::runtime_error("Choose either --quick or --case");
    if(quick){rates={48000};sizes={64,256};counts={1,8,32,48};}
    if(std::filesystem::exists(argv[1]))throw std::runtime_error("Output must not exist");
   #if defined(SYNTH2_HOST_PROFILING)
    const std::string samplePath=std::string(argv[1])+".samples.csv";
    if(std::filesystem::exists(samplePath))throw std::runtime_error("Sample output must not exist");
    std::ofstream samples(samplePath);if(!samples)throw std::runtime_error("Cannot open sample output");
    samples<<std::setprecision(10)<<"round,profile,rate,block,notes,unison,operation,index,wall_us,cpu_us,parameters_us,synthesis_us,effects_us\n";
   #endif
    std::ofstream out(argv[1]);if(!out)throw std::runtime_error("Cannot open output");out<<std::setprecision(10);
    out<<"round,profile,operation,sample_rate,block_size,requested_notes,unison,active_cores,iterations,mean_us,p50_us,p95_us,p99_us,max_us,deadline_us,over_budget,peak,steady_cpu_mean_us,steady_cpu_p95_us,audio_hash\n";
    for(currentRound=0;currentRound<rounds;++currentRound)
    for(const auto& profileName:profiles)
    for(int rate:rates)for(int block:sizes)for(int count:counts)for(int unison:unisons)
    {
        const char* profile=profileName.c_str();audioHash=0;
       #if defined(SYNTH2_HOST_PROFILING)
        const auto sample=[&](const char* operation,int index,double wall,double cpu) {
            const auto& r=synth2::hostprofile::current;
            samples<<currentRound<<','<<profile<<','<<rate<<','<<block<<','<<count<<','<<unison<<','<<operation<<','<<index<<','
                   <<wall<<','<<cpu<<','<<r.parameters<<','<<r.synthesis<<','<<r.effects<<'\n';
        };
        struct Sample {double wall,cpu;synth2::hostprofile::Record stages;};
        std::vector<Sample> captured;captured.reserve(static_cast<size_t>(blocks));
       #endif
        auto p=processor();auto text=patch(unison);
        if(std::string(profile)=="chorus-delay")
        {
            text=replaceText(text,"\n65,0\n","\n65,1\n");
            text=replaceText(text,"\n66,0\n","\n66,1\n");
            text=replaceText(text,"\n35,0\n","\n35,64\n");
            text=replaceText(text,"\n36,0\n","\n36,32\n");
            text=replaceText(text,"\n37,0\n","\n37,64\n");
            text=replaceText(text,"\n64,0\n","\n64,4\n");text+="52,64\n53,64\n54,64\n55,64\n56,64\n";
        }
        load(*p,text);p->prepare(rate,block);
        synth2::audio::AudioBuffer<float> audio(2,block);synth2::audio::MidiBuffer midi;notes(midi,count);
        const double deadline=block*1e6/rate;
        const double start=timed([&]{process(*p,audio,midi);});midi.clear();
        if(!finite(audio))throw std::runtime_error("Nonfinite note onset");
        synth2::RenderControls mapped;if(!synth2::readMappedSynthParams(*p,mapped) || mapped.unisonVoices!=unison
            || mapped.chorus.enabled!=(std::string(profile)=="chorus-delay") || mapped.delay.enable!=(std::string(profile)=="chorus-delay"))
            throw std::runtime_error("Unexpected mapped benchmark controls");
        const int cores=activeCores(*p);if(cores!=std::min(32,count*unison))throw std::runtime_error("Unexpected voice count");
        row(out,profile,"note_on_callback",rate,block,count,unison,cores,{start},deadline,audio.getMagnitude(0,block));
        // Warm up the actual sounding cores, caches and envelopes outside timing.
        for(int n=0;n<128;++n)process(*p,audio,midi);
        std::vector<double> times,cpu;times.reserve(static_cast<size_t>(blocks));cpu.reserve(static_cast<size_t>(blocks));float peak=0;
        audioHash=14695981039346656037uLL;
        for(int n=0;n<blocks;++n)
        {
            const double cpuStart=cpuMicros();
            times.push_back(timed([&]{process(*p,audio,midi);}));cpu.push_back(cpuMicros()-cpuStart);
           #if defined(SYNTH2_HOST_PROFILING)
            captured.push_back({times.back(),cpu.back(),synth2::hostprofile::current});
           #endif
            for(int ch=0;ch<2;++ch)for(int i=0;i<block;++i)
            {
                uint32_t word;const float value=audio.getSample(ch,i);std::memcpy(&word,&value,4);
                audioHash=(audioHash^word)*1099511628211uLL;
            }
            if(!finite(audio))throw std::runtime_error("Nonfinite steady output");
            const float blockPeak=audio.getMagnitude(0,block);
            if(blockPeak<1e-9f)throw std::runtime_error("Unexpected silent sustained callback");
            peak=std::max(peak,blockPeak);
        }
        if(peak<1e-6f)throw std::runtime_error("Silent steady output");
        row(out,profile,"steady_callback",rate,block,count,unison,cores,std::move(times),deadline,peak,std::move(cpu));
       #if defined(SYNTH2_HOST_PROFILING)
        for(int n=0;n<blocks;++n)
        {
            const auto& c=captured[static_cast<size_t>(n)];synth2::hostprofile::current=c.stages;sample("steady",n,c.wall,c.cpu);
        }
       #endif
        audioHash=0;
        std::vector<uint8_t> state;double saveTime=timed([&]{state=p->saveState();});
        row(out,profile,"save_state",rate,block,count,unison,cores,{saveTime},0,0);
        std::vector<double> loads,restores,callbacks;
        // Change an actual control so the callback must remap, rather than
        // measuring an identical-preset cache hit.
        const auto alternate=replaceText(text,"\n19,127\n","\n19,80\n");
        for(int n=0;n<8;++n)
        {
            loads.push_back(timed([&]{load(*p,alternate);}));
            callbacks.push_back(timed([&]{process(*p,audio,midi);}));
           #if defined(SYNTH2_HOST_PROFILING)
            sample("after_load",n,callbacks.back(),0);
           #endif
            if(!finite(audio))throw std::runtime_error("Nonfinite load output");
            restores.push_back(timed([&]{restore(*p,state);}));
            callbacks.push_back(timed([&]{process(*p,audio,midi);}));
           #if defined(SYNTH2_HOST_PROFILING)
            sample("after_restore",n,callbacks.back(),0);
           #endif
            if(!finite(audio))throw std::runtime_error("Nonfinite restored output");
        }
        row(out,profile,"load_patch",rate,block,count,unison,cores,std::move(loads),0,0);
        row(out,profile,"restore_state",rate,block,count,unison,cores,std::move(restores),0,0);
        row(out,profile,"callback_after_load_or_restore",rate,block,count,unison,activeCores(*p),std::move(callbacks),deadline,audio.getMagnitude(0,block));
        const auto prepare=timed([&]{p->release();p->prepare(rate,block);});
        row(out,profile,"reprepare",rate,block,count,unison,activeCores(*p),{prepare},0,0);
    }
    if(!out)throw std::runtime_error("Incomplete CSV");return 0;
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
