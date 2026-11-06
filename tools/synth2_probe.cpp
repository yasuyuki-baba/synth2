// SDK-independent offline host. Keep loading and callback partitions explicit.
#include "host/ProcessorBridge.h"
#include "SynthDiagnostics.h"
#include "WaveFile.h"
#include "audio/Midi.h"
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
using synth2::host::ProcessorBridge;
using synth2::audio::AudioBuffer;
using synth2::audio::MidiBuffer;
using synth2::audio::MidiMessage;
static void log(const std::string& s){std::cout<<s<<'\n';}
static std::string text(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot read patch: "+path.string());return {std::istreambuf_iterator<char>(in),{}};}
static std::string trim(std::string s){const auto a=s.find_first_not_of(" \t\r\n");return a==s.npos?"":s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);}
static std::vector<std::string> split(const std::string& s,char delimiter){std::vector<std::string> out;std::istringstream in(s);std::string v;while(std::getline(in,v,delimiter))out.push_back(trim(v));return out;}
static std::string key(std::string s){s=trim(s);for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));while(!s.empty() && s[0]=='*')s=trim(s.substr(1));if(s.starts_with("osc "))s=trim(s.substr(4));auto i=s.find("p/w");if(i!=s.npos)s.replace(i,3,"pulse width");while((i=s.find("  "))!=s.npos)s.replace(i,2," ");return s;}
static void set(ProcessorBridge& p,int i,float v){if(i<0 || i>=128)throw std::runtime_error("Invalid source parameter");p.setNormalizedValue(i,std::clamp(v,0.f,1.f));}
static void overrides(ProcessorBridge& p){
    if(const auto* env=std::getenv("SYNTH2_SET_PARAMS_NORM"))for(const auto& pair:split(env,';')) {
        const auto eq=pair.find('=');if(eq==pair.npos)continue;const auto name=key(pair.substr(0,eq));int index=-1;
        if(name.size()>1 && name[0]=='p' && name.find_first_not_of("0123456789",1)==name.npos)index=std::stoi(name.substr(1));
        else for(int i=0;i<128;++i)if(key(p.parameter(i).name)==name){index=i;break;}
        if(index>=0 && index<128)set(p,index,std::stof(pair.substr(eq+1)));else log("[Synth2Probe] WARNING: failed to override by name: "+name);
    }
}
static void load(ProcessorBridge& p,const std::filesystem::path& path){std::string error;if(!p.loadPatch(text(path),error))throw std::runtime_error(error);}
static void process(ProcessorBridge& p,AudioBuffer<float>& audio,const MidiBuffer& midi,double tempo){for(const auto& e:midi){const auto* bytes=e.getMessage().getRawData();if(!p.enqueueMidi(e.samplePosition,bytes[0],bytes[1],bytes[2]))throw std::runtime_error("MIDI queue overflow");}if(!p.process(audio.getWritePointer(0),audio.getWritePointer(1),audio.getNumSamples(),tempo))throw std::runtime_error("Rejected render block");}
int main(int argc,char** argv) try {
    double sampleRate=44100.;if(auto* e=std::getenv("SYNTH2_PROBE_SAMPLE_RATE")){double v=std::atof(e);if(std::isfinite(v) && v>=8000 && v<=384000)sampleRate=v;}
    int blockSize=512,velocity=127;if(auto* e=std::getenv("SYNTH2_PROBE_BLOCK_SIZE"))blockSize=std::atoi(e);if(auto* e=std::getenv("SYNTH2_PROBE_VELOCITY"))velocity=std::atoi(e);
    if(blockSize<1 || blockSize>8192 || velocity<1 || velocity>127)throw std::runtime_error("Invalid block size or velocity");
    std::filesystem::path outFile="docs/synth1_probe/synth2_a4_2s.wav",patchFile,initialPreset,lfoActionsFile,midiTimeline;
    int oversamplingFactor=1;
    int midiNote=69,presetWarmupBlocks=1,primeNoteCount=1,primeUnisonVoices=0,positional=0;
    double durationSec=2.,tempo=120.,primeNoteSeconds=0.,primeReleaseSeconds=0.;std::string noteList;
    bool floatWav=false,transactionalPresetLoad=false,warmupSpecified=false,bypass=false;
    std::vector<std::pair<int,float>> diagnosticOverrides;
    for(int i=1;i<argc;++i) {
        const std::string arg=argv[i];auto next=[&](){if(i+1>=argc)throw std::runtime_error("Missing argument for "+arg);return std::string(argv[++i]);};
        if(arg=="--transactional-preset-load")transactionalPresetLoad=true;
        else if(arg=="--float-wav")floatWav=true;
        else if(arg=="--oversampling")oversamplingFactor=std::stoi(next());
        else if(arg=="--bypass-env")bypass=true;
        else if(arg=="-o")outFile=next();else if(arg=="-n")midiNote=std::stoi(next());else if(arg=="-d")durationSec=std::stof(next());else if(arg=="-p")patchFile=next();
        else if(arg=="--initial-preset")initialPreset=next();else if(arg=="--preset-warmup-blocks"){presetWarmupBlocks=std::stoi(next());warmupSpecified=true;}
        else if(arg=="--notes")noteList=next();else if(arg=="--tempo")tempo=std::stod(next());
        else if(arg=="--prime-note-seconds")primeNoteSeconds=std::stod(next());else if(arg=="--prime-release-seconds")primeReleaseSeconds=std::stod(next());
        else if(arg=="--prime-note-count")primeNoteCount=std::stoi(next());else if(arg=="--prime-unison-voices")primeUnisonVoices=std::stoi(next());
        else if(arg=="--lfo-actions")lfoActionsFile=next();else if(arg=="--midi-timeline")midiTimeline=next();
        else if(arg=="--osc1-shape")diagnosticOverrides.push_back({0,std::stof(next())/127.f});else if(arg=="--osc1-mix")diagnosticOverrides.push_back({5,std::stof(next())/127.f});
        else if(!arg.starts_with('-') || (arg.size()>1 && std::isdigit(static_cast<unsigned char>(arg[1])))) {
            if(positional==0)outFile=arg;else if(positional==1)midiNote=std::stoi(arg);else if(positional==2)durationSec=std::stof(arg);else if(positional==3)patchFile=arg;else throw std::runtime_error("Too many positional arguments");++positional;
        } else throw std::runtime_error("Unknown option: "+arg);
    }
    if(!std::isfinite(durationSec) || durationSec<=0 || durationSec>60 || midiNote<0 || midiNote>127 || primeNoteCount<0 || primeNoteCount>128
       || primeUnisonVoices<0 || primeUnisonVoices==1 || primeUnisonVoices>8 || !std::isfinite(primeNoteSeconds) || primeNoteSeconds<0 || primeNoteSeconds>60
       || !std::isfinite(primeReleaseSeconds) || primeReleaseSeconds<0 || primeReleaseSeconds>60 || !std::isfinite(tempo) || tempo<=0 || tempo>1000)throw std::runtime_error("Invalid note, duration or tempo");
    if((!initialPreset.empty() && (!std::filesystem::is_regular_file(initialPreset) || !std::filesystem::is_regular_file(patchFile))) || (warmupSpecified && initialPreset.empty()) || presetWarmupBlocks<1 || presetWarmupBlocks>1024)throw std::runtime_error("Preset warmup requires initial and target patches and 1..1024 blocks");
    const int totalSamples=static_cast<int>(std::round(durationSec*sampleRate)),numChannels=2;
    std::vector<int> notes;for(const auto& n:split(noteList,',')){if(n.empty() || n.find_first_not_of("0123456789")!=n.npos || std::stoi(n)>127)throw std::runtime_error("Invalid chord note list");notes.push_back(std::stoi(n));}if(notes.empty())notes.push_back(midiNote);
    auto processor=std::make_unique<ProcessorBridge>();
    if(!processor->setOversamplingFactor(oversamplingFactor))throw std::runtime_error("Oversampling must be 1, 2, 4, 8 or 16");
    auto warmup=[&](){AudioBuffer<float> audio(2,blockSize);for(int i=0;i<presetWarmupBlocks;++i){audio.clear();MidiBuffer none;process(*processor,audio,none,tempo);}};
    if(!initialPreset.empty()){load(*processor,initialPreset);processor->prepare(sampleRate,blockSize);warmup();}
    if(!patchFile.empty()) {if(!transactionalPresetLoad)for(int i=0;i<128;++i)processor->setNormalizedValue(i,processor->parameter(i).defaultNormalized);load(*processor,patchFile);}
    if(auto* path=std::getenv("SYNTH_OSC1_PATCH_FILE"))load(*processor,path);
    overrides(*processor);for(const auto& [i,v]:diagnosticOverrides)set(*processor,i,v);
    if(bypass){for(const auto& [i,v]:std::map<int,float>{{19,1},{27,1},{29,1},{35,1},{4,0},{13,0},{14,.5},{15,.5}})set(*processor,i,v);
#ifdef _WIN32
        _putenv_s("SYNTH2_BYPASS_ENV","1");
#else
        setenv("SYNTH2_BYPASS_ENV","1",1);
#endif
    }
    if(std::getenv("SYNTH1_FORCE_NOISE")){set(*processor,2,4.f/127.f);set(*processor,15,1);set(*processor,95,0);}
    if(auto* e=std::getenv("SYNTH1_EQ_RAW_OVR")){auto v=split(e,',');if(v.size()==4){set(*processor,50,std::stof(v[0]));set(*processor,51,std::stof(v[1]));set(*processor,52,std::stof(v[2]));set(*processor,80,std::stof(v[3]));}}
    // The preceding probe parsed SYNTH2_AUTOMATE_PARAMS but never dispatched it.
    // Fail visibly rather than silently claiming a scheduled automation capture.
    if(std::getenv("SYNTH2_AUTOMATE_PARAMS"))throw std::runtime_error("SYNTH2_AUTOMATE_PARAMS was inactive in the baseline; use an explicit action timeline");
    if(initialPreset.empty())processor->prepare(sampleRate,blockSize);else warmup();
    struct LfoAction{int frame,kind,a,b;};std::vector<LfoAction> lfoActions;
    if(!lfoActionsFile.empty()) {
        std::ifstream in(lfoActionsFile);if(!in)throw std::runtime_error("Cannot open LFO actions");LfoAction a;int previous=-1;
        while(in>>a.frame>>a.kind>>a.a>>a.b) {
            if(a.frame<previous || a.frame<0 || a.frame>=totalSamples || a.frame%blockSize || a.kind<0 || a.kind>3
               || (a.kind==0 && ((a.a!=43 && a.a!=48 && a.a!=67 && a.a!=69) || a.b<0 || a.b>((a.a==67 || a.a==69)?1:127)))
               || ((a.kind==1 || a.kind==3) && (a.a<0 || a.a>2)) || (a.kind==2 && (a.a<10000 || a.a>192000 || a.b<1 || a.b>512)))throw std::runtime_error("Invalid LFO action");
            previous=a.frame;lfoActions.push_back(a);
        }if(!in.eof())throw std::runtime_error("Malformed LFO actions");
    }
    int noteOnSample=0;if(auto* e=std::getenv("SYNTH2_NOTE_ON_SAMPLES"))noteOnSample=std::atoi(e);else if(auto* e=std::getenv("SYNTH2_NOTE_ON_SEC"))noteOnSample=std::round(std::atof(e)*sampleRate);
    int noteOffSample=std::round(sampleRate*(durationSec*.99));if(auto* e=std::getenv("SYNTH2_NOTE_OFF_SEC"))noteOffSample=std::round(std::atof(e)*sampleRate);
    MidiBuffer midi;for(int n:notes){midi.addEvent(MidiMessage::noteOn(1,n,static_cast<uint8_t>(velocity)),noteOnSample);if(noteOffSample>=0)midi.addEvent(MidiMessage::noteOff(1,n),noteOffSample);}
    if(!midiTimeline.empty()) {
        std::ifstream in(midiTimeline);if(!in)throw std::runtime_error("Cannot open MIDI timeline");midi.clear();int frame,status,note,eventVelocity,previous=-1;
        while(in>>frame>>status>>note>>eventVelocity){if(frame<previous || frame<0 || frame>=totalSamples || (status<128 || status>239 || ((status&240)!=128 && (status&240)!=144 && (status&240)!=176 && (status&240)!=224)) || ((status&240)==176 && note!=1 && note!=64 && note!=120 && note!=121 && note!=123) || note<0 || note>127 || eventVelocity<0 || eventVelocity>127 || midi.getNumEvents()==MidiBuffer::capacity)throw std::runtime_error("Invalid MIDI timeline");previous=frame;midi.addEvent(MidiMessage(status,note,eventVelocity),frame);}
        if(!in.eof() || midi.isEmpty())throw std::runtime_error("Malformed MIDI timeline");
    }
    const float targetUnison=processor->normalizedValue(93);if(primeUnisonVoices>0)set(*processor,93,primeUnisonVoices/127.f);
    const int primeOff=std::round(primeNoteSeconds*sampleRate),primeTotal=primeOff+static_cast<int>(std::round(primeReleaseSeconds*sampleRate));
    for(int i=0;i<primeNoteCount;++i)for(int start=0;start<primeTotal;start+=blockSize){const int n=std::min(blockSize,primeTotal-start);AudioBuffer<float> a(2,n);MidiBuffer events;for(int note:notes){if(start==0)events.addEvent(MidiMessage::noteOn(1,note,static_cast<uint8_t>(velocity)),0);if(primeOff>=start && primeOff<start+n)events.addEvent(MidiMessage::noteOff(1,note),primeOff-start);}process(*processor,a,events,tempo);}
    if(primeUnisonVoices>0)set(*processor,93,targetUnison);
    for(int i=0;i<128;++i)std::printf("[Synth2Probe_PARAMS] p%d = %.4f\n",i,processor->normalizedValue(i));
    AudioBuffer<float> renderBuffer(numChannels,totalSamples);
    std::ofstream envelopeTrace;
    if(const auto* path=std::getenv("SYNTH2_PROBE_ENVELOPE_TRACE"))
    {
        envelopeTrace.open(path);
        if(!envelopeTrace)throw std::runtime_error("Cannot open physical envelope trace");
    }
    std::ofstream renderingTrace;
    if(const auto* path=std::getenv("SYNTH2_PROBE_RENDERING_TRACE"))
    {
        renderingTrace.open(path);
        if(!renderingTrace)throw std::runtime_error("Cannot open logical rendering trace");
    }
    std::ofstream renderingFloatTrace;
    if(const auto* path=std::getenv("SYNTH2_PROBE_RENDERING_FLOAT_TRACE"))
    {
        renderingFloatTrace.open(path);
        if(!renderingFloatTrace)throw std::runtime_error("Cannot open logical rendering float trace");
    }
    std::ofstream voiceCoreTrace;
    if(const auto* path=std::getenv("SYNTH2_PROBE_VOICE_CORE_TRACE"))
    {
        voiceCoreTrace.open(path);
        if(!voiceCoreTrace)throw std::runtime_error("Cannot open active voice-core trace");
    }
    std::ofstream fmTrace;
    if(const auto* path=std::getenv("SYNTH2_PROBE_FM_TRACE")){fmTrace.open(path);if(!fmTrace)throw std::runtime_error("Cannot open active FM trace");}
    std::ofstream stackTrace;
    if(const auto* path=std::getenv("SYNTH2_PROBE_STACK_TRACE"))
    {
        stackTrace.open(path);
        if(!stackTrace)throw std::runtime_error("Cannot open active stack trace");
    }
    std::ofstream coreTrace;
    std::ofstream multiEffectTrace;
    if(const auto* path=std::getenv("SYNTH2_PROBE_MULTIFX_TRACE"))
    {
        multiEffectTrace.open(path);
        if(!multiEffectTrace)throw std::runtime_error("Cannot open MultiFX trace");
    }
    if(const auto* path=std::getenv("SYNTH2_PROBE_CORE_TRACE"))
    {
        coreTrace.open(path);
        if(!coreTrace)throw std::runtime_error("Cannot open physical core trace");
    }
    std::ofstream randomLfoTrace;
    if(const auto* path=std::getenv("SYNTH2_PROBE_RANDOM_LFO_TRACE")){randomLfoTrace.open(path);if(!randomLfoTrace)throw std::runtime_error("Cannot open random LFO trace");}
    std::ofstream lfoTrace;
    if(const auto* path=std::getenv("SYNTH2_PROBE_LFO_TRACE"))
    {
        lfoTrace.open(path);
        if(!lfoTrace)throw std::runtime_error("Cannot open native LFO trace");
    }
    std::ofstream poolTrace;
    if(const auto* path=std::getenv("SYNTH2_PROBE_POOL_TRACE"))
    {
        poolTrace.open(path);
        if(!poolTrace)throw std::runtime_error("Cannot open physical pool trace");
    }
    int rendered = 0;
    std::ofstream floatTrace;
    if(const auto* path=std::getenv("SYNTH2_PROBE_FLOAT_TRACE"))
    {
        floatTrace.open(path);
        if(!floatTrace)throw std::runtime_error("Cannot open physical float trace");
    }

    std::ofstream controllerTrace;
    if(const auto* path=std::getenv("SYNTH2_PROBE_CONTROLLER_TRACE"))
    {
        controllerTrace.open(path);
        if(!controllerTrace)throw std::runtime_error("Cannot open controller trace");
    }

    int currentActionProgram=1;
    auto actionLoad=[&](int program){char name[16];std::snprintf(name,sizeof(name),"%03d.sy1",program+1);load(*processor,patchFile.parent_path()/name);};
    while(rendered<totalSamples) {
        int pendingMidiProgram=-1;
        for(const auto& action:lfoActions)if(action.frame==rendered) {
            if(action.kind==0){const auto& p=processor->parameter(action.a);set(*processor,action.a,(static_cast<float>(action.b)-static_cast<float>(p.minimum))/(static_cast<float>(p.maximum)-static_cast<float>(p.minimum)));}
            else if(action.kind==3)pendingMidiProgram=action.a;else if(action.kind==1)actionLoad(action.a);else {processor->release();processor->prepare(action.a,action.b);}
        }
        if(pendingMidiProgram>=0 && pendingMidiProgram!=currentActionProgram){actionLoad(pendingMidiProgram);currentActionProgram=pendingMidiProgram;}
        const int samplesThisBlock=std::min(blockSize,totalSamples-rendered);AudioBuffer<float> blockBuffer(2,samplesThisBlock);MidiBuffer blockMidi;
        for(const auto& e:midi)if(e.samplePosition>=rendered && e.samplePosition<rendered+samplesThisBlock)blockMidi.addEvent(e.getMessage(),e.samplePosition-rendered);
        process(*processor,blockBuffer,blockMidi,tempo);
        if(std::getenv("SYNTH2_PROBE_PROGRAM_RECEIVERS"))
        {
            for(size_t voice=0;voice<8;++voice)
            {
                std::array<uint32_t,5> coarse{};
                if(!synth2::readPhysicalCoarseWords(*processor,voice,coarse))throw std::runtime_error("Cannot inspect coarse receiver");
                std::cerr << "PROGRAM_COARSE " << rendered+samplesThisBlock << ' ' << voice;
                for(auto word:coarse)std::cerr << ' ' << word;
                std::cerr << '\n';
                std::array<uint32_t,7> fm{};
                if(synth2::readPhysicalFmWords(*processor,voice,fm))
                {
                    std::cerr << "PROGRAM_FM " << rendered+samplesThisBlock << ' ' << voice;
                    for(auto word:fm)std::cerr << ' ' << word;
                    std::cerr << '\n';
                }
            }
            if(rendered==8192 || rendered==20480)
            {
                std::array<int,99> controls{};
                if(!synth2::readNativePatchControls(*processor,controls))throw std::runtime_error("Cannot inspect running load");
                for(size_t index=0;index<controls.size();++index)
                    std::cerr << "PROGRAM_LOAD " << rendered+samplesThisBlock << ' ' << index << ' ' << controls[index] << '\n';
            }
        }
        if(std::getenv("SYNTH2_PROBE_PERFORMANCE_SNAPSHOT") && rendered>=8064 && rendered<=8448)
        {
            synth2::RenderControls values;synth2::readMappedSynthParams(*processor,values);
            std::cerr << "PERFORMANCE_POLICY " << rendered << ' ' << values.nativeFilterEnvelope << ' ' << values.nativeVoicePerformance << '\n';
            for(size_t voice=0;voice<32;++voice)
            {
                std::array<uint32_t,16> words{};std::array<uint32_t,22> floats{};
                if(!synth2::readRenderingCoreWords(*processor,voice,0,words) || words[0]==0)continue;
                if(!synth2::readRenderingCoreFloatWords(*processor,voice,0,floats))continue;
                std::cerr << "PERFORMANCE_CORE " << rendered << ' ' << voice;
                for(auto word:words)std::cerr << ' ' << word;
                for(auto word:floats)std::cerr << ' ' << word;
                std::cerr << '\n';
            }
        }
        if(controllerTrace.is_open())
        {
            synth2::RenderControls values;
            if(!synth2::readMappedSynthParams(*processor,values))throw std::runtime_error("Cannot inspect controller values");
            std::array<int,4> pitch;
            if(!synth2::readNativePitchBendWords(*processor,pitch))throw std::runtime_error("Cannot inspect pitch bend");
            controllerTrace<<rendered+samplesThisBlock;
            for(auto word:pitch)controllerTrace<<'\t'<<word;
            controllerTrace<<'\t'<<values.nativeVoicePerformance<<'\t'
                <<values.cutoffIndex<<'\t'<<values.osc1FmAmount<<'\t'
                <<values.ampGainMultiplier<<'\t'<<values.lfo1.amount<<'\t'<<values.lfo2.amount<<'\n';
        }

        if(randomLfoTrace.is_open())
        {
            randomLfoTrace << rendered+samplesThisBlock;
            for(size_t slot=0;slot<2;++slot){std::array<uint32_t,9> words{};if(!synth2::readNativeRandomLfoWords(*processor,slot,words))throw std::runtime_error("Cannot inspect random LFO");for(auto word:words)randomLfoTrace << '\t' << word;}
            randomLfoTrace << '\n';if(!randomLfoTrace)throw std::runtime_error("Incomplete random LFO trace");
        }
        if(lfoTrace.is_open())
        {
            lfoTrace << rendered+samplesThisBlock;
            for(size_t lfo=0;lfo<2;++lfo)
            {
                std::array<uint32_t,7> words{};
                if(!synth2::readNativeLfoClockWords(*processor,lfo,words))throw std::runtime_error("Cannot inspect native LFO");
                for(auto word:words)lfoTrace << '\t' << word;
            }
            lfoTrace << '\n';
            if(!lfoTrace)throw std::runtime_error("Incomplete native LFO trace");
        }
        if(renderingTrace.is_open())
        {
            for(size_t voice=0;voice<32;++voice)
            {
                std::array<uint32_t,16> words{};
                if(!synth2::readRenderingCoreWords(*processor,voice,0,words))continue;
                renderingTrace<<rendered+samplesThisBlock<<'\t'<<voice;
                for(auto word:words)renderingTrace<<'\t'<<word;
                renderingTrace<<'\n';
            }
            if(!renderingTrace)throw std::runtime_error("Incomplete logical rendering trace");
        }
        if(renderingFloatTrace.is_open())
        {
            for(size_t voice=0;voice<32;++voice)
            {
                std::array<uint32_t,22> words{};
                if(!synth2::readRenderingCoreFloatWords(*processor,voice,0,words))continue;
                renderingFloatTrace<<rendered+samplesThisBlock<<'\t'<<voice;
                for(auto word:words)renderingFloatTrace<<'\t'<<word;
                renderingFloatTrace<<'\n';
            }
            if(!renderingFloatTrace)throw std::runtime_error("Incomplete logical rendering float trace");
        }
        if(voiceCoreTrace.is_open())
        {
            for(size_t voice=0;voice<32;++voice)for(size_t core=0;core<8;++core)
            {
                std::array<uint32_t,16> state{};std::array<uint32_t,22> floats{};
                if(!synth2::readRenderingCoreWords(*processor,voice,core,state) || state[0]==0)continue;
                if(!synth2::readRenderingCoreFloatWords(*processor,voice,core,floats))throw std::runtime_error("Missing active core float state");
                voiceCoreTrace<<rendered+samplesThisBlock<<'\t'<<voice<<'\t'<<core;
                for(auto word:state)voiceCoreTrace<<'\t'<<word;
                for(auto word:floats)voiceCoreTrace<<'\t'<<word;
                voiceCoreTrace<<'\n';
            }
            voiceCoreTrace<<"END\t"<<rendered+samplesThisBlock<<'\n';
            if(!voiceCoreTrace)throw std::runtime_error("Incomplete active voice-core trace");
        }
        if(fmTrace.is_open())
        {
            for(size_t voice=0;voice<32;++voice)for(size_t core=0;core<8;++core)
            {
                std::array<uint32_t,16> state{};std::array<uint32_t,7> fm{};
                if(!synth2::readRenderingCoreWords(*processor,voice,core,state) || state[0]==0)continue;
                if(!synth2::readRenderingFmWords(*processor,voice,core,fm))continue;
                fmTrace<<rendered+samplesThisBlock<<'\t'<<voice<<'\t'<<core<<'\t'<<state[6]<<'\t'<<state[15];
                for(auto word:fm)fmTrace<<'\t'<<word;
                fmTrace<<'\n';
            }
            fmTrace<<"END\t"<<rendered+samplesThisBlock<<'\n';
            if(!fmTrace)throw std::runtime_error("Incomplete active FM trace");
        }
        if(stackTrace.is_open())
        {
            for(size_t voice=0;voice<32;++voice)for(size_t core=0;core<8;++core)
            {
                std::array<uint32_t,16> state{};std::array<uint32_t,17> stack{};
                if(!synth2::readRenderingCoreWords(*processor,voice,core,state) || state[0]==0)continue;
                if(!synth2::readRenderingStackWords(*processor,voice,core,stack))throw std::runtime_error("Missing active stack state");
                stackTrace<<rendered+samplesThisBlock<<'\t'<<voice<<'\t'<<core<<'\t'<<state[6]<<'\t'<<state[15];
                for(auto word:stack)stackTrace<<'\t'<<word;
                stackTrace<<'\n';
            }
            stackTrace<<"END\t"<<rendered+samplesThisBlock<<'\n';
            if(!stackTrace)throw std::runtime_error("Incomplete active stack trace");
        }
        if(multiEffectTrace.is_open())
        {
            std::array<uint32_t,7> words{};
            if(!synth2::readNativeMultiEffectWords(*processor,words))throw std::runtime_error("Cannot inspect MultiFX");
            multiEffectTrace<<rendered+samplesThisBlock;
            for(auto word:words)multiEffectTrace<<'\t'<<word;
            multiEffectTrace<<'\n';
            if(!multiEffectTrace)throw std::runtime_error("Incomplete MultiFX trace");
        }
        if(coreTrace.is_open())
        {
            std::array<uint32_t,9> words {};
            if(!synth2::readPhysicalCoreWords(*processor,0,words))throw std::runtime_error("Cannot inspect physical core");
            coreTrace << rendered+samplesThisBlock;
            for(auto word:words)coreTrace << '\t' << word;
            coreTrace << '\n';
            if(!coreTrace)throw std::runtime_error("Incomplete physical core trace");
        }
        if(poolTrace.is_open())
        {
            for(size_t slot=0;slot<32;++slot)
            {
                std::array<uint32_t,9> core{};
                std::array<int,13> envelopes{};
                if(!synth2::readPhysicalCoreWords(*processor,slot,core)
                   || !synth2::readPhysicalCoreEnvelopeWords(*processor,slot,envelopes))
                    throw std::runtime_error("Cannot inspect physical pool");
                poolTrace << rendered+samplesThisBlock << '\t' << slot;
                for(auto word:envelopes)poolTrace << '\t' << word;
                for(auto word:core)poolTrace << '\t' << word;
                poolTrace << '\n';
            }
            if(!poolTrace)throw std::runtime_error("Incomplete physical pool trace");
        }
        if(floatTrace.is_open())
        {
            for(size_t slot=0;slot<32;++slot)
            {
                std::array<uint32_t,22> words{};
                if(!synth2::readPhysicalCoreFloatWords(*processor,slot,words))throw std::runtime_error("Cannot inspect physical float state");
                floatTrace << rendered+samplesThisBlock << '\t' << slot;
                for(auto word:words)floatTrace << '\t' << word;
                floatTrace << '\n';
            }
            if(!floatTrace)throw std::runtime_error("Incomplete physical float trace");
        }
        if(envelopeTrace.is_open())
        {
            std::array<int,13> words{};
            if(!synth2::readPhysicalCoreEnvelopeWords(*processor,0,words))throw std::runtime_error("Cannot inspect physical envelopes");
            envelopeTrace << rendered+samplesThisBlock;
            for(auto word:words)envelopeTrace << '\t' << word;
            envelopeTrace << '\n';
            if(!envelopeTrace)throw std::runtime_error("Incomplete physical envelope trace");
        }


        for(int ch=0;ch<2;++ch)renderBuffer.copyFrom(ch,rendered,blockBuffer.getReadPointer(ch),samplesThisBlock);
        rendered+=samplesThisBlock;
    }
    float peak=0;for(int ch=0;ch<2;++ch)for(int i=0;i<totalSamples;++i){const auto v=renderBuffer.getSample(ch,i);if(!std::isfinite(v))throw std::runtime_error("Non-finite audio");peak=std::max(peak,std::abs(v));}
    std::cout<<"[Synth2Probe] peak |x| = "<<std::setprecision(9)<<peak<<'\n';
    synth2::offline::writeWave(outFile,renderBuffer,sampleRate,floatWav);processor->release();return 0;
} catch(const std::exception& e){std::cerr<<"[Synth2Probe] ERROR: "<<e.what()<<'\n';return 1;}
