#include "support/PooledTestVoice.h"
#include <gtest/gtest.h>
#include "../tools/host_scenarios.h"
#include "midi/ControllerRuntime.h"
#include "SynthEngine.h"
#include "patch/PatchFormat.h"
#include <array>
#include <vector>

namespace
{
struct Event { int frame; uint8_t status, first, second; };
// Events are absolute and stably ordered. Each is enqueued once in the owning
// half-open callback interval; this preserves simultaneous and boundary events.
std::vector<double> render(const std::vector<Event>& events, const std::vector<int>& sizes,
                          const std::string& patch=hostvalidation::patch(), int total=16384)
{
    synth2::host::ProcessorBridge processor;
    std::string error;
    if (!processor.loadPatch(patch, error)) throw std::runtime_error(error);
    processor.prepare(48000, *std::max_element(sizes.begin(), sizes.end()));
    std::vector<double> result;
    size_t next=0, block=0;
    for (int start=0; start<total;)
    {
        const int frames=std::min(sizes[block++ % sizes.size()], total-start);
        while (next<events.size() && events[next].frame<start+frames)
        {
            const auto& event=events[next++];
            EXPECT_GE(event.frame, start);
            EXPECT_TRUE(processor.enqueueMidi(event.frame-start, event.status, event.first, event.second));
        }
        std::vector<double> left(frames), right(frames);
        EXPECT_TRUE(processor.process(left.data(), right.data(), frames, 120));
        for (int i=0; i<frames; ++i)
        {
            EXPECT_TRUE(std::isfinite(left[i])); EXPECT_TRUE(std::isfinite(right[i]));
            result.push_back(left[i]); result.push_back(right[i]);
        }
        start+=frames;
    }
    EXPECT_EQ(next, events.size());
    return result;
}
double energy(const std::vector<double>& audio, int begin, int end)
{
    double result=0;
    for (int i=begin*2; i<end*2; ++i) result+=double(audio[i])*audio[i];
    return result;
}
}

TEST(PerformanceBehavior, FractionalPortamentoTimeChangesRepeatedNoteAudio)
{
    const std::vector<Event> events{{0,0x90,60,100},{1024,0x90,72,100},{1152,0x80,60,0},
        {4096,0x90,67,100},{4224,0x80,72,0},{8192,0x80,67,0}};
    for(int mode:{1,2})
    {
        const auto patch=[&](const std::string& dial) {
            auto source=hostvalidation::processor();
            hostvalidation::load(*source,hostvalidation::patch(1,mode));
            auto modern=synth2::parseContinuousPatch(source->savePatch());
            modern.name="Fractional glide";modern.values[39]=std::stof(dial);
            return synth2::formatContinuousPatch(modern);
        };
        const auto first=render(events,{64},patch("64.125"));
        const auto second=render(events,{64},patch("64.375"));
        EXPECT_NE(first,second);EXPECT_GT(energy(first,1024,8192),1e-8);
        EXPECT_GT(energy(second,1024,8192),1e-8);
    }
}

