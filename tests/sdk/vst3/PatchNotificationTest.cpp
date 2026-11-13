#include "Synth2.h"
#include "patch/PatchFormat.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

// UI callbacks are outside this test. The constructor, state/parameter methods,
// patch notification, processor and VST3 host API are the production code.
void Synth2::OnIdle() {}
void Synth2::OnUIOpen() {}
void Synth2::OnUIClose() {}
void Synth2::OnParentWindowResize(int,int) {}

namespace
{
void require(bool condition,const char* message)
{
    if(!condition)throw std::runtime_error(message);
}
struct Host final : Steinberg::Vst::IComponentHandler
{
    Synth2* plugin{};
    unsigned reads=0,mismatches=0,latencyChanges=0;
    Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid,void** result) override
    {
        if(Steinberg::FUnknownPrivate::iidEqual(iid,Steinberg::Vst::IComponentHandler::iid) ||
           Steinberg::FUnknownPrivate::iidEqual(iid,Steinberg::FUnknown::iid)) {
            *result=static_cast<Steinberg::Vst::IComponentHandler*>(this);addRef();return Steinberg::kResultOk;
        }
        *result=nullptr;return Steinberg::kNoInterface;
    }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }
    Steinberg::tresult PLUGIN_API beginEdit(Steinberg::Vst::ParamID) override { return Steinberg::kResultOk; }
    Steinberg::tresult PLUGIN_API performEdit(Steinberg::Vst::ParamID id,Steinberg::Vst::ParamValue value) override
    {
        ++reads;
        if(std::abs(plugin->getParamNormalized(id)-value)>1e-12)++mismatches;
        return Steinberg::kResultOk;
    }
    Steinberg::tresult PLUGIN_API endEdit(Steinberg::Vst::ParamID) override { return Steinberg::kResultOk; }
    Steinberg::tresult PLUGIN_API restartComponent(Steinberg::int32 flags) override
    {
        if(flags & Steinberg::Vst::kLatencyChanged)++latencyChanges;
        return Steinberg::kResultOk;
    }
};
}

struct Synth2Vst3PatchTest
{
    static double energy(Synth2& plugin)
    {
        iplug::IMidiMsg note;note.MakeNoteOnMsg(60,100,0);plugin.ProcessMidiMsg(note);
        std::array<iplug::sample,128> left{},right{};
        iplug::sample* outputs[] {left.data(),right.data()};
        double result=0;
        for(int block=0;block<32;++block) {
            plugin.ProcessBlock(nullptr,outputs,128);
            if(block>=16)for(int i=0;i<128;++i)result+=left[i]*left[i]+right[i]*right[i];
        }
        return result;
    }
    static void run()
    {
        Host host;
        Synth2 plugin {iplug::InstanceInfo{}};host.plugin=&plugin;
        require(plugin.initialize(&host)==Steinberg::kResultOk,"VST3 initialize failed");
        plugin.setComponentHandler(&host);
        Steinberg::Vst::ProcessSetup setup {};
        setup.sampleRate=48000;setup.maxSamplesPerBlock=128;setup.symbolicSampleSize=Steinberg::Vst::kSample64;
        require(plugin.setupProcessing(setup)==Steinberg::kResultOk,"VST3 processing setup failed");
        plugin.setActive(true);
        const auto patch=[](int gain) {
            return std::string("VST3 synthetic patch\n0,1\n1,1\n19,127\n25,0\n26,0\n27,127\n28,0\n")+
                "29,"+std::to_string(gain)+"\n37,0\n56,0\n65,0\n66,0\n77,0\nver=113\n";
        };
        std::string error;
        require(plugin.processor.loadProgram(patch(100),0,error),"Sounding patch load failed");
        plugin.notifyHostOfPatch();
        const double sounding=energy(plugin);
        require(sounding>1e-8,"Generated sounding patch did not produce audio");
        require(plugin.processor.loadProgram(patch(0),1,error),"Silent patch load failed");
        const auto before=plugin.processor.saveState();
        plugin.notifyHostOfPatch();
        require(plugin.processor.saveState()==before,"Host notification changed loaded processor state");
        const double muted=energy(plugin);
        require(muted==0,"Loaded silent patch did not silence the engine");
        // A host may query the controller's current value and later return it
        // through process(inputParameterChanges). Exercise that real SDK path.
        Steinberg::Vst::ParameterChanges changes;
        Steinberg::int32 queueIndex=0,pointIndex=0;
        auto* queue=changes.addParameterData(29,queueIndex);
        queue->addPoint(0,plugin.getParamNormalized(29),pointIndex);
        Steinberg::Vst::ProcessData data {};data.inputParameterChanges=&changes;
        iplug::IPlugQueue<iplug::IMidiMsg> outgoing {128};
        plugin.ProcessParameterChanges(data,outgoing);
        const double echoed=energy(plugin);
        std::cout << "Audio energy: sounding=" << sounding << "; muted=" << muted << "; host readback echo=" << echoed << '\n';
        std::cout << "Host readbacks: " << host.reads << "; stale: " << host.mismatches << '\n';
        require(echoed==0,"Host readback echo restored the previous audible gain");
        require(host.reads==static_cast<unsigned>(plugin.NParams()*2),"Host did not receive both patches");
        for(int i=0;i<plugin.NParams();++i)
            require(std::abs(plugin.getParamNormalized(i)-plugin.GetParam(i)->GetNormalized())<1e-12,
                    "VST3 host readback differs from the loaded SDK parameter");
        require(host.mismatches==0,"VST3 exposed stale values during patch notification");
        // Discrete SDK presentation must not round fractional recalled bridge
        // controls back into the engine while publishing a patch.
        auto fractional=synth2::parseContinuousPatch(plugin.processor.savePatch());
        fractional.values[64]=2.25f;fractional.values[19]=63.75f;
        require(plugin.processor.loadProgram(synth2::formatContinuousPatch(fractional),2,error),"Fractional patch load failed");
        const auto fractionalBefore=plugin.processor.saveState();
        plugin.notifyHostOfPatch();
        require(plugin.processor.saveState()==fractionalBefore,"Notification rounded fractional recalled values");
        // Repeated UI ticks must not repeatedly request a latency restart.
        for(int factor:{4,1})
        {
            const int previous=plugin.GetLatency();
            plugin.processor.setOversamplingFactor(factor);
            energy(plugin);
            const int next=plugin.processor.latencySamples();
            require(factor==1 ? next==0 : next>0,"Oversampling did not reach the requested latency");
            const auto notifications=host.latencyChanges;
            plugin.syncQualityLatency();
            require(plugin.GetLatency()==next,"SDK latency differs from the processor latency");
            require(host.latencyChanges==notifications+(previous!=next),"Latency change was not published once");
            plugin.syncQualityLatency();plugin.syncQualityLatency();
            require(host.latencyChanges==notifications+(previous!=next),"Unchanged latency requested another restart");
        }
        plugin.setActive(false);
        plugin.setComponentHandler(nullptr);plugin.terminate();
    }
};
int main()
{
    try { Synth2Vst3PatchTest::run(); }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
