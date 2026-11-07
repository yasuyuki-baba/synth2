#include <gtest/gtest.h>
#include "host/ProcessorBridge.h"
#include "patch/PatchFormat.h"
#include "patch/Synth1EnvelopeTime.h"
#include "../tools/host_scenarios.h"
#include "support/StateDocument.h"
#include <array>
#include <bit>
#include <limits>
#include <locale>
#include <utility>

namespace
{
struct CommaDecimal : std::numpunct<char>
{
    char do_decimal_point() const override {return ',';}
};
struct ScopedCommaLocale
{
    std::locale previous=std::locale();
    ScopedCommaLocale(){std::locale::global(std::locale(previous,new CommaDecimal));}
    ~ScopedCommaLocale(){std::locale::global(previous);}
};
using synth2::host::ProcessorBridge;
using Stereo=std::array<std::array<float,64>,2>;
Stereo render(ProcessorBridge& p,int block)
{
    if(block==1)EXPECT_TRUE(p.enqueueMidi(13,0x90,60,100));
    if(block==400)EXPECT_TRUE(p.enqueueMidi(29,0x80,60,0));
    Stereo out{};EXPECT_TRUE(p.process(out[0].data(),out[1].data(),64,120));return out;
}
}

TEST(PatchFormat, Version200PreservesEveryFloatAndEscapedNameExactly)
{
    ProcessorBridge processor;auto patch=synth2::parseContinuousPatch(processor.savePatch());
    patch.name=R"(Owned #1; "quoted" \ patch, 日本語)";
    for(int i=0;i<129;++i)if(i!=126)
    {
        const auto& info=processor.parameter(i);
        patch.values[i]=static_cast<float>(info.minimum+(info.maximum-info.minimum)*0.371234567);
    }
    const auto text=synth2::formatContinuousPatch(patch);
    EXPECT_TRUE(text.starts_with("ver=200\n"));
    const auto decoded=synth2::parseContinuousPatch(text);
    EXPECT_EQ(decoded.name,patch.name);
    for(int i=0;i<129;++i)if(i!=126)
        EXPECT_EQ(std::bit_cast<uint32_t>(decoded.values[i]),std::bit_cast<uint32_t>(patch.values[i]))<<i;
    std::string error;ASSERT_TRUE(processor.loadPatch(text,error))<<error;
    EXPECT_EQ(processor.savePatch(),text);
    EXPECT_EQ(processor.currentPatchName(),patch.name);
    processor.prepare(44100,64);render(processor,0);
    EXPECT_EQ(processor.mappedVersion(),200);
    std::array<int,99> controls;EXPECT_FALSE(processor.readNativeControls(controls));
}

TEST(PatchFormat, DecimalParsingIsLocaleIndependentAndRejectsInvalidRanges)
{
    const ScopedCommaLocale locale;
    ProcessorBridge processor;
    const auto original=processor.savePatch();
    const auto entry=original.find("\n19,")+1;
    ASSERT_NE(entry,0u);
    const auto end=original.find('\n',entry);
    const auto withValue=[&](const std::string& token)
    {
        auto text=original;text.replace(entry,end-entry,"19,"+token);return text;
    };
    for(const auto& [token,expected]:std::array<std::pair<const char*,float>,6>{{
        {".5",.5f},{"5.",5.f},{"1.25e+1",12.5f},{"1.25E-1",.125f},
        {"1.40129846e-45",std::numeric_limits<float>::denorm_min()},{"-0",-0.f}}})
    {
        const auto patch=synth2::parseContinuousPatch(withValue(token));
        EXPECT_EQ(std::bit_cast<uint32_t>(patch.values[19]),std::bit_cast<uint32_t>(expected))<<token;
    }
    const auto before=processor.saveState();
    for(const auto* token:{"+1","1e","1e+","1.2.3","0x1p0","1 2","nan","inf","1e1000","1e-1000"})
    {
        std::string error;
        EXPECT_FALSE(processor.loadPatch(withValue(token),error))<<token;
        EXPECT_EQ(processor.saveState(),before)<<token;
    }
}