namespace
{
// A marker voice makes dispatch timing observable without oscillator/filter,
// RNG, receiver partitions or output-gain history. The dispatcher is production.
class MarkerVoice final : public synth2::audio::Voice
{
public:
    bool canPlaySound(synth2::audio::Sound*) override { return true; }
    void startNote(int note,float,synth2::audio::Sound*,int) override { value=note; }
    void stopNote(float,bool) override { value=0; clearCurrentNote(); }
    void pitchWheelMoved(int) override {}
    void controllerMoved(int,int) override {}
    void renderNextBlock(synth2::audio::AudioBuffer<float>& output,int start,int frames) override
    { for(int i=start;i<start+frames;++i) output.setSample(0,i,static_cast<float>(value)); }
private:
    int value=0;
};
std::vector<double> dispatch(const std::vector<Event>& events,const std::vector<int>& sizes)
{
    synth2::dsp::CrtRandom arpRandom;synth2::VoiceDispatcher dispatcher(arpRandom);
    dispatcher.addVoice(new MarkerVoice);
    dispatcher.addSound(new synth2::SynthSound);
    dispatcher.arpeggiator.prepare(48000);
    std::vector<double> output;
    size_t block=0,next=0;
    for(int start=0;start<1024;)
    {
        const int frames=std::min(sizes[block++ % sizes.size()],1024-start);
        constexpr int prefix=7;
        synth2::audio::AudioBuffer<float> audio(2,frames+2*prefix);
        audio.clear();
        synth2::audio::MidiBuffer midi;
        while(next<events.size() && events[next].frame<start+frames)
        {
            const auto& e=events[next++];
            midi.addEvent(synth2::audio::MidiMessage(e.status,e.first,e.second),prefix+e.frame-start);
        }
        dispatcher.renderRegion(audio,midi,prefix,frames);
        output.insert(output.end(),audio.getReadPointer(0)+prefix,audio.getReadPointer(0)+prefix+frames);
        for(int i=0;i<prefix;++i)
        { EXPECT_EQ(audio.getSample(0,i),0); EXPECT_EQ(audio.getSample(0,prefix+frames+i),0); }
        start+=frames;
    }
    EXPECT_EQ(next,events.size());
    return output;
}
}

TEST(PerformanceBehavior, DispatcherPreservesSamplePositionsAndSameTimeOrderAcrossBlocks)
{
    const std::vector<Event> events{{13,0x90,60,100},{63,0x80,60,0},
        {64,0x90,67,90},{64,0x80,67,0},{257,0x80,67,0},{257,0x90,72,100},
        {513,0xb0,64,127},{777,0x80,72,0},{900,0xb0,64,0}};
    std::vector<double> expected(1024);
    std::fill(expected.begin()+13,expected.begin()+63,60);
    std::fill(expected.begin()+257,expected.begin()+900,72);
    for(const auto& sizes : {std::vector<int>{256},std::vector<int>{64},
                             std::vector<int>{1,17,63,257,31}})
        EXPECT_EQ(dispatch(events,sizes),expected);
}

TEST(PerformanceBehavior, SustainKeepsReleasedKeysAudibleUntilPedalUp)
{
    const auto held=render({{0,0x90,60,100},{512,0xb0,64,127},{1024,0x80,60,0},
                            {4096,0xb0,64,0}}, {64});
    const auto released=render({{0,0x90,60,100},{1024,0x80,60,0}}, {64});
    EXPECT_GT(energy(held,2048,4096),0.01);
    // Release means inaudible (-100 dBFS RMS), not erased physical histories.
    EXPECT_LT(std::sqrt(energy(released,8192,16384)/(2*8192)),1e-5);
    EXPECT_LT(std::sqrt(energy(held,8192,16384)/(2*8192)),1e-5);
}

TEST(PerformanceBehavior, ResetControllersCentersOrdinaryBendWithoutLiftingSustain)
{
    const std::vector<Event> prefix{{0,0x90,60,100},{256,0xe0,0,96},
        {512,0xb0,64,127},{1024,0x80,60,0}};
    auto reset=prefix, center=prefix;
    reset.push_back({2048,0xb0,121,0}); center.push_back({2048,0xe0,0,64});
    reset.push_back({4096,0xb0,64,0}); center.push_back({4096,0xb0,64,0});
    const auto audio=render(reset,{64});
    EXPECT_EQ(audio,render(center,{64}));
    EXPECT_GT(energy(audio,2304,4096),0.01);
    EXPECT_LT(std::sqrt(energy(audio,8192,16384)/(2*8192)),1e-5);
}

