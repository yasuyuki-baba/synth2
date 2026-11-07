#include <gtest/gtest.h>
#include "support/GeneratedPatches.h"
#include "host/PatchLibrary.h"
#include "host/ProcessorBridge.h"
#include "editor/EditorPages.h"
#include <miniz.h>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <cstring>
using synth2::host::PatchLibrary;
namespace {
struct TemporaryFile {
    std::filesystem::path path;
    explicit TemporaryFile(const char* extension){static std::atomic<unsigned> next{};path=std::filesystem::temp_directory_path()/("synth2-bank-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(next++)+extension);}
    ~TemporaryFile(){std::error_code error;std::filesystem::remove(path,error);}
    bool write(const std::string& data){std::ofstream out(path,std::ios::binary);out.write(data.data(),data.size());return static_cast<bool>(out);}
};
bool writeZip(const std::filesystem::path& file,const std::vector<std::pair<std::string,std::string>>& entries){mz_zip_archive zip{};if(!mz_zip_writer_init_file(&zip,file.string().c_str(),0))return false;bool ok=true;for(const auto& [name,text]:entries)ok=ok && mz_zip_writer_add_mem(&zip,name.c_str(),text.data(),text.size(),MZ_BEST_COMPRESSION);ok=ok && mz_zip_writer_finalize_archive(&zip);mz_zip_writer_end(&zip);return ok;}
const std::string ownedPatch="Host validation\nver=113\n0,0\n19,127\n29,80\n";
}
TEST(PatchLibrary, ImportedBankRetainsSourceBytesAndLoadedStates) {
    TemporaryFile input(".zip");ASSERT_TRUE(writeZip(input.path,testfiles::generatedBank()));
    PatchLibrary library;std::string error;ASSERT_TRUE(library.openFile(input.path.string(),error))<<error;ASSERT_EQ(library.patches().size(),128u);
    mz_zip_archive zip{};const auto path=input.path.string();ASSERT_TRUE(mz_zip_reader_init_file(&zip,path.c_str(),0));
    for(const auto& patch:library.patches()) {
        const auto filename=patch.name.substr(0,patch.name.find("  -  "));size_t size=0;void* data=mz_zip_reader_extract_file_to_heap(&zip,filename.c_str(),&size,0);ASSERT_NE(data,nullptr);EXPECT_EQ(patch.text,std::string(static_cast<const char*>(data),size));mz_free(data);
        synth2::host::ProcessorBridge direct,restored;ASSERT_TRUE(direct.loadPatch(patch.text,error));const auto state=direct.saveState();restored.restoreState(state.data(),state.size());EXPECT_EQ(state,restored.saveState());
    }mz_zip_reader_end(&zip);
}
TEST(PatchLibrary, DirectPatchRetainsUtf8TitleCrLfAndSignedValues) {
    TemporaryFile file(".SY1");const std::string text="日本語 patch\r\nver=113\r\n9,-24\r\n86,-1\r\n";ASSERT_TRUE(file.write(text));PatchLibrary library;std::string error;
    ASSERT_TRUE(library.openFile(file.path.string(),error));ASSERT_EQ(library.patches().size(),1u);EXPECT_EQ(library.patches()[0].text,text);EXPECT_NE(library.patches()[0].name.find("日本語 patch"),std::string::npos);EXPECT_EQ(library.search("日本語"),(std::vector<size_t>{0}));
}
TEST(PatchLibrary, ZipSortsNaturallyAndSearchReturnsOriginalSnapshotIndices) {
    TemporaryFile file(".zip");ASSERT_TRUE(writeZip(file.path,{{"bank/10.SY1","Bright lead\nver=113\n0,0\n"},{"bank/2.sy1","Soft bass\nver=113\n0,1\n"},{"readme.md","Read me"}}));PatchLibrary library;std::string error;
    ASSERT_TRUE(library.openFile(file.path.string(),error))<<error;ASSERT_EQ(library.patches().size(),2u);EXPECT_EQ(library.patches()[0].name,"bank/2.sy1  -  Soft bass");EXPECT_EQ(library.patches()[1].name,"bank/10.SY1  -  Bright lead");
    EXPECT_EQ(library.search("  LEAD  "),(std::vector<size_t>{1}));EXPECT_EQ(library.search("bank/2"),(std::vector<size_t>{0}));EXPECT_EQ(library.search(""),(std::vector<size_t>{0,1}));EXPECT_TRUE(library.search("no such patch").empty());
}
TEST(PatchLibrary, FailedOpenPreservesPreviousSnapshot) {
    TemporaryFile input(".zip");ASSERT_TRUE(writeZip(input.path,testfiles::generatedBank()));
    PatchLibrary library;std::string error;ASSERT_TRUE(library.openFile(input.path.string(),error));const auto name=library.name(),first=library.patches().front().text;
    TemporaryFile corrupt(".zip"),empty(".sy1"),other(".bin"),missing(".zip");ASSERT_TRUE(corrupt.write("Not a ZIP"));ASSERT_TRUE(empty.write(""));ASSERT_TRUE(other.write("x"));
    for(const auto& file:{corrupt.path,empty.path,other.path,missing.path}){EXPECT_FALSE(library.openFile(file.string(),error));EXPECT_FALSE(error.empty());EXPECT_EQ(library.name(),name);ASSERT_EQ(library.patches().size(),128u);EXPECT_EQ(library.patches().front().text,first);}
}
TEST(PatchLibrary, EmptyAndOversizedZipPatchesDoNotReplaceTheBank) {
    TemporaryFile input(".zip");ASSERT_TRUE(writeZip(input.path,testfiles::generatedBank()));
    PatchLibrary library;std::string error;ASSERT_TRUE(library.openFile(input.path.string(),error));TemporaryFile noPatches(".zip"),empty(".zip"),large(".zip");ASSERT_TRUE(writeZip(noPatches.path,{{"README.md","No patches"}}));ASSERT_TRUE(writeZip(empty.path,{{"empty.sy1",""}}));ASSERT_TRUE(writeZip(large.path,{{"large.sy1",std::string(PatchLibrary::maximumPatchBytes+1,'x')}}));
    for(const auto& file:{noPatches.path,empty.path,large.path}){EXPECT_FALSE(library.openFile(file.string(),error));EXPECT_FALSE(error.empty());EXPECT_EQ(library.name(),input.path.filename().string());EXPECT_EQ(library.patches().size(),128u);}
}
TEST(PatchLibrary, SelectedPatchStateRestoresWithoutTheSourceFile) {
    TemporaryFile file(".zip");ASSERT_TRUE(writeZip(file.path,{{"Owned.sy1",ownedPatch}}));PatchLibrary library;std::string error;ASSERT_TRUE(library.openFile(file.path.string(),error));synth2::host::ProcessorBridge loaded,recalled;
    ASSERT_TRUE(loaded.loadProgram(library.patches()[0].text,0,error,library.patches()[0].name));const auto displayName=library.patches()[0].name;const auto state=loaded.saveState();ASSERT_TRUE(std::filesystem::remove(file.path));recalled.restoreState(state.data(),state.size());EXPECT_EQ(recalled.currentPatchName(),displayName);loaded.prepare(48000,64);recalled.prepare(48000,64);
    std::array<float,64> l{},r{},otherL{},otherR{};ASSERT_TRUE(loaded.enqueueMidi(7,0x90,60,100));ASSERT_TRUE(recalled.enqueueMidi(7,0x90,60,100));for(int block=0;block<100;++block){ASSERT_TRUE(loaded.process(l.data(),r.data(),64,120));ASSERT_TRUE(recalled.process(otherL.data(),otherR.data(),64,120));EXPECT_EQ(l,otherL);EXPECT_EQ(r,otherR);}
}
TEST(PatchLibrary, Synth2ContinuousPatchLoadsWithItsNameAndFractionalValues) {
    synth2::host::ProcessorBridge source,loaded;std::string error;
    source.setNormalizedValue(19,37.25/127.0);
    TemporaryFile file(".sy1");ASSERT_TRUE(file.write(source.savePatch()));
    PatchLibrary library;ASSERT_TRUE(library.openFile(file.path.string(),error))<<error;
    ASSERT_EQ(library.patches().size(),1u);
    EXPECT_NE(library.patches()[0].name.find(source.currentPatchName()),std::string::npos);
    ASSERT_TRUE(loaded.loadProgram(library.patches()[0].text,0,error))<<error;
    EXPECT_EQ(loaded.savePatch(),source.savePatch());
}
TEST(PatchLibrary, InvalidSourcePatchDoesNotChangeTheSoundingState) {
    synth2::host::ProcessorBridge bridge;std::string error;ASSERT_TRUE(bridge.loadPatch(ownedPatch,error));const auto before=bridge.saveState();TemporaryFile file(".sy1");ASSERT_TRUE(file.write("Invalid\nver=999\n0,0\n"));PatchLibrary library;ASSERT_TRUE(library.openFile(file.path.string(),error));EXPECT_FALSE(bridge.loadPatch(library.patches()[0].text,error));EXPECT_FALSE(error.empty());EXPECT_EQ(bridge.saveState(),before);
}
TEST(PatchLibrary, LongZipFilenameIsNotTruncated) {
    TemporaryFile file(".zip");const std::string name=std::string(700,'a')+".sy1";ASSERT_TRUE(writeZip(file.path,{{name,ownedPatch}}));PatchLibrary library;std::string error;ASSERT_TRUE(library.openFile(file.path.string(),error))<<error;ASSERT_EQ(library.patches().size(),1u);EXPECT_TRUE(library.patches()[0].name.starts_with(name+"  -  "));EXPECT_EQ(library.patches()[0].text,ownedPatch);
}
TEST(EditorPages, SoundParametersAppearOnceAndMetadataIsHidden) {
    std::array<int,128> seen{};for(const auto& page:synth2::editor::editorPages()){EXPECT_FALSE(page.parameters.empty());EXPECT_LE(page.parameters.size(),30u);for(int i:page.parameters){ASSERT_GE(i,0);ASSERT_LT(i,128);++seen[i];}}for(int i=0;i<128;++i)EXPECT_EQ(seen[i],i<99 ? 1 : 0)<<i;
}

TEST(PatchLibrary, CommaTitleIsDisplayedAndSearchable) {
    TemporaryFile file(".sy1");ASSERT_TRUE(file.write("Owned lead (wheel,aftertouch)\ncolor=blue\nver=112\n19,91\n"));
    PatchLibrary library;std::string error;ASSERT_TRUE(library.openFile(file.path.string(),error))<<error;
    EXPECT_TRUE(library.patches()[0].name.ends_with("  -  Owned lead (wheel,aftertouch)"));
    EXPECT_EQ(library.search("wheel,aftertouch"),(std::vector<size_t>{0}));
}

TEST(PatchLibrary, ColorsAreAvailableForLooseAndArchivedSourcesWithoutChangingBytes) {
    const std::string text="Owned color\r\nver=113\r\ncolor=magenta\r\n19,64\r\n";
    TemporaryFile loose(".sy1"),archive(".zip");
    ASSERT_TRUE(loose.write(text));
    ASSERT_TRUE(writeZip(archive.path,{{"Nested/Color.sy1",text}}));
    for(const auto& path:{loose.path,archive.path}) {
        PatchLibrary library;std::string error;
        ASSERT_TRUE(library.openFile(path.string(),error))<<error;
        ASSERT_EQ(library.patches().size(),1u);
        EXPECT_EQ(library.patches()[0].color,0xff00ffu);
        EXPECT_EQ(library.patches()[0].text,text);
    }
}
