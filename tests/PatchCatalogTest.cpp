#include <gtest/gtest.h>
#include "host/PatchCatalog.h"
#include "host/ProcessorBridge.h"
#include <miniz.h>
#include <atomic>
#include <chrono>
#include <fstream>

using namespace synth2::host;
namespace {
namespace fs=std::filesystem;
struct CatalogFiles {
    fs::path dir;
    CatalogFiles() {
        static std::atomic<unsigned> serial{0};
        dir=fs::temp_directory_path()/("synth2-catalog-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(serial++));
        fs::create_directories(dir);
    }
    ~CatalogFiles(){std::error_code ignored;fs::remove_all(dir,ignored);}
    fs::path bank(const std::string& name,const std::vector<std::pair<std::string,std::string>>& patches) {
        const auto path=dir/name;mz_zip_archive zip{};
        if(!mz_zip_writer_init_file(&zip,path.string().c_str(),0))throw std::runtime_error("ZIP creation failed");
        for(const auto& [file,text]:patches)
            if(!mz_zip_writer_add_mem(&zip,file.c_str(),text.data(),text.size(),MZ_BEST_COMPRESSION))throw std::runtime_error("ZIP write failed");
        if(!mz_zip_writer_finalize_archive(&zip))throw std::runtime_error("ZIP finalize failed");
        mz_zip_writer_end(&zip);return path;
    }
};
const std::string warm="Warm Pad\nver=113\n19,97\n29,87\n";
const std::string bass="Soft Bass\nver=113\n19,83\n29,76\n";
const std::string lead="Bright Lead\nver=113\n19,103\n29,85\n";
std::size_t imported(const PatchCatalog& catalog) {
    if(!catalog.patches().empty())return 0;
    throw std::runtime_error("Missing imported patch");
}
}

TEST(PatchCatalog, ImportAccumulatesAndSurvivesSourceRemovalAndNewInstance) {
    CatalogFiles files;PatchCatalog catalog(files.dir/"Library");std::string error;ImportResult result;
    ASSERT_TRUE(catalog.reload(error))<<error;const auto initial=catalog.patches().size();ASSERT_EQ(initial,0u);
    const auto first=files.bank("Ambient.zip",{{"1.sy1",warm},{"2.sy1",bass}});
    const auto second=files.bank("Leads.zip",{{"1.sy1",lead},{"duplicate.sy1",warm}});
    ASSERT_TRUE(catalog.importFile(first.string(),result,error))<<error;EXPECT_EQ(result.added,2u);
    ASSERT_TRUE(catalog.importFile(second.string(),result,error))<<error;EXPECT_EQ(result.added,1u);EXPECT_EQ(result.duplicates,1u);
    EXPECT_EQ(catalog.patches().size(),initial+3);
    fs::remove(first);fs::remove(second);
    PatchCatalog reopened(files.dir/"Library");ASSERT_TRUE(reopened.reload(error))<<error;
    EXPECT_EQ(reopened.patches().size(),initial+3);
    const auto matches=reopened.search({"WARM pad ambient"});ASSERT_EQ(matches.size(),1u);
    EXPECT_EQ(reopened.patches()[matches[0]].patch.text,warm);
    ASSERT_TRUE(reopened.importFile(files.bank("Again.zip",{{"same.sy1",warm}}).string(),result,error));
    EXPECT_EQ(result.added,0u);EXPECT_EQ(result.duplicates,1u);
}
TEST(PatchCatalog, TagsAndFavoritesAreSeparatePersistentMetadataAndCombineWithSearch) {
    CatalogFiles files;PatchCatalog catalog(files.dir/"Library");std::string error;ImportResult result;
    const auto source=files.bank("Ambient.zip",{{"1.sy1",warm},{"2.sy1",bass}});
    ASSERT_TRUE(catalog.importFile(source.string(),result,error))<<error;
    const auto patch=catalog.patches()[imported(catalog)];
    ASSERT_TRUE(catalog.setTags(patch.id," Pad , warm, 柔らかい, PAD, ",error))<<error;
    ASSERT_TRUE(catalog.setFavorite(patch.id,true,error));
    PatchCatalog reopened(files.dir/"Library");ASSERT_TRUE(reopened.reload(error))<<error;
    CatalogFilter filter{"AMBIENT 柔らかい",patch.bankId,"pAd",true};
    const auto matches=reopened.search(filter);ASSERT_EQ(matches.size(),1u);
    const auto& recalled=reopened.patches()[matches[0]];
    EXPECT_EQ(recalled.patch.text,warm);EXPECT_EQ(recalled.tags,(std::vector<std::string>{"Pad","warm","柔らかい"}));
    filter.query="Bass";EXPECT_TRUE(reopened.search(filter).empty());
    filter.query.clear();filter.bankId="factory";EXPECT_TRUE(reopened.search(filter).empty());
    ASSERT_TRUE(reopened.setTags(patch.id,"",error));ASSERT_TRUE(reopened.setFavorite(patch.id,false,error));
    ASSERT_TRUE(catalog.reload(error));EXPECT_TRUE(catalog.search({"柔らかい"}).empty());
    EXPECT_TRUE(catalog.search({"", "", "", true}).empty());
    PatchLibrary original;ASSERT_TRUE(original.openFile(source.string(),error));EXPECT_EQ(original.patches()[0].text,warm);
}
TEST(PatchCatalog, EveryImportedBankCanBeTaggedFavoritedAndRemoved) {
    CatalogFiles files;PatchCatalog catalog(files.dir/"Library");std::string error;ImportResult result;
    ASSERT_TRUE(catalog.reload(error));EXPECT_TRUE(catalog.patches().empty());
    ASSERT_TRUE(catalog.importFile(files.bank("Factory.zip",{{"1.sy1",warm},{"2.sy1",bass}}).string(),result,error));
    const auto id=catalog.patches()[0].id,bank=catalog.patches()[0].bankId;
    ASSERT_TRUE(catalog.setTags(id,"My choice",error));ASSERT_TRUE(catalog.setFavorite(id,true,error));
    ASSERT_TRUE(catalog.reload(error));EXPECT_EQ(catalog.search({"",bank,"my choice",true}).size(),1u);
    ASSERT_TRUE(catalog.removePatch(id,error));ASSERT_TRUE(catalog.removeBank(bank,error));
    ASSERT_TRUE(catalog.reload(error));EXPECT_TRUE(catalog.patches().empty());
}
TEST(PatchCatalog, EmptyLibraryKeepsInitAndExistingSoundStateIndependent) {
    CatalogFiles files;PatchCatalog catalog(files.dir/"Library");ProcessorBridge sounding;std::string error;
    const auto init=sounding.saveState();ASSERT_TRUE(catalog.reload(error));EXPECT_TRUE(catalog.patches().empty());
    EXPECT_EQ(sounding.currentPatchName(),"Init");EXPECT_EQ(sounding.saveState(),init);
    ASSERT_TRUE(sounding.loadPatch(warm,error));const auto edited=sounding.saveState();
    ASSERT_TRUE(catalog.reload(error));EXPECT_TRUE(catalog.patches().empty());EXPECT_EQ(sounding.saveState(),edited);
}
TEST(PatchCatalog, InvalidImportsDoNotPublishPartialBanksOrChangeProcessorState) {
    CatalogFiles files;PatchCatalog catalog(files.dir/"Library");ProcessorBridge sounding;std::string error;ImportResult result;
    ASSERT_TRUE(sounding.loadPatch(lead,error));const auto state=sounding.saveState();
    ASSERT_TRUE(catalog.importFile(files.bank("Good.zip",{{"1.sy1",warm}}).string(),result,error));
    const auto count=catalog.patches().size();
    EXPECT_FALSE(catalog.importFile(files.bank("Bad.zip",{{"1.sy1",bass},{"2.sy1","Invalid\nver=999\n19,0\n"}}).string(),result,error));
    EXPECT_EQ(result.added,0u);EXPECT_EQ(catalog.patches().size(),count);
    ASSERT_TRUE(catalog.reload(error));EXPECT_EQ(catalog.patches().size(),count);
    EXPECT_EQ(sounding.saveState(),state);
    EXPECT_FALSE(catalog.setTags("../../outside","escape",error));
    EXPECT_FALSE(catalog.removeBank("../outside",error));
}
TEST(PatchCatalog, RemovingSoundAndBankPreservesSourceAndSelfContainedRecall) {
    CatalogFiles files;PatchCatalog catalog(files.dir/"Library");std::string error;ImportResult result;
    const auto source=files.bank("Owned.zip",{{"1.sy1",warm},{"2.sy1",bass}});
    ASSERT_TRUE(catalog.importFile(source.string(),result,error));const auto p=catalog.patches()[imported(catalog)];
    ProcessorBridge sounding,recalled;ASSERT_TRUE(sounding.loadProgram(p.patch.text,p.program,error,p.patch.name));
    const auto state=sounding.saveState();ASSERT_TRUE(catalog.setTags(p.id,"Pad",error));
    ASSERT_TRUE(catalog.removePatch(p.id,error));ASSERT_TRUE(catalog.reload(error));
    EXPECT_TRUE(catalog.search({"Warm",p.bankId}).empty());ASSERT_EQ(catalog.search({"",p.bankId}).size(),1u);
    ASSERT_TRUE(catalog.removeBank(p.bankId,error));ASSERT_TRUE(catalog.reload(error));
    EXPECT_TRUE(catalog.search({"",p.bankId}).empty());EXPECT_TRUE(fs::exists(source));
    recalled.restoreState(state.data(),static_cast<int>(state.size()));EXPECT_EQ(recalled.currentPatchName(),p.patch.name);EXPECT_EQ(recalled.saveState(),state);
}
TEST(PatchCatalog, SeparateInstancesRefreshBeforeImportAndMetadataPersists) {
    CatalogFiles files;PatchCatalog first(files.dir/"Library"),second(files.dir/"Library");std::string error;ImportResult result;
    ASSERT_TRUE(first.reload(error));ASSERT_TRUE(second.reload(error));
    const auto source=files.bank("Owned.zip",{{"1.sy1",warm}});
    ASSERT_TRUE(first.importFile(source.string(),result,error));
    ASSERT_TRUE(second.importFile(source.string(),result,error));EXPECT_EQ(result.added,0u);EXPECT_EQ(result.duplicates,1u);
    const auto p=second.patches()[imported(second)];
    ASSERT_TRUE(second.setTags(p.id,"Pad",error));ASSERT_TRUE(first.reload(error));
    ASSERT_TRUE(first.setFavorite(p.id,true,error));ASSERT_TRUE(second.reload(error));
    EXPECT_EQ(second.search({"",p.bankId,"pad",true}).size(),1u);
}
TEST(PatchCatalog, RejectsOversizedTagWithoutChangingMetadata) {
    CatalogFiles files;PatchCatalog catalog(files.dir/"Library");std::string error;ImportResult result;
    ASSERT_TRUE(catalog.importFile(files.bank("Owned.zip",{{"1.sy1",warm}}).string(),result,error));
    const auto id=catalog.patches()[0].id;
    ASSERT_TRUE(catalog.setTags(id,"Pad",error));
    EXPECT_FALSE(catalog.setTags(id,std::string(129,'a'),error));ASSERT_TRUE(catalog.reload(error));
    EXPECT_EQ(catalog.patches()[0].tags,(std::vector<std::string>{"Pad"}));
}

TEST(PatchCatalog, FolderImportPreservesRelativeNamesAndNaturalOrder) {
    CatalogFiles files;const auto folder=files.dir/"OwnedFolder";
    fs::create_directories(folder/"Nested");
    {std::ofstream out(folder/"Nested"/"10.sy1");out<<bass;}
    {std::ofstream out(folder/"Nested"/"2.sy1");out<<warm;}
    PatchCatalog catalog(files.dir/"Library");std::string error;ImportResult result;
    ASSERT_TRUE(catalog.importFile(folder.string()+"/",result,error))<<error;EXPECT_EQ(result.added,2u);
    const auto i=imported(catalog);EXPECT_EQ(catalog.patches()[i].patch.text,warm);
    EXPECT_TRUE(catalog.patches()[i].patch.name.starts_with("Nested/2.sy1"));
    fs::remove_all(folder);ASSERT_TRUE(catalog.reload(error));
    EXPECT_EQ(catalog.search({"OwnedFolder"}).size(),2u);
}

TEST(PatchCatalog, ThousandsOfSoundsRetainBankOrdinalsAndCanBeNarrowed) {
    CatalogFiles files;PatchCatalog catalog(files.dir/"Library");std::string error;ImportResult result;
    for(const auto* bank:{"bulk-a","bulk-b"}) {
        std::vector<std::pair<std::string,std::string>> patches;
        for(int i=0;i<2100;++i)patches.push_back({std::to_string(i)+".sy1",std::string(bank)+" "+std::to_string(i)+"\nver=113\n19,91\n"});
        ASSERT_TRUE(catalog.importFile(files.bank(std::string(bank)+".zip",patches).string(),result,error))<<error;
        EXPECT_EQ(result.added,2100u);
    }
    const auto matches=catalog.search({"bulk-b 2099"});ASSERT_EQ(matches.size(),1u);EXPECT_GT(matches[0],4095u);
    const auto p=catalog.patches()[matches[0]];EXPECT_EQ(p.program,2099);
    ASSERT_TRUE(catalog.setTags(p.id,"Pad, Warm",error));ASSERT_TRUE(catalog.setFavorite(p.id,true,error));
    EXPECT_EQ(catalog.search({"warm",p.bankId,"pad",true}),matches);
    ProcessorBridge processor;ASSERT_TRUE(processor.loadProgram(p.patch.text,p.program,error,p.patch.name));
    EXPECT_EQ(processor.currentProgram(),2099);
}