TEST(PerformanceBehavior, PerformanceMessagesNeverModifySavedPatchOrSourceGetters)
{
    synth2::host::ProcessorBridge processor;
    std::string error;
    ASSERT_TRUE(processor.loadPatch(hostvalidation::patch()+"50,127\n51,127\n86,45057\n87,19\n88,57344\n89,29\n",error));
    const auto saved=processor.saveState();
    std::array<double,129> values{};
    for (int i=0;i<129;++i) values[i]=processor.normalizedValue(i);
    processor.prepare(48000,64);
    std::array<double,64> left{},right{};
    // Mapping is lazy; establish the baseline at a callback boundary.
    ASSERT_TRUE(processor.process(left.data(),right.data(),64,120));
    std::array<int,99> controls{};
    ASSERT_TRUE(processor.readNativeControls(controls));
    const auto& routes=processor.engineForInspection().getRenderHistory().controllerRoutes;
    ASSERT_EQ(routes[0].destination,19); ASSERT_EQ(routes[1].destination,29);
    ASSERT_EQ(routes[0].sensitivity,1); ASSERT_EQ(routes[1].sensitivity,1);
    for (const auto& event : std::vector<Event>{{0,0x90,60,100},{0,0xb0,1,127},
             {0,0xe0,0,100},{0,0xb0,64,127},{0,0xb0,121,0},{0,0x80,60,0},{0,0xb0,64,0}})
    {
        ASSERT_TRUE(processor.enqueueMidi(0,event.status,event.first,event.second));
        ASSERT_TRUE(processor.process(left.data(),right.data(),64,120));
        EXPECT_EQ(processor.saveState(),saved);
        for (int i=0;i<129;++i) EXPECT_EQ(processor.normalizedValue(i),values[i])<<i;
        std::array<int,99> after{};
        ASSERT_TRUE(processor.readNativeControls(after));
        EXPECT_EQ(after,controls);
    }
}

TEST(ControllerBehavior, RoutesSumBeforeClampingAndSelectiveResetPreservesOtherInputs)
{
    using namespace synth2::midi;
    ControllerRuntime runtime;
    const std::array<ControllerRoute,2> routes{{{45057,19,64,0,127,1},
                                               {57344,19,64,0,127,-1}}};
    const auto ignore=[](int,int){};
    runtime.message(0xb0,1,127,routes,ignore);
    EXPECT_EQ(runtime.dial(0,routes),127);
    runtime.message(0xe0,0,96,routes,ignore); // Half positive bend subtracts 64.
    EXPECT_EQ(runtime.dial(0,routes),127);
    runtime.message(0xb0,1,95,routes,ignore);
    EXPECT_EQ(runtime.dial(0,routes),95); // Sum 64 + 95/127*128 - 64 before clamp.
    runtime.message(0xb0,1,0,routes,ignore);
    EXPECT_EQ(runtime.dial(0,routes),0);
    runtime.reset({false,true});
    EXPECT_EQ(runtime.dial(0,routes),64);
    runtime.message(0xb0,1,64,routes,ignore);
    const auto wheel=runtime.inputs[0];
    runtime.reset({false,true});
    EXPECT_EQ(runtime.inputs[0],wheel);
    runtime.reset();
    EXPECT_EQ(runtime.dial(0,routes),64);
}

TEST(PerformanceBehavior, BendUsesOneReceiverWithChannelIsolationAndNativeOmniIngress)
{
    for(bool omni:{false,true})
    {
        synth2::SynthEngine engine;
        auto history=engine.getRenderHistory();history.nativeMidiRoutingLoadHistory=omni;
        engine.setRenderHistory(history);engine.getParams().pitchBendRange=12;
        engine.prepareToPlay(44100,64);
        synth2::audio::AudioBuffer<float> output(2,64);
        synth2::audio::MidiBuffer midi;
        midi.addEvent(synth2::audio::MidiMessage::pitchWheel(2,16383),0);
        engine.render(output,midi,0,64);
        const int channel=omni ? 1 : 2;
        EXPECT_EQ(engine.pitchBendWords(channel),(std::array<int,4>{2,63,1,4}));
        EXPECT_EQ(engine.pitchBendWords(omni ? 2 : 1),(std::array<int,4>{0,0,0,0}));
        midi.clear();midi.addEvent(synth2::audio::MidiMessage::controllerEvent(2,121,0),0);
        engine.render(output,midi,0,64);
        EXPECT_EQ(engine.pitchBendWords(channel),(std::array<int,4>{0,0,0,0}));
    }
}

