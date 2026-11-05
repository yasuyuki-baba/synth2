#include <gtest/gtest.h>
#include "host/PatchLibrary.h"
#include "support/PortableHost.h"
#include "support/StateDocument.h"
#include "patch/PatchLoader.h"
#include <array>
#include <memory>
#include <limits>
#include <vector>

namespace {
using synth2::host::ProcessorBridge;
auto processor(){return std::make_unique<ProcessorBridge>();}
int index(const std::string& id){return id=="masterVolume" ? 128 : std::stoi(id.substr(1));}
float normalized(ProcessorBridge& p,const std::string& id){return p.normalizedValue(index(id));}
float raw(ProcessorBridge& p,const std::string& id){return stateinspection::raw(p,index(id));}
void edit(ProcessorBridge& p,const std::string& id,float value){p.setNormalizedValue(index(id),value);}
auto save(ProcessorBridge& p){return p.saveState();}
void restore(ProcessorBridge& p,const std::vector<uint8_t>& bytes){p.restoreState(bytes.data(),static_cast<int>(bytes.size()));}
}

TEST(ProcessorState, RoundTripRestoresEveryParameterInANewInstance)
{
    auto source = processor();
    std::string error;
    ASSERT_TRUE(loadPortablePatch (*source, "State test\nver=105\n9,-12\n14,2\n19,44\n29,99\n", error));
    edit(*source, "masterVolume",0.37f);
    const auto bytes = save (*source);
    ASSERT_GT(bytes.size(), 0u);
    auto target = processor();
    restore (*target, bytes);
    for(int i=0;i<ProcessorBridge::parameterCount;++i)
        EXPECT_FLOAT_EQ(source->normalizedValue(i),target->normalizedValue(i))<<i;
    EXPECT_FLOAT_EQ(raw(*target, "p9"), -12.0f);
    EXPECT_GT(ProcessorBridge::tailLengthSeconds, 0.0);
}

TEST(ProcessorState, MalformedUnknownAndIncompleteStatesAreAtomicNoOps)
{
    auto p = processor();
    edit(*p, "masterVolume",0.37f);
    const auto before = save (*p);
    p->restoreState (nullptr, 0);
    const char invalid[] = "invalid state";
    p->restoreState (invalid, sizeof(invalid));
    EXPECT_EQ(before, save (*p));
    for (int mutation = 0; mutation < 7; ++mutation)
    {
        auto xml = stateinspection::document(before);
        ASSERT_NE(xml, nullptr);
        if (mutation == 0) xml->RootElement()->SetName("OTHER");
        if (mutation == 1) xml->RootElement()->SetAttribute ("stateVersion", 99);
        if (mutation == 2) xml->RootElement()->DeleteChild(xml->RootElement()->FirstChildElement());
        if (mutation == 3) xml->RootElement()->FirstChildElement()->SetAttribute ("value", "nan");
        if (mutation == 4) xml->RootElement()->FirstChildElement()->SetAttribute ("value", "9999");
        if (mutation == 5) xml->RootElement()->FirstChildElement()->SetAttribute ("value", "invalid");
        if (mutation == 6) xml->RootElement()->InsertEndChild(xml->RootElement()->FirstChildElement()->DeepClone(xml.get()));
        std::vector<uint8_t> bytes;
        bytes=stateinspection::bytes(*xml);
        restore (*p, bytes);
        EXPECT_EQ(before, save (*p)) << mutation;
    }
}

TEST(ProcessorState, RestoredStateProducesTheSameAudioAndRemainsEditable)
{
    auto source = processor();
    std::string error;
    ASSERT_TRUE(loadPortablePatch (*source, "Audible state\nver=105\n0,0\n5,0\n9,-12\n17,127\n19,127\n27,127\n29,100\n", error));
    edit(*source, "masterVolume",0.4f);
    const auto bytes = save (*source);
    auto target = processor();
    restore (*target, bytes);
    source->prepare (48000.0, 64);
    target->prepare (48000.0, 64);
    float peak = 0.0f;
    for (int block = 0; block < 20; ++block)
    {
        synth2::audio::AudioBuffer<float> a (2, 64), b (2, 64);
        a.clear(); b.clear();
        synth2::audio::MidiBuffer midiA, midiB;
        if (block == 0)
        {
            midiA.addEvent (synth2::audio::MidiMessage::noteOn (1, 69, 1.0f), 0);
            midiB = midiA;
        }
        processPortableBlock(*source,a,midiA);
        processPortableBlock(*target,b,midiB);
        peak = std::max (peak, a.getMagnitude (0, 64));
        for (int ch = 0; ch < 2; ++ch)
        for (int frame = 0; frame < 64; ++frame)
            EXPECT_FLOAT_EQ(a.getSample(ch, frame), b.getSample(ch, frame));
    }
    EXPECT_GT(peak, 0.001f);
    edit(*target, "masterVolume",0.0f);
    EXPECT_NE(bytes, save (*target));
    source->release(); target->release();
}

