#include "ProcessorBridge.h"
#include "PatchLibrary.h"
#include "ProcessorState.h"
#include "SynthEngine.h"
#include "patch/PatchLoader.h"
#include "patch/PatchFormat.h"
#include "patch/Synth1PatchMapper.h"
#include "patch/Synth1SourceRange.h"
#include "render/RandomArpAdmission.h"
#include "CoreResources.h"
#include "dsp/LfoHistory.h"
#include "render/FilterParameterMapping.h"
#include <Synth1NativeLoading.h>
#include <Synth1PatchNativeControls.h>
#include <atomic>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
#if defined(__SSE__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

namespace synth2::host
{
namespace
{
struct NoDenormals
{
#if defined(__SSE__) || defined(_M_X64)
    unsigned previous=_mm_getcsr();
    NoDenormals(){_mm_setcsr(previous|0x8040);}
    ~NoDenormals(){_mm_setcsr(previous);}
#elif defined(__aarch64__)
    uint64_t previous{};
    NoDenormals(){asm volatile("mrs %0, fpcr":"=r"(previous));const auto next=previous|(uint64_t{1}<<24);asm volatile("msr fpcr, %0"::"r"(next));}
    ~NoDenormals(){asm volatile("msr fpcr, %0"::"r"(previous));}
#endif
};
float defaultSource(int i)
{
    float value=0;
    if(i==4)value=1;
    if(i==52)value=64;
    if(i==83)value=66;
    if(i==85)value=24;
    if(i==75)value=22;
    if(i==93)value=2;
    if(i==94)value=16;
    if(i==1)value=1;
    if(i==2 || i==3 || i==8 || i==11 || i==21 || (i>=60 && i<=63) || i==72 || i==84 || i==90 || i==98)value=64;
    if(i==19 || i==17 || i==27)value=127;
    if(i==29)value=100;
    if(i==31)value=2;
    if(i==33)value=11;
    if(i==34)value=64;
    if(i==126)value=105;
    return value;
}

}
struct ProcessorBridge::Impl
{
    SynthEngine engine;
    std::atomic<int> requestedOversampling{1};
    int fadeOut=0,fadeIn=0;float fadeFrom=1,fadeGain=1;
    std::array<std::atomic<float>,parameterCount> values{};
    std::array<ParameterInfo,parameterCount> info{};
    std::array<std::atomic<float>*,126> rawSourceValues{};
    std::atomic<float>* rawPatchVersion=&values[126];
    std::atomic<float>* rawMasterVolume=&values[128];
    std::unique_ptr<Synth1PatchMapper> parameterMapper=std::make_unique<Synth1PatchMapper>();
    bool continuousControls=true;
    bool directControls=false;
    int parameterMappingVersion=-1;
    std::atomic<bool> replacingParameters{false},phaseResetRequested{false};
    std::atomic<unsigned> missingEnvelopeFields{0};
    std::atomic<bool> nativeSourceLoading{false},nativeRandomArpSeen{false};
    std::array<std::atomic<uint64_t>,2> nativeSourcePresence{},nativeRuntimeEdits{};
    std::array<std::atomic<int>,99> nativeInitialControls{};
    std::atomic<int> nativeSourceVersion{0};
    std::atomic<uint64_t> lastNativeParameterPair{~uint64_t{0}};
    std::atomic<uint64_t> lastReceiverParameterPair{~uint64_t{0}};
    struct FilterEdit {int index{};float before{},after{};uint64_t generation{};};
    dsp::HistoryQueue<FilterEdit> filterEdits;
    std::array<FilterEdit,decltype(filterEdits)::capacity> pendingFilterEdits{};
    std::atomic<uint64_t> filterGeneration{0};
    uint64_t appliedFilterDrops=0;
    dsp::LfoHistory<> lfoHistory;
    std::string lastLoadedPreset;
    std::string patchName = "Init";
    int program=0;
    struct Event{int offset;uint8_t status,data1,data2;};
    std::array<Event,midiCapacity> events{};
    size_t eventCount=0;uint64_t drops=0;bool overflow=false;int capacity=0;
    audio::AudioBuffer<float> scratch;
    audio::MidiBuffer midi,incoming;
    EditorMidiInput editorMidi;
    Impl()
    {
        parameterMapper->setContinuousControls(continuousControls);
        for(int i=0;i<parameterCount;++i)
        {
            const float minimum=i==128 ? 0.f : static_cast<float>(sourceMinimum(i));
            const float maximum=i==128 ? 1.f : static_cast<float>(sourceMaximum(i));
            const float initial=i==128 ? 1.f : defaultSource(i);
            info[i]={i==128 ? "masterVolume" : "p"+std::to_string(i),i==128 ? "Master Volume" : resources::parameterNames[i],minimum,maximum,(initial-minimum)/(maximum-minimum)};
            values[i].store(initial);
            if(i<126)rawSourceValues[i]=&values[i];
        }
        std::string error;
        if(!load(std::string(reinterpret_cast<const char*>(resources::initPatch),resources::initPatchSize),error))throw std::runtime_error(error);
        patchName="Init";
        updateSynthParams();engine.initializeNativeRandomHistory();
    }
    float normalized(int i)const
    {const auto& p=info.at(i);return (values.at(i).load()-static_cast<float>(p.minimum))/static_cast<float>(p.maximum-p.minimum);}
    void setNormalized(int i,double normalizedValue)
    {
        if(!std::isfinite(normalizedValue))return;
        const auto& p=info.at(i);const auto n=static_cast<float>(std::clamp(normalizedValue,0.,1.));
        const float previous=values[i].exchange(static_cast<float>(p.minimum)+static_cast<float>(p.maximum-p.minimum)*n);
        if(i<126)parameterValueChanged(i,n,previous);
    }
    void setSource(int i,float value)
    {
        const auto& p=info[i];
        const float normalizedValue=(value-static_cast<float>(p.minimum))/static_cast<float>(p.maximum-p.minimum);
        setNormalized(i,normalizedValue);
    }
    std::array<uint64_t,2> runtimeEdits() const noexcept
    { return {nativeRuntimeEdits[0].load(),nativeRuntimeEdits[1].load()}; }
    std::array<int,99> initialControls() const noexcept
    { std::array<int,99> result;for(int i=0;i<99;++i)result[i]=nativeInitialControls[i].load();return result; }
    void storeNativeContext(const std::array<int,99>& values,std::array<uint64_t,2> edits,int version) noexcept
    { for(int i=0;i<99;++i)nativeInitialControls[i].store(values[i]);for(int i=0;i<2;++i)nativeRuntimeEdits[i].store(edits[i]);nativeSourceVersion.store(version); }
    std::array<uint64_t,2> sourcePresence() const noexcept
    { return {nativeSourcePresence[0].load(),nativeSourcePresence[1].load()}; }
    void storeSourcePresence(std::array<uint64_t,2> values) noexcept
    { for(size_t i=0;i<2;++i)nativeSourcePresence[i].store(values[i]); }