TEST(ControllerBehavior, WheelUsesConfiguredFmAndFilterRoutesWithoutChangingStoredControls)
{
    const std::vector<Event> notes{{0,0x90,60,100},{8192,0x80,60,0}};
    auto moved=notes;moved.insert(moved.begin()+1,{1024,0xb0,1,127});
    for(int destination:{19,45})
    {
        const auto patch=hostvalidation::patch()+"19,40\n50,127\n86,45057\n87,"+std::to_string(destination)+"\n";
        const auto steady=render(notes,{64},patch);
        const auto changed=render(moved,{64},patch);
        EXPECT_EQ(std::vector<double>(steady.begin(),steady.begin()+2048),
                  std::vector<double>(changed.begin(),changed.begin()+2048));
        EXPECT_NE(steady,changed)<<destination;
    }
}

TEST(PerformanceBehavior, OrdinaryBendAdvancesWithoutAnLfoProvider)
{
    synth2::RenderControls controls;controls.pitchBendRange=12;
    synth2::dsp::CrtRandom arpRandom;synth2::VoiceDispatcher dispatcher(arpRandom);dispatcher.params=&controls;
    dispatcher.setCurrentPlaybackSampleRate(44100);
    dispatcher.addSound(new synth2::SynthSound);
    auto* voice=new synth2::test::PooledTestVoice(controls);voice->prepare(44100);dispatcher.addVoice(voice);
    synth2::audio::AudioBuffer<float> audio(2,64);audio.clear();
    synth2::audio::MidiBuffer midi;
    midi.addEvent(synth2::audio::MidiMessage::pitchWheel(2,16383),0);
    midi.addEvent(synth2::audio::MidiMessage::noteOn(2,69,1.f),0);
    dispatcher.renderRegion(audio,midi,0,64);
    EXPECT_NEAR(voice->getCurrentFrequency(),440*std::pow(2.,4./128),.001);
    EXPECT_EQ(dispatcher.pitchBendWords(2),(std::array<int,4>{2,63,1,4}));
    EXPECT_EQ(dispatcher.pitchBendWords(1),(std::array<int,4>{0,0,0,0}));
}

TEST(PerformanceBehavior, DoubleHostOutputIsAnExactWideningOfFloatDsp)
{
    for(int factor:{1,2,4})
    {
        auto first=hostvalidation::processor(),second=hostvalidation::processor();
        for(auto* processor:{first.get(),second.get()})
        {
            hostvalidation::load(*processor,hostvalidation::patch(2,1));
            processor->setOversamplingFactor(factor);processor->prepare(48000,64);
        }
        synth2::audio::AudioBuffer<double> precise(2,64);
        synth2::audio::AudioBuffer<float> narrow(2,64);
        synth2::audio::MidiBuffer midi;hostvalidation::notes(midi,1);
        for(int block=0;block<20;++block)
        {
            hostvalidation::process(*first,precise,midi);hostvalidation::process(*second,narrow,midi);
            midi.clear();
            for(int channel=0;channel<2;++channel)for(int frame=0;frame<64;++frame)
            {
                const double value=precise.getSample(channel,frame);
                ASSERT_TRUE(std::isfinite(value));
                EXPECT_EQ(narrow.getSample(channel,frame),static_cast<float>(value));
                EXPECT_EQ(value,static_cast<double>(narrow.getSample(channel,frame)));
            }
        }
    }
}