TEST(PatchFormat, FractionalDialsBypassLegacyConversionAndSurviveStateRecall)
{
    ProcessorBridge source;auto patch=synth2::parseContinuousPatch(source.savePatch());
    patch.values[16]=64.25f;patch.values[19]=37.25f;patch.values[52]=0.125f;
    std::string error;ASSERT_TRUE(source.loadPatch(synth2::formatContinuousPatch(patch),error))<<error;
    const auto state=source.saveState();
    EXPECT_EQ(stateinspection::attribute(*stateinspection::document(state),"stateVersion"),"3");
    EXPECT_EQ(stateinspection::attribute(*stateinspection::document(state),"patchFormat"),"200");
    ProcessorBridge restored;restored.restoreState(state.data(),static_cast<int>(state.size()));
    EXPECT_EQ(restored.saveState(),state);EXPECT_EQ(restored.savePatch(),source.savePatch());
    source.prepare(44100,64);restored.prepare(44100,64);
    for(int i=0;i<700;++i)EXPECT_EQ(render(source,i),render(restored,i))<<i;
    synth2::RenderControls mapped;ASSERT_TRUE(synth2::readMappedSynthParams(source,mapped));
    EXPECT_FLOAT_EQ(mapped.cutoffIndex,(37.25f/127.0f)*1023.0f);
    EXPECT_FLOAT_EQ(mapped.chorus.time,0.125f/127.0f);
    EXPECT_FLOAT_EQ(mapped.filterEnv.decaySec,0.01f*std::pow(6000.f,64.25f/127.f));
}

TEST(PatchFormat, ImportedNativeFilesExportResolvedDialsWithoutDoubleConversion)
{
    for(int version:{100,104,105,106,107,108,109,110,111,112,113})
    {
        SCOPED_TRACE(version);ProcessorBridge source,restored;std::string error;
        ASSERT_TRUE(source.loadPatch("Owned converted patch\nver="+std::to_string(version)+
            "\n19,91\n29,73\n16,64\n52,0\n75,52\n84,31\n",error))<<error;
        source.setNormalizedValue(19,37.25/127.0);
        const auto text=source.savePatch();
        ASSERT_TRUE(restored.loadPatch(text,error))<<error;
        EXPECT_EQ(restored.savePatch(),text);
        source.prepare(44100,64);restored.prepare(44100,64);
        for(int i=0;i<700;++i)EXPECT_EQ(render(source,i),render(restored,i))<<i;
    }
}

TEST(PatchFormat, InvalidContinuousFilesPreserveProgramAndState)
{
    ProcessorBridge p;std::string error;ASSERT_TRUE(p.loadProgram(p.savePatch(),7,error));
    const auto text=p.savePatch();const auto before=p.saveState();
    for(const auto& suffix:{"19,nan\n","19,inf\n","19,1e1000\n","19,1junk\n","19,128\n",
                           "-1,0\n","129,0\n","126,113\n","19,1\n","ver=200\n"})
    {
        EXPECT_FALSE(p.loadProgram(text+suffix,3,error))<<suffix;
        EXPECT_EQ(p.saveState(),before);EXPECT_EQ(p.currentProgram(),7);
    }
    for(const auto& bad:{"ver=200\n19,37.25\n","ver=199\n19,37\n","ver=201\n19,37\n","ver=113\n19,37.25\n"})
    {EXPECT_FALSE(p.loadPatch(bad,error))<<bad;EXPECT_EQ(p.saveState(),before);}
    auto bad=stateinspection::document(before);bad->RootElement()->SetAttribute("patchFormat","201");
    const auto bytes=stateinspection::bytes(*bad);p.restoreState(bytes.data(),static_cast<int>(bytes.size()));
    EXPECT_EQ(p.saveState(),before);
}

