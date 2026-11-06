// Observe actual processor default materialization and maintained core conversion.
#include "host/ProcessorBridge.h"
#include "WaveFile.h"
#include "audio/Midi.h"
#include <patch/PatchLoader.h>
#include <SynthDiagnostics.h>
#include <RenderControls.h>
#include <Synth1PatchFamilyRawAdaptationInternal.h>
#include <Synth1PatchParser.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <map>
#include <memory>
#include <stdexcept>
#include "dsp_route_snapshot.h"
using synth2::host::ProcessorBridge;
static void process(ProcessorBridge& p,synth2::audio::AudioBuffer<float>& a,const synth2::audio::MidiBuffer& midi){for(const auto& e:midi){const auto* b=e.getMessage().getRawData();p.enqueueMidi(e.samplePosition,b[0],b[1],b[2]);}if(!p.process(a.getWritePointer(0),a.getWritePointer(1),a.getNumSamples(),120))throw std::runtime_error("Rejected render block");}
static std::string text(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary);if(!in)throw std::runtime_error("Missing source file");return {std::istreambuf_iterator<char>(in),{}};}
int main(int argc,char** argv) try
{
    if(argc!=4 && argc!=5)throw std::runtime_error("Expected bank root, cases and TSV");
    std::ifstream cases(argv[2]);std::ofstream out(argv[3]);std::string id;
    const bool dspRoutes=argc==5 && std::string(argv[4])=="--dsp-routes";
    const bool derived=argc==5 && std::string(argv[4])=="--derived";
    const bool routes=argc==5 && std::string(argv[4])=="--routes";
    const bool lfoRoutes=argc==5 && std::string(argv[4])=="--lfo-routes";
    if(!cases || !out)throw std::runtime_error("Cannot open audit files");
    while(cases>>id)
    {
        auto processor=std::make_unique<ProcessorBridge>();
        if(argc==5)processor->prepare(44100,64);
        auto update=[&](){if(argc==5){synth2::audio::AudioBuffer<float> audio(2,64);synth2::audio::MidiBuffer midi;process(*processor,audio,midi);}};
        if(dspRoutes)
        {
            synth2::audio::AudioBuffer<float> audio(2,64);synth2::audio::MidiBuffer none;
            const auto load=[&](const char* name){
                const auto file=std::filesystem::path(argv[1])/id/name;
                std::string error;
                if(!std::filesystem::is_regular_file(file) || !processor->loadPatch(text(file),error))throw std::runtime_error("Cannot load DSP-route source");
                audio.clear();process(*processor,audio,none);
            };
            load("001.sy1");load("002.sy1");
            std::map<std::string,double> previous;
            const auto snapshot=[&](int frame,bool force){
                synth2::MappedSynthSnapshot params;
                if(!synth2::readMappedSynthParams(*processor,params))throw std::runtime_error("Missing mapped processor state");
                const auto current=dspRouteSnapshot(params);
                if(force || current!=previous)
                {
                    for(const auto& [name,value]:current)out<<id<<'\t'<<frame<<'\t'<<name<<'\t'<<std::setprecision(17)<<value<<'\n';
                    std::array<int,99> controls;
                    if(!synth2::readNativePatchControls(*processor,controls))throw std::runtime_error("Non-native DSP-route source");
                    for(int i=0;i<99;++i)out<<id<<'\t'<<frame<<"\tcontrol_"<<i<<'\t'<<controls[i]<<'\n';
                    previous=current;
                }
            };
            snapshot(0,true);
            for(int frame=0;frame<154350;frame+=64)
            {
                const int count=std::min(64,154350-frame);audio.setSize(2,count,false,false,true);audio.clear();
                synth2::audio::MidiBuffer midi;
                if(frame==0)midi.addEvent(synth2::audio::MidiMessage::noteOn(1,60,static_cast<uint8_t>(100)),0);
                if(frame<=66150 && frame+count>66150)midi.addEvent(synth2::audio::MidiMessage::noteOff(1,60),66150-frame);
                process(*processor,audio,midi);
                snapshot(frame+count,frame==0 || (frame<=66150 && frame+count>66150) || frame+count==154350);
            }
            continue;
        }
        for(int donor:{0,2})
        {
            auto load=[&](int n){
                const auto file=std::filesystem::path(argv[1])/id/(n==0?"001.sy1":n==1?"002.sy1":"003.sy1");
                std::string error;
                if(!std::filesystem::is_regular_file(file))throw std::runtime_error("Missing source file");
                return processor->loadPatch(text(file),error);
            };
            if(!load(donor))throw std::runtime_error("Production rejected donor");update();
            for(int repeat=0;repeat<2;++repeat)
            {
                if(!load(1))
                {
                    for(int i=0;i<126;++i)out<<id<<'\t'<<donor<<'\t'<<repeat<<'\t'<<i<<"\t-9998\t-9998\n";
                    continue;
                }
                update();
                std::map<int,int> source;int version=0;
                for(int index=0;index<128;++index) {
                    const auto& p=processor->parameter(index);
                    const float value=static_cast<float>(p.minimum)+(static_cast<float>(p.maximum)-static_cast<float>(p.minimum))*static_cast<float>(processor->normalizedValue(index));
                    if(index<126)source[index]=std::lround(value);else if(index==126)version=std::lround(value);
                }
                if(source.size()!=126)throw std::runtime_error("Missing production parameter");
                std::vector<Synth1PatchEntry> entries;
                for(const auto& [index,value]:source)entries.push_back({index,value});
                auto document=Synth1PatchDocument::makeParsed("Audit",version,std::move(entries));std::map<int,int> normalized;
                for(const auto& e:buildRemappedSynth1PatchEntries(document))normalized[e.index]=e.rawValue;
                for(const auto& e:deriveFamilySpecificRawImplicitEntries(document))normalized[e.index]=e.rawValue;
                if(argc==5)
                {
                    std::array<int,99> resolved;
                    if(!synth2::readNativePatchControls(*processor,resolved))throw std::runtime_error("Production did not use native resolver");
                    normalized.clear();for(int index=0;index<99;++index)normalized[index]=resolved[index];
                }
                if(lfoRoutes)
                {
                    synth2::MappedSynthSnapshot params;
                    if(!synth2::readMappedSynthParams(*processor,params))throw std::runtime_error("Missing mapped processor state");
                    for(int slot=0;slot<2;++slot)
                    {
                        const auto& lfo=slot==0 ? params.lfo1 : params.lfo2;
                        out<<id<<'\t'<<donor<<'\t'<<repeat<<'\t'<<slot<<'\t'<<lfo.on<<'\t'<<lfo.dest
                           <<'\t'<<lfo.wave<<'\t'<<lfo.nativeSharedClock<<'\n';
                    }
                    continue;
                }
                if(routes)
                {
                    synth2::MappedSynthSnapshot params;
                    if(!synth2::readMappedSynthParams(*processor,params))throw std::runtime_error("Missing mapped processor state");
                    const auto emit=[&](const char* name,auto value){out<<id<<'\t'<<donor<<'\t'<<repeat<<'\t'<<name<<'\t'<<std::setprecision(17)<<value<<'\n';};
                    emit("arp",params.arpeggiator.enabled);emit("lfo1",params.lfo1.on);emit("lfo2",params.lfo2.on);
                    emit("filter_type",params.filterType);emit("filter_attack_seconds",params.filterEnv.attackSec);
                    emit("filter_decay_seconds",params.filterEnv.decaySec);emit("filter_sustain",params.filterEnv.sustain);
                    emit("filter_release_seconds",params.filterEnv.releaseSec);
                    emit("native_filter_envelope",params.nativeFilterEnvelope);emit("native_filter_lfo",params.nativeFilterLfo);
                    emit("native_filter_kernel",params.nativeOrdinaryStateVariable);
                    emit("native_pitch_lfo",params.nativePitchLfo);emit("native_pw_lfo",params.nativePulseWidthLfo);
                    emit("native_lfo1_clock",params.lfo1.nativeSharedClock);emit("native_lfo2_clock",params.lfo2.nativeSharedClock);
                    continue;
                }
                if(derived)
                {
                    synth2::MappedSynthSnapshot params;
                    if(!synth2::readMappedSynthParams(*processor,params))throw std::runtime_error("Missing mapped processor state");
                    out<<id<<'\t'<<donor<<'\t'<<repeat<<'\t'<<source.at(64)<<'\t'<<normalized.at(64)
                        <<'\t'<<params.chorus.type<<'\t'<<params.nativePitchLfo<<'\t'<<params.modEnv.nativePitch
                        <<'\n';
                    continue;
                }
                for(const auto& [index,value]:source)
                    out<<id<<'\t'<<donor<<'\t'<<repeat<<'\t'<<index<<'\t'<<value<<'\t'<<(normalized.count(index)?normalized.at(index):-9999)<<'\n';
            }
        }
    }
    if(!cases.eof() || !out)throw std::runtime_error("Incomplete candidate audit");return 0;
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