TEST(PerformanceBehavior, SameTimeLegatoReplacementRetainsPhysicalAmplitudePhase)
{
    auto reference=hostvalidation::processor(),replacement=hostvalidation::processor();
    for(auto* processor:{reference.get(),replacement.get()}){
        hostvalidation::load(*processor,hostvalidation::patch(1,2)+"25,90\n26,100\n27,100\n");
        processor->prepare(48000,64);
        ASSERT_TRUE(processor->enqueueMidi(0,144,60,100));
        std::array<float,64> left{},right{};
        for(int i=0;i<8;++i)ASSERT_TRUE(processor->process(left.data(),right.data(),64,120));
    }
    std::array<int,13> before{};
    ASSERT_TRUE(synth2::readPhysicalCoreEnvelopeWords(*reference,0,before));
    ASSERT_GT(before[12],0);
    ASSERT_TRUE(reference->enqueueMidi(31,176,64,0));
    ASSERT_TRUE(replacement->enqueueMidi(31,128,60,0));
    ASSERT_TRUE(replacement->enqueueMidi(31,176,64,0));
    ASSERT_TRUE(replacement->enqueueMidi(31,144,60,100));
    std::array<float,64> left{},right{};
    ASSERT_TRUE(reference->process(left.data(),right.data(),64,120));
    ASSERT_TRUE(replacement->process(left.data(),right.data(),64,120));
    std::array<int,13> expected{},actual{};
    ASSERT_TRUE(synth2::readPhysicalCoreEnvelopeWords(*reference,0,expected));
    ASSERT_TRUE(synth2::readPhysicalCoreEnvelopeWords(*replacement,0,actual));
    EXPECT_EQ(actual[10],expected[10]);EXPECT_EQ(actual[11],expected[11]);EXPECT_EQ(actual[12],expected[12]);
}

TEST(PerformanceBehavior, PolyphonicChordAssignsRecentKeysToFirstPhysicalCores)
{
    for(const auto& keys:{std::array<int,3>{48,60,67},std::array<int,3>{67,48,60},std::array<int,3>{67,60,48}})
    for(int frames:{64,127,257})
    {
        auto chord=hostvalidation::processor();
        hostvalidation::load(*chord,hostvalidation::patch());chord->prepare(48000,frames);
        for(int key:keys)ASSERT_TRUE(chord->enqueueMidi(13,144,key,100));
        std::array<float,257> left{},right{};
        ASSERT_TRUE(chord->process(left.data(),right.data(),frames,120));
        for(size_t slot=0;slot<keys.size();++slot)
        {
            auto single=hostvalidation::processor();
            hostvalidation::load(*single,hostvalidation::patch());single->prepare(48000,frames);
            ASSERT_TRUE(single->enqueueMidi(13,144,keys[2-slot],100));
            ASSERT_TRUE(single->process(left.data(),right.data(),frames,120));
            std::array<uint32_t,9> actual{},expected{};
            ASSERT_TRUE(synth2::readPhysicalCoreWords(*chord,slot,actual));
            ASSERT_TRUE(synth2::readPhysicalCoreWords(*single,0,expected));
            EXPECT_EQ(actual[2],expected[2]);EXPECT_EQ(actual[3],expected[3]);
        }
    }
}

TEST(PerformanceBehavior, CancelledAndRepeatedPendingKeysDoNotStartExtraVoices)
{
    const auto patch=hostvalidation::patch()+"90,0\n";
    const auto expected=render({{13,144,67,100}}, {64},patch);
    for(const auto& events:{std::vector<Event>{{13,144,60,75},{13,128,60,0},{13,144,67,100}},
                             std::vector<Event>{{13,144,67,42},{13,144,67,100}}})
        EXPECT_EQ(render(events,{64},patch),expected);
}

TEST(PerformanceBehavior, PolyphonicAdmissionIgnoresOlderPendingKeysBeyondCapacity)
{
    const auto patch=hostvalidation::patch()+"90,0\n94,2\n";
    for(const auto& sizes:{std::vector<int>{64},std::vector<int>{1,17,64,127,63,32,257}})
        EXPECT_EQ(render({{13,144,48,100},{13,144,60,100},{13,144,67,100}},sizes,patch),
                  render({{13,144,60,100},{13,144,67,100}},sizes,patch));
}

TEST(PerformanceBehavior, PedalHeldPendingKeyReleasesAfterPedalUp)
{
    const auto audio=render({{0,176,64,127},{0,144,60,100},{0,128,60,0},{1024,176,64,0}}, {64});
    EXPECT_GT(energy(audio,512,1024),0.01);
    EXPECT_LT(std::sqrt(energy(audio,8192,16384)/(2*8192)),1e-5);
}