TEST(ProcessorState, ProgramLoadsAndRejectedTransactionsPreserveTheSelectedState)
{
    const std::array<std::string,2> programs{{"Program A\nver=105\n19,22\n","Program B\nver=105\n19,91\n"}};
    auto p=processor();std::string error;
    ASSERT_TRUE(p->loadProgram(programs[1],1,error));
    EXPECT_EQ(p->currentProgram(),1);EXPECT_FLOAT_EQ(raw(*p,"p19"),91.f);
    const auto bytes=save(*p);
    ASSERT_TRUE(p->loadProgram(programs[0],0,error));EXPECT_FLOAT_EQ(raw(*p,"p19"),22.f);restore(*p,bytes);
    EXPECT_EQ(p->currentProgram(),1);EXPECT_FLOAT_EQ(raw(*p,"p19"),91.f);
    // Processor transactions retain the sound and index after invalid input.
    EXPECT_FALSE(p->loadProgram("Invalid\nver=999\n",0,error));EXPECT_EQ(bytes,save(*p));
    EXPECT_FALSE(p->loadProgram(programs[0],-1,error));EXPECT_EQ(bytes,save(*p));
    EXPECT_FALSE(p->loadProgram(programs[0],static_cast<int>(synth2::host::PatchLibrary::maximumPatches),error));EXPECT_EQ(bytes,save(*p));
}

TEST(HostTempo, TempoChangesReachSyncedLfoAndInvalidTempoUsesFallback)
{
    auto render = [](double bpm)
    {
        auto p = processor();
        std::string error;
        EXPECT_TRUE(loadPortablePatch (*p, "Tempo\nver=105\n0,0\n17,127\n19,127\n27,127\n29,100\n41,4\n42,5\n43,70\n44,127\n57,1\n67,1\n68,1\n", error));
        p->prepare (48000.0, 64);
        std::vector<float> samples;
        for (int block = 0; block < 200; ++block)
        {
            synth2::audio::AudioBuffer<float> buffer (2, 64); buffer.clear();
            synth2::audio::MidiBuffer midi;
            if (block == 0) midi.addEvent (synth2::audio::MidiMessage::noteOn (1, 69, 1.0f), 0);
            processPortableBlock(*p,buffer,midi,bpm);
            samples.insert (samples.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + 64);
        }
        p->release();
        return samples;
    };
    const auto normal = render (120.0);
    EXPECT_EQ(normal, render (std::numeric_limits<double>::quiet_NaN()));
    EXPECT_EQ(normal, render (0.0));
    const auto slow = render (60.0);
    double energy = 0.0;
    for (size_t i = 0; i < slow.size(); ++i) energy += std::pow (slow[i] - normal[i], 2);
    EXPECT_GT(energy, 0.01);
}

TEST(ProcessorState, MissingEnvelopePresenceSurvivesRestoreAndSameValueEdits)
{
    auto p = processor();
    std::string error;
    ASSERT_TRUE(loadPortablePatch (*p, "Sparse\nver=113\n26,0\n", error));
    auto presence = [] (ProcessorBridge& instance)
    {
        const auto bytes = save (instance);
        auto xml = stateinspection::document(bytes);
        return xml->RootElement()->IntAttribute ("missingEnvelopeFields", -1);
    };
    EXPECT_EQ(presence(*p), 47); // p26 is explicitly zero; the other five are absent.
    EXPECT_FLOAT_EQ(normalized(*p, "p26"), 0.0f);
    EXPECT_FLOAT_EQ(normalized(*p, "p16") * 127, 64.0f);
    EXPECT_FLOAT_EQ(normalized(*p, "p25") * 127, 64.0f);
    EXPECT_FLOAT_EQ(normalized(*p, "p27") * 127, 107.0f);
    EXPECT_FLOAT_EQ(normalized(*p, "p29") * 127, 107.0f);
    EXPECT_FLOAT_EQ(normalized(*p, "p30") * 127, 64.0f);

    EXPECT_FLOAT_EQ(p->parameter(21).maximum, 127.0f);
    EXPECT_FLOAT_EQ(p->normalizedValue(21), 1.0f);
    auto restored = processor();
    restore (*restored, save(*p));
    EXPECT_EQ(presence(*restored), 47);
    // Even setting the same visible 127 explicitly changes source presence.
    edit(*restored, "p21",1.0f);
    EXPECT_EQ(presence(*restored), 39);
    edit(*restored, "p16",0.0f);
    EXPECT_EQ(presence(*restored), 37);
    restore (*p, save(*restored));
    EXPECT_EQ(presence(*p), 37);
    const auto edited = save(*p);
    ASSERT_FALSE(loadPortablePatch (*p, "Invalid\nver=113\n26,128\n", error));
    EXPECT_EQ(save(*p), edited);

    auto xml = stateinspection::document(edited);
    for (const auto& invalid : {"64", "-1", "nan", "1.0", "", "999999999999999999"})
    {
        xml->RootElement()->SetAttribute ("missingEnvelopeFields", invalid);
        std::vector<uint8_t> bytes;
        bytes=stateinspection::bytes(*xml);
        restore (*p, bytes);
        EXPECT_EQ(save(*p), edited) << invalid;
    }
    // Old version-1 sessions retain their materialized parameter values.
    xml->RootElement()->DeleteAttribute ("missingEnvelopeFields");
    xml->RootElement()->DeleteAttribute ("sourcePresence");
    xml->RootElement()->SetAttribute ("stateVersion",1);
    std::vector<uint8_t> old;
    old=stateinspection::bytes(*xml);
    restore (*p, old);
    EXPECT_EQ(presence(*p), 0);
}