    void parameterValueChanged (int index, float value,float previous)
    {
        if (! replacingParameters.load())
        {
            if(index>=0 && index<126)
            {
                const auto oldEdits=nativeRuntimeEdits[index/64].fetch_or(uint64_t{1}<<(index%64));
                if(render::filterParameter(index) && nativeSourceLoading.load()
                    && (oldEdits&(uint64_t{1}<<(index%64)))==0)
                    previous=static_cast<float>(nativeInitialControls[index].load());
            }
            missingEnvelopeFields.fetch_and (~static_cast<unsigned> (synth1::patch::nativeEnvelopePresenceBit (index)));
            // Native RVA0x2a4d0 suppresses a consecutive identical integer
            // (parameter,dial) pair, rather than comparing the patch value.
            int dial=static_cast<int>(std::lround(synth2::sourceMinimum(index)+value*(synth2::sourceMaximum(index)-synth2::sourceMinimum(index))));
            if(index==67 || index==69)dial=dial!=0;
            const uint64_t pair=(static_cast<uint64_t>(index)<<32)|static_cast<uint32_t>(dial);
            const bool repeated=lastNativeParameterPair.exchange(pair)==pair;
            // Receiver edits retain fractional values. Consecutive identical
            // setters are neutral, including when separated by callbacks; an
            // intervening parameter makes the next setter effective again.
            uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));
            const uint64_t receiverPair=(static_cast<uint64_t>(index)<<32)|bits;
            const bool repeatedReceiver=lastReceiverParameterPair.exchange(receiverPair)==receiverPair;
            if(!repeatedReceiver && render::filterParameter(index))
            {
                const auto& info= this->info[index];
                const float next=static_cast<float>(info.minimum)+static_cast<float>(info.maximum-info.minimum)*value;
                filterEdits.push({index,previous/127.0f,next/127.0f,filterGeneration.load()});
            }
            if(!repeated && nativeRandomArpSeen.load())lfoHistory.push({4,-1,0,{}});
            if(!repeated && (index==43 || index==48))
                lfoHistory.push({0,index==43 ? 0 : 1,dial,{}});
            if(!repeated && (index==41 || index==42 || index==44 || index==46 || index==47 || index==49 || index==68 || index==70 || (index>=84 && index<=89) || index==31 || index==59))
                lfoHistory.push({4,index,dial,{}});
            if(!repeated && (index==67 || index==69))
                lfoHistory.push({3,index==67 ? 0 : 1,dial,{}});
        }
    }


    void flushNativeLfoHistory() noexcept
    {
        if(lfoHistory.droppedEvents()!=0)engine.invalidateNativeRandomHistory();
        synth2::dsp::LfoHistoryEvent event;
        // Bound work even if a producer keeps publishing during a callback.
        for(uint32_t count=0;count<decltype(lfoHistory)::capacity && lfoHistory.pop(event);++count)
            engine.applyNativeLfoHistory(event);
    }

    void queueNativeLfoLoad() noexcept
    {
        const int version=nativeSourceVersion.load();
        const int mappedVersion=static_cast<int>(std::lround((&values[126])->load()));
        const bool native=directControls || (nativeSourceLoading.load() && mappedVersion==version && synth1::patch::isNativeSourceVersion(version));
        const auto initial=initialControls();const auto edits=runtimeEdits();
        const auto control=[&](int index) {
            if(native && !directControls && (edits[index/64]&(uint64_t{1}<<(index%64)))==0)return initial[index];
            auto* value=(&values[static_cast<size_t>(index)]);
            return value!=nullptr ? static_cast<int>(std::lround(value->load())) : 0;
        };
        if(control(59)!=0 && control(31)==4)nativeRandomArpSeen.store(true);
        lfoHistory.push({native ? 2 : 1,control(43),control(48),
            {control(86),control(87),control(88),control(89)},{control(67)!=0,control(69)!=0},
            {{{control(41),control(42),control(44)},{control(46),control(47),control(49)}}},
            native && (!(control(59)!=0 && control(31)==4) || synth2::render::admitRandomArp({control(59)!=0,control(31),control(34),control(38),control(42),control(47)})),
            control(59)!=0 && control(31)==4});
    }

    void updateSynthParams()
    {
        updateParametersFromSy1();
    }

    void updateParametersFromSy1()
    {
        if(replacingParameters.load())return;
        auto& synthParams = engine.getParams();
        const int patchVersion=directControls ? currentPatchFormatVersion : static_cast<int>(std::round(rawPatchVersion->load()));

        if (! parameterMapper || parameterMappingVersion != patchVersion)
        {
            parameterMapper->resetSourceVersion(directControls ? currentPatchFormatVersion : patchVersion>=1 && patchVersion<=113 ? patchVersion : 105);
            parameterMappingVersion = patchVersion;
        }
        if (parameterMapper != nullptr)
        {
            parameterMapper->setDirectControls(directControls);
            flushNativeLfoHistory();
            parameterMapper->setRandomHistoryValid(engine.nativeRandomHistoryValid());
            parameterMapper->setMissingEnvelopeFields (missingEnvelopeFields.load());
            parameterMapper->setNativeSourceContext(nativeSourceLoading.load() && patchVersion==nativeSourceVersion.load() && synth1::patch::isNativeSourceVersion(patchVersion),sourcePresence(),initialControls(),runtimeEdits());
            parameterMapper->mapParameterValues (rawSourceValues, synthParams);
            engine.setRenderHistory(parameterMapper->getRenderHistory());
            synthParams.masterVolume = rawMasterVolume->load();
            if (phaseResetRequested.exchange (false)) engine.requestPhaseReset();
            // Restore the state before the pending edits from their recorded
            // source values, then apply every edit before MIDI can sample an
            // envelope's release level. No samples or shared clocks advance.
            const auto generation=filterGeneration.load();
            FilterEdit event;size_t count=0;
            for(size_t read=0;read<decltype(filterEdits)::capacity && filterEdits.pop(event);++read)
                if(event.generation==generation)pendingFilterEdits[count++]=event;
            if(count!=0)
            {
                const auto final=synthParams;
                for(size_t i=count;i>0;--i)
                {
                    const auto& edit=pendingFilterEdits[i-1];
                    render::applyFilterParameter(synthParams,edit.index,edit.before,engine.getRenderHistory().nativeMidiRoutingLoadHistory);
                }
                for(size_t i=0;i<count;++i)
                {
                    const auto& edit=pendingFilterEdits[i];engine.applyFilterParameter(edit.index,edit.after);
                }
                synthParams=final;
                engine.updateFilterEnvelopeConfiguration();
            }
            const auto drops=filterEdits.droppedEvents();
            if(drops!=appliedFilterDrops)
            {
                // Lost intermediate edits cannot be reconstructed. Publish the
                // final controls before MIDI, report every drop and recover on
                // the next accepted edit without inventing missing history.
                engine.updateFilterEnvelopeConfiguration();engine.restartFilterCutoff();
                appliedFilterDrops=drops;
            }
        }
    }
    bool load(const std::string& text,std::string& error)
    {
        try
        {
            if(isContinuousPatch(text))
            {
                const auto patch=parseContinuousPatch(text);
                replacingParameters.store(true);
                // Store exact parsed floats; a normalize/denormalize round trip
                // can otherwise lose a low bit at signed source ranges.
                for(int i=0;i<parameterCount;++i)values[i].store(patch.values[i]);
                directControls=true;continuousControls=true;
                parameterMapper->setContinuousControls(true);
                lastLoadedPreset=text;patchName=patch.name;
                missingEnvelopeFields.store(0);storeSourcePresence({});storeNativeContext({}, {},113);
                nativeSourceLoading.store(false);lastNativeParameterPair.store(~uint64_t{0});lastReceiverParameterPair.store(~uint64_t{0});filterGeneration.fetch_add(1);
                queueNativeLfoLoad();phaseResetRequested.store(true);replacingParameters.store(false);
                error.clear();return true;
            }
            const auto document=parsePatchDocument(text);
            if(document.version() && (*document.version()<1 || *document.version()>113))throw std::runtime_error("Unsupported Synth1 patch format version");
            const int version=document.version().value_or(hasPatchTitle(text) ? 100 : 105);
            const bool native=document.version() ? synth1::patch::isNativeSourceVersion(version) : hasPatchTitle(text);
            if(!native)for(const auto& e:document.entries)
                if(e.index<0 || e.index>=126 || e.rawValue<sourceMinimum(e.index) || e.rawValue>sourceMaximum(e.index))throw std::runtime_error("Patch value is outside the supported source parameter range");
            std::array<int,99> initial{};
            if(native)initial=synth1::patch::resolveNativePatchControls(document).values;
            std::array<uint64_t,2> presence{};unsigned missing=63;
            for(const auto& e:document.entries)
            {if(e.index<0 || e.index>=126)throw std::runtime_error("Invalid source index");presence[e.index/64]|=uint64_t{1}<<(e.index%64);missing&=~static_cast<unsigned>(synth1::patch::nativeEnvelopePresenceBit(e.index));}
            replacingParameters.store(true);
            directControls=false;
            for(int i=0;i<128;++i)setNormalized(i,info[i].defaultNormalized);
            if(native)
                for(int i=0;i<99;++i)setSource(i,static_cast<float>(std::clamp(synth1::patch::nativeDefaultSourceDial(i,version),sourceMinimum(i),sourceMaximum(i))));
            else if(document.version().value_or(105)>=104)
                for(int i:synth1::patch::nativeEnvelopeIndices)
                    if(missing&synth1::patch::nativeEnvelopePresenceBit(i))setNormalized(i,(i==13 ? 0.f : i==21 ? 127.f : 64.f)/127.f);
            setNormalized(126,version/127.f);
            for(const auto& e:document.entries)setSource(e.index,static_cast<float>(std::clamp(e.rawValue,sourceMinimum(e.index),sourceMaximum(e.index))));
            lastLoadedPreset=text;patchName=hasPatchTitle(text) ? document.name : "Untitled";missingEnvelopeFields.store(missing);storeSourcePresence(presence);
            storeNativeContext(initial,{},version);nativeSourceLoading.store(native);lastNativeParameterPair.store(~uint64_t{0});lastReceiverParameterPair.store(~uint64_t{0});filterGeneration.fetch_add(1);
            queueNativeLfoLoad();phaseResetRequested.store(true);replacingParameters.store(false);error.clear();return true;
        }
        catch(const std::exception& e){replacingParameters.store(false);error=e.what();return false;}
    }
    std::string savePatch()const
    {
        ContinuousPatch patch;patch.name=patchName;patch.metadata=readPatchMetadata(lastLoadedPreset);
        for(int i=0;i<parameterCount;++i)patch.values[i]=values[i].load();
        // Import conversion has already happened for native files. Export the
        // resolved dials, then overlay live edits, rather than converting again
        // when the Synth2 file is read.
        const int version=static_cast<int>(std::lround(values[126].load()));
        if(!directControls && nativeSourceLoading.load() && version==nativeSourceVersion.load())
        {
            const auto initial=initialControls();const auto edits=runtimeEdits();
            for(int i=0;i<99;++i)if(!(edits[i/64]&(uint64_t{1}<<(i%64))))
                patch.values[i]=static_cast<float>(std::clamp(initial[i],sourceMinimum(i),sourceMaximum(i)));
        }
        if(!continuousControls)for(int i=0;i<128;++i)patch.values[i]=std::round(patch.values[i]);
        return formatContinuousPatch(patch);
    }
    std::vector<uint8_t> save()const
    {
        ProcessorState state;state.oversamplingFactor=requestedOversampling.load();
        const int version=static_cast<int>(std::lround(values[126].load()));
        state.nativeSource=nativeSourceLoading.load() && version==nativeSourceVersion.load()
            && synth1::patch::isNativeSourceVersion(version);
        state.version=continuousControls ? 3 : state.nativeSource ? 2 : 1;
        state.directControls=directControls;
        for(int i=0;i<parameterCount;++i)state.values[i]=values[i].load();
        state.sourcePresence=sourcePresence();state.runtimeEdits=runtimeEdits();
        state.missingEnvelopeFields=missingEnvelopeFields.load();state.program=program;
        state.presetText=lastLoadedPreset;state.patchName=patchName;
        return encodeProcessorState(state,info);
    }
    bool restore(const void* data,int size)
    {
        auto state=decodeProcessorState(data,size,info);
        if(!state)return false;
        replacingParameters.store(true);
        requestedOversampling.store(state->oversamplingFactor);
        directControls=state->directControls;
        for(int i=0;i<parameterCount;++i)values[i].store(state->values[i]);
        patchName=std::move(state->patchName);program=state->program;lastLoadedPreset=std::move(state->presetText);
        missingEnvelopeFields.store(state->missingEnvelopeFields);storeSourcePresence(state->sourcePresence);
        storeNativeContext(state->initialControls,state->runtimeEdits,static_cast<int>(std::lround(state->values[126])));
        nativeSourceLoading.store(state->nativeSource);lastNativeParameterPair.store(~uint64_t{0});lastReceiverParameterPair.store(~uint64_t{0});filterGeneration.fetch_add(1);queueNativeLfoLoad();phaseResetRequested.store(true);replacingParameters.store(false);
        continuousControls=state->version==3;
        parameterMapper->setContinuousControls(continuousControls);return true;
    }
    template<class Sample> bool render(Sample* left,Sample* right,int frames,double bpm)
    {
        // A prepared zero-frame callback owns no sample memory and must retain
        // queued events, even when the host supplies null channel pointers.
        if(frames==0 && capacity>0)return true;
        if(!left || !right || frames<0 || frames>capacity || capacity==0)
        {
            if(left && frames>0)std::fill_n(left,frames,Sample{});if(right && frames>0)std::fill_n(right,frames,Sample{});
            eventCount=0;overflow=false;midi.clear();return false;
        }
        midi.clear();incoming.clear();
        if(overflow)for(int c=1;c<=16;++c)incoming.addEvent(audio::MidiMessage::allSoundOff(c),0);
        else for(size_t i=0;i<eventCount;++i)
        {const auto& e=events[i];if(e.offset>=frames){++drops;continue;}incoming.addEvent(audio::MidiMessage(e.status,e.data1,e.data2),e.offset);}
        eventCount=0;overflow=false;
        bool editorOverflow=false;
        editorMidi.consume([&](const auto& message){if(!midi.addEvent(message,0))editorOverflow=true;});
        for(const auto& event:incoming)
            editorMidi.host(event.message,[&](const auto& message){if(!midi.addEvent(message,event.samplePosition))editorOverflow=true;});
        if(editorOverflow) {
            ++drops;midi.clear();editorMidi.resetAudio();
            for(int c=1;c<=16;++c)midi.addEvent(audio::MidiMessage::allSoundOff(c),0);
        }
        float* pointers[]={scratch.getWritePointer(0),scratch.getWritePointer(1)};audio::AudioBuffer<float> buffer(pointers,2,frames);buffer.clear();
        const NoDenormals noDenormals;
        bpm=std::isfinite(bpm) && bpm>0 && bpm<=1000 ? bpm : 120;
        engine.getParams().hostBpm=static_cast<float>(bpm);
        const bool randomArp=values[59].load()!=0 && values[31].load()==4;
        if((randomArp && bpm!=120.) || frames>64)engine.invalidateNativeRandomHistory();
        for(const auto& metadata:midi)
        {
            const auto& message=metadata.getMessage();
            const int controller=message.isController() ? message.getControllerNumber() : -1;
            const bool supportedController=message.isPitchWheel() || controller==1 || controller==64 || controller==120 || controller==121 || controller==123;
            if((message.isController() || message.isChannelPressure() || message.isPitchWheel()) && !supportedController)engine.invalidateNativeRandomHistory();
        }
        updateSynthParams();
        const int desired=requestedOversampling.load();
        if(desired!=engine.getOversamplingFactor() && fadeOut==0)
        {fadeOut=64;fadeFrom=fadeGain;fadeIn=0;}
        engine.render(buffer,midi,0,frames);
        const bool changing=fadeOut>0;
        for(int i=0;i<frames;++i)
        {
            if(changing){fadeGain=fadeFrom*static_cast<float>(fadeOut)/64.f;if(fadeOut>0)--fadeOut;}
            else if(fadeIn>0){fadeGain=static_cast<float>(129-fadeIn)/128.f;--fadeIn;}
            left[i]=pointers[0][i];right[i]=pointers[1][i];
            if(changing || fadeIn>0 || fadeGain!=1){left[i]*=fadeGain;right[i]*=fadeGain;}
        }
        if(changing && fadeOut==0)
        {engine.setOversamplingFactor(desired);fadeIn=128;fadeGain=0;}
        return true;
    }
};
ProcessorBridge::ProcessorBridge():impl(std::make_unique<Impl>()){}
ProcessorBridge::~ProcessorBridge()=default;
const ParameterInfo& ProcessorBridge::parameter(int index)const{return impl->info.at(index);}
double ProcessorBridge::normalizedValue(int index)const{return impl->normalized(index);}
void ProcessorBridge::setNormalizedValue(int index,double value){impl->setNormalized(index,value);}
void ProcessorBridge::prepare(double rate,int frames)
{
    if(!std::isfinite(rate) || rate<=0 || frames<=0)throw std::invalid_argument("Invalid Synth2 playback configuration");
    impl->scratch.setSize(2,frames);impl->eventCount=0;impl->overflow=false;impl->midi.clear();
    impl->flushNativeLfoHistory();if(rate!=44100. || frames!=64)impl->engine.invalidateNativeRandomHistory();
    impl->engine.prepareToPlay(rate,frames);impl->engine.setOversamplingFactor(impl->requestedOversampling.load());
    impl->fadeOut=impl->fadeIn=0;impl->fadeGain=1;
    impl->editorMidi.resetAudio();impl->capacity=frames;
}
bool ProcessorBridge::setOversamplingFactor(int factor) noexcept
{if(!dsp::OversamplingSettings::valid(factor))return false;impl->requestedOversampling.store(factor);return true;}
int ProcessorBridge::oversamplingFactor() const noexcept {return impl->requestedOversampling.load();}
int ProcessorBridge::latencySamples() const noexcept {return dsp::OversamplingSettings::latency(oversamplingFactor());}
void ProcessorBridge::release(){impl->engine.releaseResources();impl->capacity=0;impl->eventCount=0;impl->overflow=false;impl->midi.clear();}
void ProcessorBridge::setEditorNote(int note,int velocity,EditorNoteSource source)noexcept {impl->editorMidi.set(note,velocity,source);}
void ProcessorBridge::releaseEditorNotes()noexcept {impl->editorMidi.release();}
bool ProcessorBridge::editorNotePressed(int note)const noexcept {return impl->editorMidi.pressed(note);}
bool ProcessorBridge::enqueueMidi(int offset,uint8_t status,uint8_t first,uint8_t second)noexcept
{
    if(offset<0 || status<0x80 || status>=0xf0 || first>127 || second>127){++impl->drops;return false;}
    if(impl->eventCount==midiCapacity){++impl->drops;impl->overflow=true;return false;}
    impl->events[impl->eventCount++]={offset,status,first,second};return true;
}
bool ProcessorBridge::process(float* l,float* r,int n,double bpm){return impl->render(l,r,n,bpm);}
bool ProcessorBridge::process(double* l,double* r,int n,double bpm){return impl->render(l,r,n,bpm);}
uint64_t ProcessorBridge::droppedMidiMessages()const noexcept{return impl->drops;}
uint64_t ProcessorBridge::droppedFilterControlEdits()const noexcept{return impl->filterEdits.droppedEvents();}
bool ProcessorBridge::loadPatch(const std::string& text,std::string& error){return impl->load(text,error);}
std::string ProcessorBridge::savePatch()const{return impl->savePatch();}
bool ProcessorBridge::loadProgram(const std::string& text,int index,std::string& error,const std::string& displayName)
{
    if(index<0 || static_cast<size_t>(index)>=PatchLibrary::maximumPatches){error="The selected patch is outside the supported bank range.";return false;}
    if(!impl->load(text,error))return false;
    impl->program=index;if(!displayName.empty())impl->patchName=displayName;return true;
}
std::string ProcessorBridge::currentPatchName()const{return impl->patchName;}
int ProcessorBridge::currentProgram()const noexcept{return impl->program;}
std::vector<uint8_t> ProcessorBridge::saveState()const{return impl->save();}
void ProcessorBridge::restoreState(const void* data,int size){impl->restore(data,size);}
bool ProcessorBridge::tryRestoreState(const void* data,int size){return impl->restore(data,size);}
const SynthEngine& ProcessorBridge::engineForInspection()const noexcept{return impl->engine;}
bool ProcessorBridge::readNativeControls(std::array<int,99>& out)const noexcept{return impl->parameterMapper && impl->parameterMapper->readNativeControls(out);}
int ProcessorBridge::mappedVersion()const noexcept{return impl->parameterMapper ? impl->parameterMapper->getSourceVersion() : 0;}
uint64_t ProcessorBridge::historyDrops()const noexcept{return impl->lfoHistory.droppedEvents();}

}