TEST(PerformanceBehavior, FineSettersPreserveTheirDistinctFrequencyRefreshRules)
{
    const auto patch=hostvalidation::patch()+"9,1\n11,64\n12,127\n13,127\n";
    auto edited=hostvalidation::processor(),target=hostvalidation::processor();
    hostvalidation::load(*edited,patch);hostvalidation::load(*target,patch+"2,68\n");
    std::array<float,64> left{},right{};
    for(auto* processor:{edited.get(),target.get()})
    {
        processor->prepare(48000,64);ASSERT_TRUE(processor->enqueueMidi(0,144,60,100));
        ASSERT_TRUE(processor->process(left.data(),right.data(),64,120));
    }
    edited->setNormalizedValue(2,68.0f/127.0f);
    for(int block=0;block<64;++block)ASSERT_TRUE(edited->process(left.data(),right.data(),64,120));
    std::array<uint32_t,9> held{},unchanged{},refreshed{},expected{};
    ASSERT_TRUE(synth2::readPhysicalCoreWords(*edited,0,held));
    ASSERT_TRUE(synth2::readPhysicalCoreWords(*target,0,expected));
    ASSERT_NE(held[3],expected[3]);
    // A master-fine edit inside the zero band does not refresh the frequency.
    edited->setNormalizedValue(72,65.0f/127.0f);
    ASSERT_TRUE(edited->process(left.data(),right.data(),64,120));
    ASSERT_TRUE(synth2::readPhysicalCoreWords(*edited,0,unchanged));
    EXPECT_EQ(unchanged[3],held[3]);
    // OSC2 Fine republishes even when its resolved fine value is still zero.
    edited->setNormalizedValue(3,65.0f/127.0f);
    ASSERT_TRUE(edited->process(left.data(),right.data(),64,120));
    ASSERT_TRUE(synth2::readPhysicalCoreWords(*edited,0,refreshed));
    EXPECT_EQ(refreshed[3],expected[3]);
}

TEST(PerformanceBehavior, LiveFmBaseSetterStartsTheFullSlewHorizon)
{
    auto processor=hostvalidation::processor();
    hostvalidation::load(*processor,hostvalidation::patch()+"45,40\n");
    processor->prepare(48000,64);ASSERT_TRUE(processor->enqueueMidi(0,144,60,100));
    std::array<float,64> left{},right{};
    for(int block=0;block<8;++block)ASSERT_TRUE(processor->process(left.data(),right.data(),64,120));
    processor->setNormalizedValue(45,56.0f/127.0f);
    ASSERT_TRUE(processor->process(left.data(),right.data(),64,120));
    std::array<uint32_t,7> words{};ASSERT_TRUE(synth2::readPhysicalFmWords(*processor,0,words));
    EXPECT_EQ(words[2],4800u-64u);EXPECT_EQ(words[5],4800u);
}

TEST(PerformanceBehavior, KeyboardToggleRefreshesTheCurrentCoarseFrequency)
{
    const auto patch=hostvalidation::patch()+"9,1\n11,64\n12,127\n13,127\n";
    auto edited=hostvalidation::processor(),fixed=hostvalidation::processor();
    hostvalidation::load(*edited,patch);hostvalidation::load(*fixed,patch+"2,68\n4,0\n");
    std::array<float,64> left{},right{};
    for(auto* processor:{edited.get(),fixed.get()})
    {
        processor->prepare(48000,64);ASSERT_TRUE(processor->enqueueMidi(0,144,60,100));
        ASSERT_TRUE(processor->process(left.data(),right.data(),64,120));
    }
    edited->setNormalizedValue(2,68.0f/127.0f);
    for(int block=0;block<64;++block)ASSERT_TRUE(edited->process(left.data(),right.data(),64,120));
    edited->setNormalizedValue(4,0);
    ASSERT_TRUE(edited->process(left.data(),right.data(),64,120));
    std::array<uint32_t,9> actual{},expected{};
    ASSERT_TRUE(synth2::readPhysicalCoreWords(*edited,0,actual));
    ASSERT_TRUE(synth2::readPhysicalCoreWords(*fixed,0,expected));
    EXPECT_EQ(actual[3],expected[3]);
}