TEST(PatchFormat, ContinuousFileEnablesFractionalModeAfterLegacyStateRestore)
{
    ProcessorBridge p;auto legacy=stateinspection::document(p);legacy->RootElement()->SetAttribute("stateVersion","2");
    legacy->RootElement()->DeleteAttribute("nativeSource");
    const auto bytes=stateinspection::bytes(*legacy);p.restoreState(bytes.data(),static_cast<int>(bytes.size()));
    ASSERT_EQ(stateinspection::attribute(*stateinspection::document(p),"stateVersion"),"2");
    auto patch=synth2::parseContinuousPatch(p.savePatch());patch.values[19]=37.25f;
    std::string error;ASSERT_TRUE(p.loadPatch(synth2::formatContinuousPatch(patch),error))<<error;
    EXPECT_EQ(stateinspection::attribute(*stateinspection::document(p),"stateVersion"),"3");
    p.prepare(44100,64);render(p,0);synth2::RenderControls mapped;
    ASSERT_TRUE(synth2::readMappedSynthParams(p,mapped));
    EXPECT_FLOAT_EQ(mapped.cutoffIndex,(37.25f/127.f)*1023.f);
}

TEST(PatchFormat, MetadataSurvivesEditsExportAndProjectRecall)
{
    ProcessorBridge source;std::string error;
    const std::string legacy="Owned metadata patch\nver=113\ncolor=cyan\nfuture=opaque # literal; value\nfuture=second\n19,64\n";
    ASSERT_TRUE(source.loadPatch(legacy,error))<<error;
    const auto expected=synth2::readPatchMetadata(legacy);
    source.setNormalizedValue(19,37.25/127.0);
    auto exported=synth2::parseContinuousPatch(source.savePatch());
    EXPECT_EQ(exported.metadata,expected);
    EXPECT_EQ(synth2::patchColor(exported.metadata),0x00ffffu);
    ProcessorBridge restored;
    const auto state=source.saveState();
    restored.restoreState(state.data(),static_cast<int>(state.size()));
    EXPECT_EQ(restored.savePatch(),source.savePatch());
    exported.name="Renamed # patch";
    ASSERT_TRUE(restored.loadPatch(synth2::formatContinuousPatch(exported),error))<<error;
    EXPECT_EQ(synth2::parseContinuousPatch(restored.savePatch()).metadata,expected);
    EXPECT_EQ(synth2::parseContinuousPatch(restored.savePatch()).name,exported.name);
    ProcessorBridge plain;
    ASSERT_TRUE(restored.loadPatch(plain.savePatch(),error))<<error;
    EXPECT_TRUE(synth2::parseContinuousPatch(restored.savePatch()).metadata.empty());
}

TEST(PatchFormat, NamedColorsAndOpaqueUnknownValues)
{
    const std::vector<std::string> names{"red","green","blue","yellow","magenta","cyan"};
    const unsigned expected[]{0xff0000,0x00ff00,0x0000ff,0xffff00,0xff00ff,0x00ffff};
    for(std::size_t i=0;i<names.size();++i)
        EXPECT_EQ(synth2::patchColor({"color="+names[i]}),expected[i]);
    for(const auto* value:{"default","unknown","16711680",""})
        EXPECT_FALSE(synth2::patchColor({std::string("color=")+value}));
    ProcessorBridge processor;
    auto patch=synth2::parseContinuousPatch(processor.savePatch());
    patch.metadata={"color=unknown","custom=value","custom=another"};
    EXPECT_EQ(synth2::parseContinuousPatch(synth2::formatContinuousPatch(patch)).metadata,patch.metadata);
    for(const auto* line:{"ver=113","name=duplicate","color=red\n19,1"})
    {
        patch.metadata={line};
        EXPECT_THROW(synth2::formatContinuousPatch(patch),std::runtime_error);
    }
}

TEST(PatchFormat, HeadersDoNotChangeLoadedControlsOrAudio)
{
    ProcessorBridge plain,colored;std::string error;
    const std::string body="ver=113\n19,64\n29,80\n";
    ASSERT_TRUE(plain.loadPatch("Owned\n"+body,error))<<error;
    ASSERT_TRUE(colored.loadPatch("Owned\n"+body+"color=green\nfuture=opaque\n",error))<<error;
    EXPECT_EQ(synth2::parseContinuousPatch(plain.savePatch()).values,
              synth2::parseContinuousPatch(colored.savePatch()).values);
    plain.prepare(44100,64);colored.prepare(44100,64);
    for(int block=0;block<450;++block)EXPECT_EQ(render(plain,block),render(colored,block));
}
