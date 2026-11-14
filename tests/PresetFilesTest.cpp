#include "host/PresetFiles.h"
#include "host/PatchCatalog.h"
#include <algorithm>
#include "support/BankFiles.h"
#include <gtest/gtest.h>
#include "patch/PatchFormat.h"
#include "patch/PatchLoader.h"
#include <fstream>
using namespace synth2::host;
namespace fs = std::filesystem;
namespace
{
const std::string patch = "Owned bass\nver=113\n0,1\n19,90\n35,25\n";
void write(const fs::path &p, const std::string &s = patch)
{
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << s;
}
const ResolveConflict keep = [](const auto &) { return ConflictChoice{}; };
} // namespace
TEST(PresetFiles, RecursiveScanFeedsCatalogSearchWithOriginalPathsAndErrors)
{
    testfiles::Directory d;
    write(d.path / "Bass" / "10.sy1");
    write(d.path / "Bass" / "2.sy1");
    write(d.path / "bad.sy1", "Bad\nver=999\n0,0\n");
    write(d.path / "broken.zip", "not zip");
    write(d.path / "ignored.md");
    ASSERT_TRUE(testfiles::zip(d.path / "Bank.zip",
                               {{"日本語/Lead.sy1", "Bright lead\nver=113\n0,0\n"}, {"a  -  b.sy1", patch}}));
    PresetFiles library(d.path);
    std::string error;
    ASSERT_TRUE(library.scan(error)) << error;
    ASSERT_EQ(library.entries().size(), 6u);
    PatchCatalog catalog(d.path);
    ASSERT_TRUE(catalog.reload(error)) << error;
    auto matches = catalog.search({"BASS/ owned"});
    ASSERT_EQ(matches.size(), 2u);
    EXPECT_TRUE(catalog.patches()[matches[0]].location.ends_with("2.sy1"));
    EXPECT_TRUE(catalog.patches()[matches[1]].location.ends_with("10.sy1"));
    auto zip = catalog.search({"日本語 bright"});
    ASSERT_EQ(zip.size(), 1u);
    EXPECT_EQ(catalog.patches()[zip[0]].archiveEntry, "日本語/Lead.sy1");
    auto spaced = catalog.search({"a  -  b.sy1"});
    ASSERT_EQ(spaced.size(), 1u);
    EXPECT_EQ(catalog.patches()[spaced[0]].archiveEntry, "a  -  b.sy1");
    int invalid = 0;
    for (auto &item : library.entries())
        invalid += !item.error.empty();
    EXPECT_EQ(invalid, 2);
}
TEST(PresetFiles, ZipImportStaysOneFileAndFiltersExactlyThisImport)
{
    testfiles::Directory source, destination;
    auto zip = source.path / "Bank.zip";
    ASSERT_TRUE(testfiles::zip(zip, {{"a.sy1", patch}, {"b.sy1", patch}}));
    write(destination.path / "existing.sy1");
    PresetFiles library(destination.path);
    auto result = library.import(zip, keep);
    EXPECT_TRUE(result.errors.empty());
    EXPECT_EQ(result.imported, 1u);
    ASSERT_EQ(result.ids.size(), 2u);
    EXPECT_TRUE(fs::exists(destination.path / "Bank.zip"));
    EXPECT_FALSE(fs::exists(destination.path / "a.sy1"));
    PatchCatalog catalog(destination.path);
    std::string error;
    ASSERT_TRUE(catalog.reload(error)) << error;
    CatalogFilter filter;filter.registeredOnly = true;filter.registeredIds = result.ids;
    EXPECT_EQ(catalog.search(filter).size(), 2u);
    EXPECT_EQ(catalog.search({}).size(), 3u);
    auto again = library.import(destination.path / "Bank.zip", keep);
    EXPECT_EQ(again.ids, result.ids);
    EXPECT_FALSE(fs::exists(destination.path / "Bank (2).zip"));
}
TEST(PresetFiles, FileConflictsKeepBothSkipAndReplace)
{
    testfiles::Directory source, destination;
    write(source.path / "Bass.sy1");
    write(destination.path / "Bass.sy1", "Original\nver=113\n0,0\n");
    PresetFiles library(destination.path);
    auto a = library.import(source.path / "Bass.sy1", keep);
    EXPECT_EQ(a.imported, 1u);
    EXPECT_TRUE(fs::exists(destination.path / "Bass (2).sy1"));
    auto b = library.import(source.path / "Bass.sy1",
                            [](const auto &) { return ConflictChoice{FileConflict::skip}; });
    EXPECT_EQ(b.skipped, 1u);
    EXPECT_TRUE(b.ids.empty());
    auto c = library.import(source.path / "Bass.sy1",
                            [](const auto &) { return ConflictChoice{FileConflict::replace}; });
    EXPECT_EQ(c.imported, 1u);
    const auto found = std::find_if(library.entries().begin(), library.entries().end(),
        [](const auto& item) { return item.file.filename() == "Bass.sy1"; });
    ASSERT_NE(found, library.entries().end());
    EXPECT_EQ(found->text, patch);
}
TEST(PresetFiles, FolderHierarchyAndApplyToAllPreserveOriginals)
{
    testfiles::Directory source, destination;
    auto folder = source.path / "Sounds";
    write(folder / "nested" / "a.sy1");
    write(folder / "b.sy1");
    PresetFiles library(destination.path);
    EXPECT_EQ(library.import(folder, keep).imported, 2u);
    int prompts = 0;
    auto result = library.import(folder,
                                 [&](const auto &)
                                 {
                                     ++prompts;
                                     return ConflictChoice{FileConflict::keepBoth, true};
                                 });
    EXPECT_EQ(prompts, 1);
    EXPECT_EQ(result.imported, 2u);
    EXPECT_TRUE(fs::exists(destination.path / "Sounds/nested/a (2).sy1"));
    EXPECT_TRUE(fs::exists(folder / "nested/a.sy1"));
}
TEST(PresetFiles, SavesConfinePathsAndDoNotTruncateOnFailure)
{
    testfiles::Directory d, outside;
    PresetFiles library(d.path);
    std::string id, error;
    ASSERT_TRUE(library.save("nested/New.sy1", patch, false, id, error)) << error;
    EXPECT_FALSE(library.save("nested/New.sy1", "Other\nver=113\n0,0\n", false, id, error));
    EXPECT_EQ(library.entries()[0].text, patch);
    EXPECT_FALSE(library.save("../escape.sy1", patch, false, id, error));
    EXPECT_FALSE(fs::exists(d.path.parent_path() / "escape.sy1"));
    std::error_code ec;
    fs::create_directory_symlink(outside.path, d.path / "link", ec);
    if (!ec)
        EXPECT_FALSE(library.save("link/escape.sy1", patch, false, id, error));
    EXPECT_FALSE(library.save("nested/New.sy1", "Bad\nver=999\n0,0\n", true, id, error));
    EXPECT_EQ(library.entries()[0].text, patch);
}
TEST(PresetFiles, BrowserRecallRetainsBaselineAndDoesNotLoadExternalFiles)
{
    ProcessorBridge p;
    std::string error;
    ASSERT_TRUE(p.loadPatch(patch, error));
    PresetBrowserState state;
    state.capture(p);
    state.query = "bass warm";
    state.bankId = "Bank.zip";
    state.tag = "Pad";
    state.favoritesOnly = true;
    state.page = 2;
    state.selectedId = "8:Bank.zipa.sy1";
    state.patchName = "Owned & bass";
    state.restricted = true;
    state.importedIds = {state.selectedId};
    state.scroll = 13;
    EXPECT_FALSE(state.modified(p));
    auto old = p.normalizedValue(19);
    p.setNormalizedValue(19, .25);
    EXPECT_TRUE(state.modified(p));
    auto audioState = p.saveState();
    auto browserState = state.serialize();
    ProcessorBridge recalled;
    recalled.restoreState(audioState.data(), audioState.size());
    PresetBrowserState restored;
    ASSERT_TRUE(PresetBrowserState::deserialize(browserState, restored));
    EXPECT_EQ(restored.query, state.query);
    EXPECT_EQ(restored.bankId, state.bankId);
    EXPECT_EQ(restored.tag, state.tag);
    EXPECT_TRUE(restored.favoritesOnly);
    EXPECT_EQ(restored.page, 2);
    EXPECT_EQ(restored.selectedId, state.selectedId);
    EXPECT_EQ(restored.patchName, state.patchName);
    EXPECT_EQ(restored.scroll, 13);
    EXPECT_EQ(restored.importedIds, state.importedIds);
    EXPECT_TRUE(restored.modified(recalled));
    recalled.setNormalizedValue(19, old);
    EXPECT_FALSE(restored.modified(recalled));
    EXPECT_EQ(state.baseline, restored.baseline);
    auto before = restored.serialize();
    EXPECT_FALSE(PresetBrowserState::deserialize("<browser version='1'/>", restored));
    EXPECT_EQ(before, restored.serialize());
}
TEST(PresetFiles, SavesCurrentFormatWithoutLosingFractionalControlsOrMasterGain)
{
    testfiles::Directory d;
    PresetFiles files(d.path);
    ProcessorBridge processor, recalled;
    std::string error, id;
    ASSERT_TRUE(processor.loadPatch(patch, error));
    processor.setNormalizedValue(19, .253);
    processor.setNormalizedValue(128, .37);
    const auto state = processor.saveState();
    const auto text = processor.savePatch();
    ASSERT_TRUE(files.save("Fractional.sy1", text, false, id, error)) << error;
    EXPECT_EQ(state, processor.saveState());
    ASSERT_TRUE(recalled.loadPatch(text, error)) << error;
    EXPECT_NEAR(processor.normalizedValue(19), recalled.normalizedValue(19), 1e-7);
    EXPECT_NEAR(processor.normalizedValue(128), recalled.normalizedValue(128), 1e-7);
    EXPECT_TRUE(synth2::isContinuousPatch(text));
}
TEST(PresetFiles, SessionV2AndLegacyV1RestoreTransactionally)
{
    ProcessorBridge p, recalled;
    std::string error;
    ASSERT_TRUE(p.loadPatch(patch, error));
    PresetBrowserState browser;
    browser.capture(p);
    browser.query = "Owned";
    browser.restricted = true;
    browser.importedIds = {"3:a.s"};
    browser.patchName = "Edited";
    p.setNormalizedValue(19, .5);
    auto state = savePresetSession(p, browser);
    PresetBrowserState restored;
    EXPECT_EQ(restorePresetSession(state.data(), state.size(), recalled, restored), state.size());
    EXPECT_EQ(p.saveState(), recalled.saveState());
    EXPECT_EQ(restored.serialize(), browser.serialize());
    EXPECT_TRUE(restored.modified(recalled));
    const auto sound = recalled.saveState();
    const auto view = restored.serialize();
    for (std::size_t length : {0u, 8u, 12u, static_cast<unsigned>(state.size() - 1)})
    {
        EXPECT_EQ(restorePresetSession(state.data(), length, recalled, restored), -1);
        EXPECT_EQ(sound, recalled.saveState());
        EXPECT_EQ(view, restored.serialize());
    }
    auto invalid = state;
    invalid[12] = 0;
    EXPECT_EQ(restorePresetSession(invalid.data(), invalid.size(), recalled, restored), -1);
    EXPECT_EQ(sound, recalled.saveState());
    EXPECT_EQ(view, restored.serialize());
    auto legacy = state;
    legacy[4] = 1;
    uint32_t n = uint32_t(legacy[8]) | (uint32_t(legacy[9]) << 8) | (uint32_t(legacy[10]) << 16) |
                 (uint32_t(legacy[11]) << 24);
    legacy.resize(12 + n);
    EXPECT_EQ(restorePresetSession(legacy.data(), legacy.size(), recalled, restored), legacy.size());
    EXPECT_EQ(sound, recalled.saveState());
    EXPECT_EQ(restored.patchName, p.currentPatchName());
    EXPECT_FALSE(restored.modified(recalled));
}
TEST(PresetFiles, InstancesKeepSeparateBrowserStateAndScanningDoesNotChangeSound)
{
    ProcessorBridge a, b;
    std::string error;
    ASSERT_TRUE(a.loadPatch(patch, error));
    ASSERT_TRUE(b.loadPatch(patch, error));
    PresetBrowserState x, y;
    x.capture(a);
    y.capture(b);
    x.query = "Bass";
    y.query = "Lead";
    a.setNormalizedValue(19, .25);
    EXPECT_TRUE(x.modified(a));
    EXPECT_FALSE(y.modified(b));
    testfiles::Directory d;
    write(d.path / "Gone.sy1");
    PresetFiles library(d.path);
    ASSERT_TRUE(library.scan(error));
    auto before = a.saveState();
    fs::remove(d.path / "Gone.sy1");
    ASSERT_TRUE(library.scan(error));
    EXPECT_TRUE(library.entries().empty());
    EXPECT_EQ(before, a.saveState());
    EXPECT_EQ(y.query, "Lead");
}

TEST(PresetFiles, MultipleSelectedTagsSurviveSessionRecallAndLegacyTagStillLoads) {
    synth2::host::ProcessorBridge source,restored;
    synth2::host::PresetBrowserState browser;browser.capture(source);
    browser.selectedTags={"Pad","FM","My own"};
    const auto bytes=synth2::host::savePresetSession(source,browser);
    synth2::host::PresetBrowserState recalled;
    ASSERT_GT(synth2::host::restorePresetSession(bytes.data(),static_cast<int>(bytes.size()),restored,recalled),0);
    EXPECT_EQ(recalled.selectedTags,browser.selectedTags);
    EXPECT_EQ(recalled.tag,"Pad"); // Compatibility hint for older readers.
    browser.selectedTags.clear();browser.tag="Old tag";
    ASSERT_TRUE(synth2::host::PresetBrowserState::deserialize(browser.serialize(),recalled));
    EXPECT_EQ(recalled.tag,"Old tag");EXPECT_TRUE(recalled.selectedTags.empty());
}
