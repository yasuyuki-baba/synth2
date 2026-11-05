#include <array>
#include <gtest/gtest.h>
#include "host/ProcessorBridge.h"
#include "patch/PatchLoader.h"
#include "support/StateDocument.h"

using synth2::host::ProcessorBridge;

TEST(LoadingBehavior, EveryAcceptedVersionLoadsSelfAuthoredSourcesAndRecallsThem)
{
    for (int version=1; version<=113; ++version)
    {
        SCOPED_TRACE(version);
        const auto text = "Version contract\nver=" + std::to_string(version) + "\n19,91\n29,73\n";
        ProcessorBridge source, restored;
        std::string error;
        ASSERT_TRUE(source.loadProgram(text, 7, error)) << error;
        const auto saved = source.saveState();
        restored.restoreState(saved.data(), static_cast<int>(saved.size()));
        EXPECT_EQ(restored.saveState(), saved);
        EXPECT_EQ(restored.currentProgram(), 7);
        EXPECT_EQ(restored.mappedVersion(), source.mappedVersion());
        EXPECT_NEAR(stateinspection::raw(source, 126), version, 1e-4);
        EXPECT_NEAR(stateinspection::raw(restored, 126), version, 1e-4);
        EXPECT_EQ(stateinspection::raw(restored, 19), 91);
        EXPECT_EQ(stateinspection::raw(restored, 29), 73);
    }
}

TEST(LoadingBehavior, NamedUnversionedAndCommentedSourcesRetainSubmittedFields)
{
    const std::string text = "# Header\r\nOwned patch\r\n 19,91 ; sustain\r\n29,73 # output\r\n";
    const auto document = synth2::parsePatchDocument(text);
    EXPECT_EQ(document.name, "Owned patch");
    EXPECT_FALSE(document.version().has_value());
    ASSERT_EQ(document.entries.size(), 2u);
    EXPECT_EQ(document.entries[0].index, 19);
    EXPECT_EQ(document.entries[0].rawValue, 91);
    ProcessorBridge source;
    std::string error;
    ASSERT_TRUE(source.loadPatch(text, error)) << error;
    EXPECT_EQ(stateinspection::raw(source, 19), 91);
}

TEST(LoadingBehavior, CommasInTitlesRemainNamesWithOrWithoutVersionHeaders)
{
    for(const std::string title:{"Owned lead (wheel,aftertouch)","--BRASS, lead"})
    for(const std::string header:{"", "color=blue\r\nver=112\r\n"})
    {
        const std::string text=title+"\r\n"+header+"19,91\r\n29,73\r\n";
        EXPECT_TRUE(synth2::hasPatchTitle(text));
        const auto document=synth2::parsePatchDocument(text);
        EXPECT_EQ(document.name,title);
        ProcessorBridge source,recalled;std::string error;
        ASSERT_TRUE(source.loadPatch(text,error))<<error;
        EXPECT_EQ(stateinspection::raw(source,126),header.empty() ? 100 : 112);
        EXPECT_EQ(stateinspection::raw(source,19),91);
        const auto state=source.saveState();
        recalled.restoreState(state.data(),static_cast<int>(state.size()));
        EXPECT_EQ(recalled.saveState(),state);
    }
    for(const std::string text:{"19,91\n29,73\n","19x,91\n29,73\n",",91\n29,73\n"})
        EXPECT_FALSE(synth2::hasPatchTitle(text));
    ProcessorBridge source;std::string error;
    ASSERT_TRUE(source.loadPatch("19,91\n29,73\n",error))<<error;
    const auto before=source.saveState();
    for(const std::string text:{"19x,91\n29,73\n",",91\n29,73\n"})
    {EXPECT_FALSE(source.loadPatch(text,error));EXPECT_EQ(source.saveState(),before);}
}

TEST(LoadingBehavior, ExtendedCoarseWordsRetainNativeControlsAndStateWithoutChangingHostRanges)
{
    for(int version:{100,104,105,106,107,108,109,110,111,112,113})
    for(int coarse:{-1,127,135,255})
    {
        SCOPED_TRACE(std::to_string(version)+"/"+std::to_string(coarse));
        const auto text="Owned extended coarse\nver="+std::to_string(version)+"\n2,"+std::to_string(coarse)+"\n19,91\n29,73\n";
        ProcessorBridge source,recalled;std::string error;
        ASSERT_TRUE(source.loadProgram(text,22000,error))<<error;
        EXPECT_EQ(source.parameter(2).minimum,0);EXPECT_EQ(source.parameter(2).maximum,127);
        source.prepare(44100,64);
        std::array<float,64> l{},r{};ASSERT_TRUE(source.process(l.data(),r.data(),64,120));
        std::array<int,99> controls{};ASSERT_TRUE(source.readNativeControls(controls));
        EXPECT_EQ(controls[2],coarse);
        const auto state=source.saveState();recalled.restoreState(state.data(),static_cast<int>(state.size()));
        EXPECT_EQ(recalled.saveState(),state);EXPECT_EQ(recalled.currentProgram(),22000);
        recalled.prepare(44100,64);ASSERT_TRUE(recalled.process(l.data(),r.data(),64,120));
        ASSERT_TRUE(recalled.readNativeControls(controls));EXPECT_EQ(controls[2],coarse);
    }
}

