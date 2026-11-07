#include <gtest/gtest.h>
#include "host/PatchCatalog.h"
#include "host/ProcessorBridge.h"
#include <miniz.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <algorithm>

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
    ASSERT_TRUE(catalog.importFile(second.string(),result,error))<<error;EXPECT_EQ(result.added,2u);EXPECT_EQ(result.duplicates,0u);
    EXPECT_EQ(catalog.patches().size(),initial+4);
    fs::remove(first);fs::remove(second);
    PatchCatalog reopened(files.dir/"Library");ASSERT_TRUE(reopened.reload(error))<<error;
    EXPECT_EQ(reopened.patches().size(),initial+4);
    const auto matches=reopened.search({"WARM pad ambient"});ASSERT_EQ(matches.size(),1u);
    EXPECT_EQ(reopened.patches()[matches[0]].patch.text,warm);
    ASSERT_TRUE(reopened.importFile(files.bank("Again.zip",{{"same.sy1",warm}}).string(),result,error));
    EXPECT_EQ(result.added,1u);EXPECT_EQ(result.duplicates,0u);
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
    EXPECT_FALSE(catalog.removePatch(id,error));ASSERT_TRUE(catalog.removeBank(bank,error));
    ASSERT_TRUE(catalog.reload(error));EXPECT_TRUE(catalog.patches().empty());
}
TEST(PatchCatalog, EmptyLibraryKeepsInitAndExistingSoundStateIndependent) {
    CatalogFiles files;PatchCatalog catalog(files.dir/"Library");ProcessorBridge sounding;std::string error;
    const auto init=sounding.saveState();ASSERT_TRUE(catalog.reload(error));EXPECT_TRUE(catalog.patches().empty());
    EXPECT_EQ(sounding.currentPatchName(),"Init");EXPECT_EQ(sounding.saveState(),init);
    ASSERT_TRUE(sounding.loadPatch(warm,error));const auto edited=sounding.saveState();
    ASSERT_TRUE(catalog.reload(error));EXPECT_TRUE(catalog.patches().empty());EXPECT_EQ(sounding.saveState(),edited);
}
TEST(PatchCatalog, InvalidRowsRemainVisibleAlongsideValidRowsWithoutChangingSound) {
    CatalogFiles files;PatchCatalog catalog(files.dir/"Library");ProcessorBridge sounding;std::string error;ImportResult result;
    ASSERT_TRUE(sounding.loadPatch(lead,error));const auto state=sounding.saveState();
    ASSERT_TRUE(catalog.importFile(files.bank("Good.zip",{{"1.sy1",warm}}).string(),result,error));
    const auto count=catalog.patches().size();
    EXPECT_FALSE(catalog.importFile(files.bank("Bad.zip",{{"1.sy1",bass},{"2.sy1","Invalid\nver=999\n19,0\n"}}).string(),result,error));
    EXPECT_EQ(result.added,2u);EXPECT_EQ(catalog.patches().size(),count+2);
    ASSERT_TRUE(catalog.reload(error));EXPECT_EQ(catalog.patches().size(),count+2);
    EXPECT_EQ(std::count_if(catalog.patches().begin(),catalog.patches().end(),[](const auto& p){return !p.error.empty();}),1);
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
    EXPECT_FALSE(catalog.removePatch(p.id,error));ASSERT_TRUE(catalog.reload(error));
    EXPECT_EQ(catalog.search({"Warm",p.bankId}).size(),1u);ASSERT_EQ(catalog.search({"",p.bankId}).size(),2u);
    ASSERT_TRUE(catalog.removeBank(p.bankId,error));ASSERT_TRUE(catalog.reload(error));
    EXPECT_TRUE(catalog.search({"",p.bankId}).empty());EXPECT_TRUE(fs::exists(source));
    recalled.restoreState(state.data(),static_cast<int>(state.size()));EXPECT_EQ(recalled.currentPatchName(),p.patch.name);EXPECT_EQ(recalled.saveState(),state);
}
TEST(PatchCatalog, SeparateInstancesRefreshBeforeImportAndMetadataPersists) {
    CatalogFiles files;PatchCatalog first(files.dir/"Library"),second(files.dir/"Library");std::string error;ImportResult result;
    ASSERT_TRUE(first.reload(error));ASSERT_TRUE(second.reload(error));
    const auto source=files.bank("Owned.zip",{{"1.sy1",warm}});
    ASSERT_TRUE(first.importFile(source.string(),result,error));
    PresetImportResult skipped;ASSERT_TRUE(second.registerFiles(source.string(),[](const auto&){return ConflictChoice{FileConflict::skip};},skipped));EXPECT_EQ(skipped.imported,0u);EXPECT_EQ(skipped.skipped,1u);
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
    {std::ofstream out(folder/"Nested"/"10.sy1",std::ios::binary);out<<bass;}
    {std::ofstream out(folder/"Nested"/"2.sy1",std::ios::binary);out<<warm;}
    PatchCatalog catalog(files.dir/"Library");std::string error;ImportResult result;
    ASSERT_TRUE(catalog.importFile(folder.string()+"/",result,error))<<error;EXPECT_EQ(result.added,2u);
    const auto i=imported(catalog);EXPECT_EQ(catalog.patches()[i].patch.text,warm);
    EXPECT_EQ(catalog.patches()[i].bankName,"OwnedFolder/Nested");
    EXPECT_EQ(catalog.search({"OwnedFolder/Nested/2.sy1"}),(std::vector<std::size_t>{i}));
    const auto bankId=catalog.patches()[i].bankId;
    EXPECT_EQ(catalog.search({"",bankId}).size(),2u);
    fs::remove_all(folder);ASSERT_TRUE(catalog.reload(error));
    EXPECT_EQ(catalog.patches()[i].bankName,"OwnedFolder/Nested");
    EXPECT_EQ(catalog.patches()[i].bankId,bankId);
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

TEST(PatchCatalog, LegacyManagedBanksAndMetadataRemainReadableAlongsideFiles) {
    CatalogFiles files;const auto root=files.dir/"Library";fs::create_directories(root/"banks/bank-legacy");
    std::ofstream(root/"banks/bank-legacy/name")<<"Existing bank";
    std::ofstream(root/"banks/bank-legacy/0002.sy1")<<warm;
    std::ofstream(root/"banks/bank-legacy/0002.name")<<"Existing name";
    fs::create_directories(root/"tags");std::ofstream(root/"tags/bank-legacy_0002.tags")<<"Pad";
    fs::create_directories(root/"favorites");std::ofstream(root/"favorites/bank-legacy_0002.fav")<<"favorite";
    std::ofstream(root/"New.sy1")<<bass;PatchCatalog catalog(root);std::string error;ASSERT_TRUE(catalog.reload(error))<<error;
    ASSERT_EQ(catalog.patches().size(),2u);auto matches=catalog.search({"existing","bank-legacy","Pad",true});ASSERT_EQ(matches.size(),1u);
    EXPECT_EQ(catalog.patches()[matches[0]].id,"bank-legacy/0002");EXPECT_EQ(catalog.patches()[matches[0]].program,2);
}

TEST(PatchCatalog, FiveDigitLegacyManagedOrdinalsKeepTheirIdentity) {
    CatalogFiles files;const auto root=files.dir/"Library",bank=root/"banks/bank-legacy";
    fs::create_directories(bank);std::ofstream(bank/"name")<<"Existing bank";
    std::ofstream(bank/"10000.sy1")<<warm;std::ofstream(bank/"10000.name")<<"Existing high ordinal";
    ASSERT_TRUE(PresetFiles::isManagedPatch(root,bank/"10000.sy1"));
    PatchCatalog catalog(root);std::string error;ASSERT_TRUE(catalog.reload(error))<<error;
    ASSERT_EQ(catalog.patches().size(),1u);EXPECT_EQ(catalog.patches()[0].id,"bank-legacy/10000");
    EXPECT_EQ(catalog.patches()[0].program,10000);
    ASSERT_TRUE(catalog.setFavorite(catalog.patches()[0].id,true,error))<<error;
    ASSERT_TRUE(catalog.reload(error))<<error;EXPECT_TRUE(catalog.patches()[0].favorite);
}

TEST(PatchCatalog, RegistrationFilterTracksExactFilesAndRetainsZipAsOneFile) {
    CatalogFiles files;PatchCatalog catalog(files.dir/"Library");std::string error;
    const auto zip=files.bank("Owned.zip",{{"1.sy1",warm},{"2.sy1",bass}});
    PresetImportResult result;ASSERT_TRUE(catalog.registerFiles(zip.string(),[](const auto&){return ConflictChoice{};},result));
    EXPECT_TRUE(fs::is_regular_file(catalog.root()/"Owned.zip"));EXPECT_FALSE(fs::exists(catalog.root()/"banks"));ASSERT_EQ(result.ids.size(),2u);
    CatalogFilter filter;filter.registeredOnly=true;filter.registeredIds={result.ids[0]};EXPECT_EQ(catalog.search(filter).size(),1u);
    const auto previous=catalog.patches()[0].patch.text;PresetImportResult skipped;
    ASSERT_TRUE(catalog.registerFiles(zip.string(),[](const auto&){return ConflictChoice{FileConflict::skip,true};},skipped));EXPECT_TRUE(skipped.ids.empty());EXPECT_EQ(skipped.skipped,1u);EXPECT_EQ(catalog.patches()[0].patch.text,previous);
}

TEST(PatchCatalog, FailedRegistrationReloadKeepsThePreviousCatalogSnapshot) {
    CatalogFiles files;const auto root=files.dir/"Library";PatchCatalog catalog(root);std::string error;
    const auto source=files.bank("Owned.zip",{{"1.sy1",warm}});PresetImportResult result;
    ASSERT_TRUE(catalog.registerFiles(source.string(),[](const auto&){return ConflictChoice{};},result));ASSERT_TRUE(result.catalogReady);
    const auto original=catalog.patches()[0].id;fs::rename(root,files.dir/"Backup");std::ofstream(root)<<"not a directory";
    EXPECT_FALSE(catalog.registerFiles(source.string(),[](const auto&){return ConflictChoice{};},result));EXPECT_FALSE(result.catalogReady);
    ASSERT_EQ(catalog.patches().size(),1u);EXPECT_EQ(catalog.patches()[0].id,original);
}

TEST(PatchCatalog, RefreshDiscoversDroppedZipsInsideOrdinaryAndFormerStorageFolders) {
    CatalogFiles files;const auto root=files.dir/"Library";PatchCatalog catalog(root);std::string error;
    ASSERT_TRUE(catalog.reload(error));ProcessorBridge sounding;const auto before=sounding.saveState();
    const auto source=files.bank("External.zip",{{"Nested/1.sy1",warm}});
    for(const auto* folder:{"", "banks", "tags", "favorites", "banks/bank-user"}) {
        const auto destination=root/folder/"Dropped.ZIP";fs::create_directories(destination.parent_path());fs::copy_file(source,destination);
    }
    ASSERT_TRUE(catalog.reload(error))<<error;
    EXPECT_EQ(catalog.patches().size(),5u);
    EXPECT_EQ(catalog.search({"Dropped nested warm"}).size(),5u);
    EXPECT_EQ(sounding.saveState(),before);
}

TEST(PatchCatalog, RefreshKeepsManagedIdsWithoutHidingNewSourcesInTheSameFolder) {
    CatalogFiles files;const auto root=files.dir/"Library";const auto bank=root/"banks/bank-existing";
    fs::create_directories(bank);std::ofstream(bank/"name")<<"Existing bank";
    std::ofstream(bank/"0002.sy1")<<warm;std::ofstream(bank/"0002.name")<<"Existing sound";
    PatchCatalog catalog(root);std::string error;ASSERT_TRUE(catalog.reload(error));ASSERT_EQ(catalog.patches().size(),1u);
    const auto original=catalog.patches()[0].id;ASSERT_TRUE(catalog.setFavorite(original,true,error));
    const auto source=files.bank("External.zip",{{"1.sy1",lead}});fs::copy_file(source,bank/"Added.zip");
    std::ofstream(bank/"Bass.sy1")<<bass;
    // A numbered loose file without a legacy display label is still an ordinary preset.
    std::ofstream(bank/"0003.SY1")<<bass;
    ASSERT_TRUE(catalog.reload(error))<<error;EXPECT_EQ(catalog.patches().size(),4u);
    EXPECT_EQ(std::count_if(catalog.patches().begin(),catalog.patches().end(),[&](const auto& p){return p.id==original && p.favorite;}),1);
    EXPECT_EQ(catalog.search({"Added lead"}).size(),1u);
}

TEST(PatchCatalog, UnrelatedFileNamedBanksDoesNotBlockDroppedZipDiscovery) {
    CatalogFiles files;const auto root=files.dir/"Library";fs::create_directories(root);std::ofstream(root/"banks")<<"ordinary file";
    const auto source=files.bank("External.zip",{{"1.sy1",warm}});fs::copy_file(source,root/"Added.zip");
    PatchCatalog catalog(root);std::string error;ASSERT_TRUE(catalog.reload(error))<<error;ASSERT_EQ(catalog.patches().size(),1u);
    EXPECT_EQ(catalog.patches()[0].patch.text,warm);
}

TEST(PatchCatalog, FiveDigitProgramOrdinalsImportAndReloadWithoutTruncation) {
    CatalogFiles files;std::vector<std::pair<std::string,std::string>> patches;
    for(int i=0;i<=10000;++i)
        patches.emplace_back(std::to_string(i)+".sy1","Owned sound "+std::to_string(i)+"\nver=113\n19,97\n29,87\n");
    patches.back().second+="2,135\n";
    const auto bank=files.bank("Large.zip",patches);
    PatchCatalog catalog(files.dir/"Library");std::string error;ImportResult result;
    ASSERT_TRUE(catalog.importFile(bank.string(),result,error))<<error;
    EXPECT_EQ(result.added,10001u);EXPECT_EQ(result.duplicates,0u);
    fs::remove(bank);PatchCatalog reopened(files.dir/"Library");
    ASSERT_TRUE(reopened.reload(error))<<error;ASSERT_EQ(reopened.patches().size(),10001u);
    const auto matches=reopened.search({"Owned sound 10000"});ASSERT_EQ(matches.size(),1u);
    const auto& last=reopened.patches()[matches[0]];EXPECT_EQ(last.program,10000);
    ProcessorBridge selected;ASSERT_TRUE(selected.loadProgram(last.patch.text,last.program,error,last.patch.name))<<error;
    EXPECT_EQ(selected.currentProgram(),10000);
}


TEST(PatchCatalog, ZipDirectoryBanksIncludeAncestorsAndPreserveSoundIdentity) {
    CatalogFiles files;const auto source=files.bank("Collection.zip",{
        {"root.sy1",warm},{"Pads/1.sy1",warm},{"Pads/Deep/2.sy1",bass},
        {"PadsExtra/3.sy1",lead},{"Other/Deep/4.sy1",lead}});
    const auto root=files.dir/"Library";
    PatchCatalog catalog(root);std::string error;ImportResult result;
    ASSERT_TRUE(catalog.importFile(source.string(),result,error))<<error;
    EXPECT_EQ(result.added,5u);
    const auto banks=catalog.banks();std::vector<std::string> ids;
    for(const auto& bank:banks)ids.push_back(bank.id);
    EXPECT_EQ(ids,(std::vector<std::string>{"Collection.zip","Collection.zip/Other",
        "Collection.zip/Other/Deep","Collection.zip/Pads","Collection.zip/Pads/Deep","Collection.zip/PadsExtra"}));
    EXPECT_EQ(catalog.search({"","Collection.zip"}).size(),1u);
    EXPECT_EQ(catalog.search({"","Collection.zip/Pads"}).size(),1u);
    const auto deep=catalog.search({"","Collection.zip/Pads/Deep"});ASSERT_EQ(deep.size(),1u);
    const auto patch=catalog.patches()[deep.front()];
    EXPECT_EQ(patch.bankName,"Collection.zip/Pads/Deep");
    EXPECT_EQ(patch.archiveEntry,"Pads/Deep/2.sy1");
    PresetFiles original(root);ASSERT_TRUE(original.scan(error));
    const auto originalEntry=std::find_if(original.entries().begin(),original.entries().end(),
        [&](const auto& p){return p.archiveEntry==patch.archiveEntry;});
    ASSERT_NE(originalEntry,original.entries().end());
    EXPECT_EQ(patch.id,originalEntry->id);EXPECT_EQ(patch.program,originalEntry->program);
    EXPECT_EQ(patch.patch.text,bass);
    ASSERT_TRUE(catalog.setFavorite(patch.id,true,error));ASSERT_TRUE(catalog.setTags(patch.id,"soft",error));
    PatchCatalog reopened(root);ASSERT_TRUE(reopened.reload(error));
    const auto matches=reopened.search({"Bass","Collection.zip/Pads/Deep","soft",true});ASSERT_EQ(matches.size(),1u);
    const auto& recalled=reopened.patches()[matches[0]];
    EXPECT_EQ(recalled.id,patch.id);EXPECT_EQ(recalled.program,patch.program);EXPECT_EQ(recalled.patch.text,bass);
    // A logical sub-bank must never delete its containing archive or sibling sounds.
    EXPECT_FALSE(reopened.removeBank("Collection.zip/Pads/Deep",error));
    EXPECT_TRUE(fs::exists(root/"Collection.zip"));EXPECT_EQ(reopened.patches().size(),5u);
    ASSERT_TRUE(reopened.removeBank("Collection.zip",error))<<error;
    EXPECT_TRUE(reopened.patches().empty());ASSERT_TRUE(reopened.reload(error));EXPECT_TRUE(reopened.patches().empty());
}

TEST(PatchCatalog, OrdinaryDirectoriesAndNestedZipsShareBankHierarchy) {
    CatalogFiles files;const auto root=files.dir/"Library";
    for(const auto* folder:{"Set/Pads/Deep","Set/PadsExtra","Other/Pads"})fs::create_directories(root/folder);
    std::ofstream(root/"root.sy1")<<lead;
    std::ofstream(root/"Set/Pads/1.sy1")<<warm;
    std::ofstream(root/"Set/Pads/Deep/2.sy1")<<bass;
    std::ofstream(root/"Set/PadsExtra/3.sy1")<<lead;
    std::ofstream(root/"Other/Pads/4.sy1")<<bass;
    const auto zip=files.bank("Nested.zip",{{"Inside/5.sy1",warm}});
    fs::copy_file(zip,root/"Set/Pads/Nested.zip");
    PatchCatalog catalog(root);std::string error;ASSERT_TRUE(catalog.reload(error))<<error;
    EXPECT_EQ(catalog.search({"","Set"}).size(),0u);
    EXPECT_EQ(catalog.search({"","Set/Pads"}).size(),1u);
    EXPECT_EQ(catalog.search({"","Set/Pads/Deep"}).size(),1u);
    EXPECT_EQ(catalog.search({"","Other/Pads"}).size(),1u);
    EXPECT_EQ(catalog.search({"","Set/Pads/Nested.zip"}).size(),0u);
    EXPECT_EQ(catalog.search({"","Set/Pads/Nested.zip/Inside"}).size(),1u);
    EXPECT_EQ(catalog.search({"","."}).size(),1u);
    const auto banks=catalog.banks();
    for(const auto* id:{".","Set","Set/Pads","Set/Pads/Deep","Set/PadsExtra","Other","Other/Pads", "Set/Pads/Nested.zip", "Set/Pads/Nested.zip/Inside"})
        EXPECT_NE(std::find_if(banks.begin(),banks.end(),[&](const auto& bank){return bank.id==id;}),banks.end())<<id;
    EXPECT_EQ(catalog.search({"Set/Pads/1.sy1"}).size(),1u);
    EXPECT_FALSE(catalog.removeBank("Set/Pads",error));
    ASSERT_TRUE(catalog.reload(error));EXPECT_EQ(catalog.patches().size(),6u);
    EXPECT_EQ(catalog.search({"Bright Lead","Set"}).size(),0u);
}

TEST(PatchCatalog, ZipDirectoryBanksKeepUtf8AndOriginalBackslashEntryNames) {
    CatalogFiles files;const auto source=files.bank("Sounds.zip",{{"音色\\Pad\\1.sy1",warm},{"音色/Pad/2.sy1",bass}});
    PatchCatalog catalog(files.dir/"Library");std::string error;ImportResult result;
    ASSERT_TRUE(catalog.importFile(source.string(),result,error))<<error;
    const auto matches=catalog.search({"","Sounds.zip/音色/Pad"});ASSERT_EQ(matches.size(),2u);
    EXPECT_EQ(catalog.search({"","Sounds.zip/音色"}).size(),0u);
    EXPECT_EQ(catalog.patches()[matches[0]].archiveEntry,"音色/Pad/2.sy1");
    EXPECT_EQ(catalog.patches()[matches[1]].archiveEntry,"音色\\Pad\\1.sy1");
}
