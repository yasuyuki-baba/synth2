#include <gtest/gtest.h>
#include <Synth1PatchIo.h>
#include "patch/Synth1PatchMapper.h"
#include "patch/Synth1EnvelopeTime.h"
#include "patch/Synth1ControlPromotion.h"

TEST(PatchImportWorkspace, CompatibilityWordsPromoteExactlyIncludingOutOfHostRangeValues)
{
    synth1::patch::NativePatchControls compatible;
    for(int value:{-64,0,1,63,64,127,128,45057,65535})
    {
        compatible.values.fill(value);
        const auto before=compatible.values;
        const auto promoted=synth2::promoteCompatibleControls(compatible);
        for(float dial:promoted.dials)EXPECT_DOUBLE_EQ(dial,static_cast<double>(value));
        EXPECT_EQ(compatible.values,before);
    }
}

TEST(PatchImportWorkspace, PromotionFollowsVersionConversionAndDoesNotConvertTwice)
{
    for(int version:{100,104,105,106,107,108,109,110,111,112,113})
    {
        SCOPED_TRACE(version);
        const auto document=Synth1PatchDocument::makeParsed("Owned staged loading",version,
            {{16,64},{21,64},{52,64},{75,64},{86,45057}});
        const auto compatible=synth1::patch::resolveNativePatchControls(document);
        const auto promoted=synth2::promoteCompatibleControls(compatible);
        for(size_t i=0;i<compatible.values.size();++i)
            EXPECT_DOUBLE_EQ(promoted.dials[i],static_cast<double>(compatible.values[i]));
        // The resolved legacy decay is 62, not its serialized 64. Conversion
        // must precede promotion and must not run on the promoted 62 again.
        if(version==105 || version==106)EXPECT_EQ(promoted.dials[16],62);
    }
}

TEST(PatchImportWorkspace, ReusedWorkspaceMatchesOrdinaryImportForEveryAcceptedVersion)
{
    Synth1PatchImportWorkspace workspace;
    for(int version=1;version<=113;++version) for(int variant:{0,1,2})
    {
        SCOPED_TRACE(version);
        SCOPED_TRACE(variant);
        auto document=Synth1PatchDocument::makeParsed("Owned workspace test",version,{});
        if(variant==0) document.entries={{19,91},{29,73}};
        else for(int index=0;index<126;++index) document.entries.push_back({index,variant==1 ? 0 : 127});
        const auto ordinary=importSynth1Patch(document);
        const auto reused=importSynth1Patch(document,workspace);
        EXPECT_EQ(formatSynth1PatchText(ordinary),formatSynth1PatchText(reused));
    }
}

TEST(PatchImportWorkspace, ReusedNativeWorkspaceMatchesOrdinaryNativeDecode)
{
    Synth1PatchImportWorkspace workspace;
    for(int version:{100,104,105,106,107,108,109,110,111,112,113})
    {
        SCOPED_TRACE(version);
        auto document=Synth1PatchDocument::makeParsed("Owned native workspace",version,{{19,91},{29,73},{7,3},{86,45057}});
        const auto resolved=synth1::patch::resolveNativePatchControls(document);
        EXPECT_EQ(formatSynth1PatchText(decodeNativePatchControls(resolved,document)),
                  formatSynth1PatchText(decodeNativePatchControls(resolved,document,workspace)));
    }
}

TEST(PatchImportWorkspace, DirectSourceMappingKeepsTypedVersionConversionByDefault)
{
    for(int version=104;version<=113;++version)for(int dial:{0,1,31,64,95,126,127})
    {
        SCOPED_TRACE(::testing::Message()<<version<<'/'<<dial);
        auto document=Synth1PatchDocument::makeParsed("Generated conversion boundary",version,
            {{16,dial},{18,dial},{21,dial},{26,dial},{28,dial},{52,dial}});
        const auto imported=importSynth1Patch(document);
        std::array<float,128> source{};
        for(const auto& entry:document.entries)source[entry.index]=entry.rawValue/127.0f;
        synth2::Synth1PatchMapper mapper(version);synth2::RenderControls mapped;
        mapper.mapFromRawBuffer(source.data(),mapped);
        EXPECT_FLOAT_EQ(mapped.filterEnv.decaySec,synth2::envelopeDecayReleaseSeconds(*imported.filter.decay));
        EXPECT_FLOAT_EQ(mapped.filterEnv.releaseSec,synth2::envelopeDecayReleaseSeconds(*imported.filter.release));
        EXPECT_FLOAT_EQ(mapped.ampDecaySec,synth2::envelopeDecayReleaseSeconds(*imported.amplifier.decay));
        EXPECT_FLOAT_EQ(mapped.ampReleaseSec,synth2::envelopeDecayReleaseSeconds(*imported.amplifier.release));
        EXPECT_FLOAT_EQ(mapped.filterEnvAmount,(*imported.filter.envelopeAmount-63.0f)/64.0f);
        EXPECT_FLOAT_EQ(mapped.chorus.time,*imported.chorusFlanger.time/127.0f);
        EXPECT_EQ(mapper.getSourceValues(),source);
    }
}