TEST(LoadingBehavior, UnsupportedVersionsAndMalformedTokensPreserveSelectedState)
{
    ProcessorBridge source;
    std::string error;
    ASSERT_TRUE(source.loadProgram("Valid\nver=113\n19,91\n29,73\n", 7, error));
    const auto before = source.saveState();
    for(int coarse:{-2,256})
    {
        EXPECT_FALSE(source.loadProgram("Outside coarse domain\nver=106\n2,"+std::to_string(coarse)+"\n",3,error));
        EXPECT_EQ(source.saveState(),before);
    }
    for (int version : {-1,0,114,999})
    {
        SCOPED_TRACE(version);
        EXPECT_FALSE(source.loadProgram("Invalid version\nver="+std::to_string(version)+"\n19,1\n",3,error));
        EXPECT_EQ(source.saveState(),before);
        EXPECT_EQ(source.currentProgram(),7);
    }
    for (const auto& body : {"ver=113\nver=112\n19,1",
                            "ver=nan\n19,1", "ver=113\n19,1.5", "ver=113\n19,",
                            "ver=113\n19,999999999999999999999", "ver=113\n19,128"})
    {
        SCOPED_TRACE(body);
        EXPECT_FALSE(source.loadProgram(std::string("Invalid\n") + body + "\n", 3, error));
        EXPECT_FALSE(error.empty());
        EXPECT_EQ(source.saveState(), before);
        EXPECT_EQ(source.currentProgram(), 7);
    }
}

// Host metadata is a public ABI: these expectations are authored here rather
// than obtained from the production range/default helpers or recorded fixtures.
TEST(ParameterContract, AllHostIdsRangesAndDeclaredDefaultsAreStable)
{
    ProcessorBridge source;
    ASSERT_EQ(ProcessorBridge::parameterCount, 129);
    std::array<double,128> defaults{};
    for (int i : {2,3,8,11,21,34,52,60,61,62,63,72,84,90,98}) defaults[i] = 64;
    for (int i : {17,19,27}) defaults[i] = 127;
    defaults[1]=1; defaults[4]=1; defaults[29]=100; defaults[31]=2;
    defaults[33]=11; defaults[75]=22; defaults[83]=66; defaults[85]=24;
    defaults[93]=2; defaults[94]=16; defaults[126]=105;
    for (int i=0; i<129; ++i)
    {
        SCOPED_TRACE(i);
        const auto& parameter = source.parameter(i);
        const double low = i==9 ? -24 : (i==86 || i==88) ? -1 : 0;
        const double high = i==128 ? 1 : i==9 ? 24 : (i==86 || i==88) ? 65535 : 127;
        const double initial = i==128 ? 1 : defaults[i];
        EXPECT_EQ(parameter.id, i==128 ? "masterVolume" : "p"+std::to_string(i));
        EXPECT_EQ(parameter.minimum, low);
        EXPECT_EQ(parameter.maximum, high);
        EXPECT_NEAR(parameter.defaultNormalized, (initial-low)/(high-low), 1e-7);
        EXPECT_FALSE(parameter.name.empty());
    }
}

TEST(ProcessorState, PatchDisplayNameSurvivesRecallWithoutLibrary)
{
    ProcessorBridge source, restored;
    std::string error;
    const std::string name = "Warm <Pad> & \"Air\" 音色";
    ASSERT_TRUE(source.loadProgram("ver=105\n19,91\n", 4, error, name));
    source.setNormalizedValue(29, 0.5);
    const auto saved = source.saveState();
    restored.restoreState(saved.data(), static_cast<int>(saved.size()));
    EXPECT_EQ(restored.currentPatchName(), name);
    EXPECT_EQ(restored.saveState(), saved);
    EXPECT_FALSE(restored.loadPatch("Broken\nver=999\n", error));
    EXPECT_EQ(restored.currentPatchName(), name);
    auto invalid = stateinspection::document(source);
    invalid->RootElement()->SetAttribute("stateVersion", "999");
    invalid->RootElement()->SetAttribute("patchName", "Rejected name");
    const auto rejected = stateinspection::bytes(*invalid);
    restored.restoreState(rejected.data(), static_cast<int>(rejected.size()));
    EXPECT_EQ(restored.currentPatchName(), name);
}

TEST(ProcessorState, LegacyRecallDerivesNameFromEmbeddedSource)
{
    ProcessorBridge source, restored;
    std::string error;
    ASSERT_TRUE(source.loadPatch("# Comment\r\nOriginal Pad\r\nver=113\r\n19,91\r\n", error));
    auto legacy = stateinspection::document(source);
    legacy->RootElement()->DeleteAttribute("patchName");
    const auto bytes = stateinspection::bytes(*legacy);
    restored.restoreState(bytes.data(), static_cast<int>(bytes.size()));
    EXPECT_EQ(restored.currentPatchName(), "Original Pad");
    ASSERT_TRUE(restored.loadPatch("ver=105\n19,91\n", error));
    EXPECT_EQ(restored.currentPatchName(), "Untitled");
    ProcessorBridge init;
    EXPECT_EQ(init.currentPatchName(), "Init");
}