TEST(ProcessorState, SparseEnvelopeDefaultsRenderLikeExplicitSourceDefaults)
{
    for (int version : {104,105,106,113})
    {
        const auto header = "Defaults\nver=" + std::to_string(version)
            + "\n0,0\n5,0\n13,0\n21,63\n17,20\n19,95\n27,20\n29,100\n";
        auto sparse = processor(), full = processor(), restored = processor();
        std::string error;
        ASSERT_TRUE(loadPortablePatch (*sparse, header, error));
        const int decay=64;
        ASSERT_TRUE(loadPortablePatch (*full, header + "16,"+std::to_string(decay)
            +"\n18,"+std::to_string(decay)+"\n26,"+std::to_string(decay)
            +"\n28,"+std::to_string(decay)+"\n", error));
        restore (*restored, save(*sparse));
        for (auto* instance : {sparse.get(),full.get(),restored.get()}) instance->prepare (48000,64);
        float peak = 0;
        for (int block = 0; block < 120; ++block)
        {
            synth2::audio::AudioBuffer<float> a(2,64), b(2,64), c(2,64);
            synth2::audio::MidiBuffer ma,mb,mc;
            if (block == 0) ma.addEvent (synth2::audio::MidiMessage::noteOn(1,60,1.0f),0);
            if (block == 60) ma.addEvent (synth2::audio::MidiMessage::noteOff(1,60),0);
            mb = ma; mc = ma;
            processPortableBlock(*sparse,a,ma); processPortableBlock(*full,b,mb); processPortableBlock(*restored,c,mc);
            peak = std::max(peak,a.getMagnitude(0,64));
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 64; ++i)
            {
                ASSERT_FLOAT_EQ(a.getSample(ch,i),b.getSample(ch,i)) << version << "/" << block;
                ASSERT_FLOAT_EQ(a.getSample(ch,i),c.getSample(ch,i)) << version << "/" << block;
            }
        }
        EXPECT_GT(peak,0.001f);
        for (auto* instance : {sparse.get(),full.get(),restored.get()}) instance->release();
    }
}

TEST(ProcessorState, PackedMidiSourcesLoadAndSurviveStateRestore)
{
    auto source=processor();std::string error;
    ASSERT_TRUE(loadPortablePatch(*source,"Packed MIDI\nver=112\n9,-1\n86,45057\n88,53248\n",error))<<error;
    auto target=processor();restore(*target,save(*source));
    for(auto* p:{source.get(),target.get()})
    {
        auto raw=[&](const char* id){return stateinspection::raw(*p,index(id));};
        EXPECT_FLOAT_EQ(raw("p86"),45057.0f);EXPECT_FLOAT_EQ(raw("p88"),53248.0f);EXPECT_FLOAT_EQ(raw("p9"),-1.0f);
    }
    ASSERT_TRUE(loadPortablePatch(*source,"Pitch bend source\nver=113\n86,57344\n88,45057\n",error))<<error;
    EXPECT_FLOAT_EQ(raw(*source,"p86"),57344.0f);
    ASSERT_TRUE(loadPortablePatch(*source,"No source\nver=113\n86,-1\n88,-1\n",error))<<error;
    EXPECT_FLOAT_EQ(raw(*source,"p86"),-1.0f);
}

TEST(ProcessorState, InvalidPackedSourcesAreAtomicAndDoNotRelaxOtherFields)
{
    auto p=processor();std::string error;
    ASSERT_TRUE(loadPortablePatch(*p,"Valid MIDI\nver=112\n86,45057\n88,57344\n",error));
    const auto before=save(*p);
    for(const char* entry:{"86,-2","86,65536","88,-2","88,65536","87,65536","29,128"})
    {
        EXPECT_FALSE(loadPortablePatch(*p,std::string("Invalid MIDI\nver=112\n")+entry+"\n",error));
        EXPECT_EQ(save(*p),before)<<entry;
    }
}
