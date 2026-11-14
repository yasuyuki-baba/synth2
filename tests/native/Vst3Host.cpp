#include "Module.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "host/ParameterInfo.h"
#include <algorithm>
#include <cstring>

namespace smoke
{
namespace
{
using namespace Steinberg;
using namespace Steinberg::Vst;

// These small host-owned interfaces live on the stack. The plugin may borrow
// them only during setState/process, and does not own their lifetime.
#define SMOKE_UNKNOWN(Interface) \
    tresult PLUGIN_API queryInterface(const TUID id, void** object) override \
    { \
        *object = nullptr; \
        if (!std::memcmp(id, INLINE_UID_OF(Interface), sizeof(TUID)) || \
            !std::memcmp(id, INLINE_UID_OF(FUnknown), sizeof(TUID))) \
        { *object = static_cast<Interface*>(this); addRef(); return kResultOk; } \
        return kNoInterface; \
    } \
    uint32 PLUGIN_API addRef() override { return 1; } \
    uint32 PLUGIN_API release() override { return 1; }

class MemoryStream final : public IBStream
{
public:
    explicit MemoryStream(const State& state) : bytes(state)
    {
        // The public iPlug2 VST3 component state appends an int32 bypass word.
        bytes.insert(bytes.end(), sizeof(int32), 0);
    }
    SMOKE_UNKNOWN(IBStream)
    tresult PLUGIN_API read(void* buffer, int32 count, int32* readBytes) override
    {
        if (count < 0) return kInvalidArgument;
        const auto n = std::min<size_t>(size_t(count), bytes.size() - position);
        std::memcpy(buffer, bytes.data() + position, n);
        position += n;
        if (readBytes) *readBytes = int32(n);
        return n ? kResultOk : kResultFalse;
    }
    tresult PLUGIN_API write(void*, int32, int32* written) override
    {
        if (written) *written = 0;
        return kNotImplemented;
    }
    tresult PLUGIN_API seek(int64 offset, int32 mode, int64* result) override
    {
        int64 next = offset;
        if (mode == kIBSeekCur) next += int64(position);
        else if (mode == kIBSeekEnd) next += int64(bytes.size());
        else if (mode != kIBSeekSet) return kInvalidArgument;
        if (next < 0 || next > int64(bytes.size())) return kInvalidArgument;
        position = size_t(next);
        if (result) *result = next;
        return kResultOk;
    }
    tresult PLUGIN_API tell(int64* result) override
    {
        if (!result) return kInvalidArgument;
        *result = int64(position);
        return kResultOk;
    }
private:
    State bytes;
    size_t position = 0;
};

class Events final : public IEventList
{
public:
    explicit Events(Midi midi) : count(midi == Midi::none ? 0 : 1)
    {
        if (midi == Midi::noteOn)
        {
            event.type = Event::kNoteOnEvent;
            event.noteOn.pitch = 69;
            event.noteOn.velocity = 1.0f;
            event.noteOn.noteId = -1;
        }
        else
        {
            event.type = Event::kNoteOffEvent;
            event.noteOff.pitch = 69;
            event.noteOff.noteId = -1;
        }
    }
    SMOKE_UNKNOWN(IEventList)
    int32 PLUGIN_API getEventCount() override { return count; }
    tresult PLUGIN_API getEvent(int32 index, Event& result) override
    {
        if (index != 0 || count == 0) return kInvalidArgument;
        result = event;
        return kResultOk;
    }
    tresult PLUGIN_API addEvent(Event&) override { return kNotImplemented; }
private:
    Event event{};
    int32 count;
};

class ParameterPoint final : public IParamValueQueue
{
public:
    ParameterPoint(ParamID id, ParamValue value) : id(id), value(value) {}
    SMOKE_UNKNOWN(IParamValueQueue)
    ParamID PLUGIN_API getParameterId() override { return id; }
    int32 PLUGIN_API getPointCount() override { return 1; }
    tresult PLUGIN_API getPoint(int32 index, int32& offset, ParamValue& result) override
    {
        if(index != 0) return kInvalidArgument;
        offset = 0; result = value; return kResultOk;
    }
    tresult PLUGIN_API addPoint(int32, ParamValue, int32&) override { return kNotImplemented; }
private:
    ParamID id;
    ParamValue value;
};

class ParameterEcho final : public IParameterChanges
{
public:
    SMOKE_UNKNOWN(IParameterChanges)
    int32 PLUGIN_API getParameterCount() override { return int32(points.size()); }
    IParamValueQueue* PLUGIN_API getParameterData(int32 index) override
    {
        return index >= 0 && size_t(index) < points.size() ? &points[size_t(index)] : nullptr;
    }
    IParamValueQueue* PLUGIN_API addParameterData(const ParamID&, int32&) override { return nullptr; }
    std::vector<ParameterPoint> points;
};
#undef SMOKE_UNKNOWN

class Vst3Host final : public Host
{
public:
    explicit Vst3Host(const std::filesystem::path& binary, bool echo) : module(binary), echoParameters(echo)
    {
#ifdef _WIN32
        require(module.symbol<bool (*)()>("InitDll")(), "VST3 InitDll failed");
        exit = module.symbol<bool (*)()>("ExitDll");
#elif defined(__APPLE__)
        require(module.symbol<bool (*)(CFBundleRef)>("bundleEntry")(module.bundle), "VST3 bundleEntry failed");
        exit = module.symbol<bool (*)()>("bundleExit");
#else
        require(module.symbol<bool (*)(void*)>("ModuleEntry")(module.nativeHandle()), "VST3 ModuleEntry failed");
        exit = module.symbol<bool (*)()>("ModuleExit");
#endif
        factory = module.symbol<IPluginFactory* (PLUGIN_API *)()>("GetPluginFactory")();
        require(factory != nullptr, "VST3 factory missing");
        for (int32 i = 0; i < factory->countClasses(); ++i)
        {
            PClassInfo info{};
            if (factory->getClassInfo(i, &info) == kResultOk &&
                std::strcmp(info.category, kVstAudioEffectClass) == 0)
            {
                require(factory->createInstance(info.cid, INLINE_UID_OF(IComponent),
                    reinterpret_cast<void**>(&component)) == kResultOk, "VST3 createInstance failed");
                break;
            }
        }
        require(component != nullptr, "VST3 audio component missing");
        require(component->initialize(nullptr) == kResultOk, "VST3 initialize failed");
        require(component->queryInterface(INLINE_UID_OF(IAudioProcessor),
            reinterpret_cast<void**>(&processor)) == kResultOk, "VST3 audio processor missing");
        require(processor->canProcessSampleSize(kSample32) == kResultOk, "VST3 float32 unsupported");
        SpeakerArrangement stereo = SpeakerArr::kStereo;
        require(processor->setBusArrangements(nullptr, 0, &stereo, 1) == kResultOk, "VST3 stereo setup failed");
        require(component->activateBus(kAudio, kOutput, 0, true) == kResultOk, "VST3 output bus activation failed");
        require(component->activateBus(kEvent, kInput, 0, true) == kResultOk, "VST3 MIDI bus activation failed");
    }
    ~Vst3Host() override
    {
        stop();
        if (processor) processor->release();
        if (component) { component->terminate(); component->release(); }
        if (factory) factory->release();
        if (exit) exit();
    }
    void load(const State& state) override
    {
        stop();
        MemoryStream stream(state);
        require(component->setState(&stream) == kResultOk, "VST3 setState failed");
        ProcessSetup setup{};
        setup.processMode = kOffline;
        setup.symbolicSampleSize = kSample32;
        setup.maxSamplesPerBlock = frames;
        setup.sampleRate = sampleRate;
        require(processor->setupProcessing(setup) == kResultOk, "VST3 setupProcessing failed");
        require(component->setActive(true) == kResultOk, "VST3 setActive failed");
        active = true;
        require(processor->setProcessing(true) == kResultOk, "VST3 setProcessing failed");
        processing = true;
        position = 0;
        parameterEcho.points.clear();
        if(echoParameters)
        {
            IEditController* controller = nullptr;
            require(component->queryInterface(INLINE_UID_OF(IEditController),
                reinterpret_cast<void**>(&controller)) == kResultOk, "VST3 controller missing");
            // Sound parameters and quality only; the SDK's synthetic MIDI-CC
            // parameters are events, not readback of the loaded patch.
            for(int32 i = 0; i < controller->getParameterCount(); ++i)
            {
                ParameterInfo info{};
                if(controller->getParameterInfo(i, info) == kResultOk &&
                    info.id <= synth2::host::processorParameterCount)
                    parameterEcho.points.emplace_back(info.id, controller->getParamNormalized(info.id));
            }
            controller->release();
            require(parameterEcho.points.size() == synth2::host::processorParameterCount + 1,
                "Incomplete VST3 parameter readback");
        }
    }
    void render(Block& output, Midi midi) override
    {
        Events events(midi);
        float* channels[]{output[0].data(), output[1].data()};
        AudioBusBuffers audio{};
        audio.numChannels = 2;
        audio.channelBuffers32 = channels;
        ProcessContext context{};
        context.state = ProcessContext::kTempoValid;
        context.sampleRate = sampleRate;
        context.projectTimeSamples = position;
        context.tempo = 120.0;
        ProcessData data{};
        data.processMode = kOffline;
        data.symbolicSampleSize = kSample32;
        data.numSamples = frames;
        data.numOutputs = 1;
        data.outputs = &audio;
        data.inputEvents = &events;
        data.inputParameterChanges = &parameterEcho;
        data.processContext = &context;
        require(processor->process(data) == kResultOk, "VST3 process failed");
        parameterEcho.points.clear();
        position += frames;
    }
private:
    void stop()
    {
        if (processing) processor->setProcessing(false);
        if (active) component->setActive(false);
        processing = active = false;
    }
    Module module;
    bool (*exit)() = nullptr;
    IPluginFactory* factory = nullptr;
    IComponent* component = nullptr;
    IAudioProcessor* processor = nullptr;
    bool active = false, processing = false;
    bool echoParameters;
    ParameterEcho parameterEcho;
    int64 position = 0;
};
}
std::unique_ptr<Host> makeVst3(const std::filesystem::path& binary, bool echoParameters)
{
    return std::make_unique<Vst3Host>(binary, echoParameters);
}
}
