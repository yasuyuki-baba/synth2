#include "host/PatchCatalog.h"
#include "host/PatchLibrary.h"
#include "host/ProcessorBridge.h"
#include "support/BankFiles.h"
#include <gtest/gtest.h>
#include <fstream>
#ifndef _WIN32
#include <locale.h>
#ifdef __APPLE__
#include <xlocale.h>
#endif
#endif

namespace
{
namespace fs = std::filesystem;
using namespace synth2::host;
const auto fixtures = fs::path(SYNTH2_SOURCE_DIR) / "tests/fixtures";

std::string bytes(const fs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
std::string utf8(const fs::path& path)
{
    const auto text = path.generic_u8string();
    return {text.begin(), text.end()};
}
class ArchiveBankTest : public testing::TestWithParam<const char*> {};
}

TEST_P(ArchiveBankTest, ReadsCompressedPatchesWithOriginalBytesNaturalOrderAndSoundState)
{
#ifndef _WIN32
    const auto previousLocale = uselocale(nullptr);
#endif
    PatchLibrary bank;
    std::string error;
    ASSERT_TRUE(bank.openFile(utf8(fixtures / GetParam()), error)) << error;
#ifndef _WIN32
    EXPECT_EQ(uselocale(nullptr), previousLocale);
#endif
    ASSERT_EQ(bank.patches().size(), 3u);
    EXPECT_EQ(bank.patches()[0].entry, "bank/2.sy1");
    EXPECT_EQ(bank.patches()[1].entry, "bank/10.SY1");
    EXPECT_EQ(bank.patches()[2].entry, "bank/日本語.sy1");
    for(const auto& patch : bank.patches())
    {
        const auto original = bytes(fixtures / "rar/input" / fs::path(std::u8string(patch.entry.begin(), patch.entry.end())));
        EXPECT_EQ(patch.text, original);
        ProcessorBridge direct, archived;
        ASSERT_TRUE(direct.loadPatch(original, error)) << error;
        ASSERT_TRUE(archived.loadPatch(patch.text, error)) << error;
        EXPECT_EQ(archived.saveState(), direct.saveState());
    }
}
INSTANTIATE_TEST_SUITE_P(Formats, ArchiveBankTest, testing::Values(
    "rar/compressed-rar4.rar", "rar/stored-rar4.rar", "rar/compressed-rar5.rar", "rar/solid-rar5.rar",
    "7z/lzma.7z", "7z/lzma2.7z", "7z/solid.7z", "7z/stored.7z", "7z/ppmd.7z"));

TEST(ArchiveBank, RegistrationRetainsOneArchiveAndCatalogHierarchyAcrossReload)
{
    for(const auto* input : {"rar/solid-rar5.rar", "7z/solid.7z"})
    {
        SCOPED_TRACE(input);
        testfiles::Directory source, destination;
        const auto filename = fs::path(input).extension() == ".rar" ? "Collection.RAR" : "Collection.7Z";
        const auto rar = source.path / filename;
        fs::copy_file(fixtures / input, rar);
        PatchCatalog catalog(destination.path);
        ImportResult imported;
        std::string error;
        ASSERT_TRUE(catalog.importFile(utf8(source.path), imported, error)) << error;
        EXPECT_EQ(imported.added, 3u);
        const auto copy = destination.path / source.path.filename() / filename;
        EXPECT_EQ(bytes(copy), bytes(rar));
        for(const auto& entry : fs::recursive_directory_iterator(destination.path))
            EXPECT_NE(entry.path().extension(), ".sy1");
        const auto bankId = utf8(source.path.filename()) + "/" + filename + "/bank";
        EXPECT_EQ(catalog.search({"", bankId}).size(), 3u);
        PresetFiles files(destination.path);
        ASSERT_TRUE(files.scan(error)) << error;
        ASSERT_EQ(files.entries().size(), 3u);
        for(std::size_t i = 0; i < files.entries().size(); ++i)
        {
            const auto& patch = files.entries()[i];
            EXPECT_TRUE(patch.error.empty()) << patch.error;
            EXPECT_FALSE(patch.archiveEntry.empty());
            EXPECT_EQ(patch.program, i);
            EXPECT_EQ(patch.location, utf8(source.path.filename()) + "/" + filename + " / " + patch.archiveEntry);
        }
        const auto previous = catalog.patches();
        fs::remove_all(source.path);
        ASSERT_TRUE(catalog.reload(error)) << error;
        ASSERT_EQ(catalog.patches().size(), previous.size());
        for(std::size_t i = 0; i < previous.size(); ++i)
        {
            EXPECT_EQ(catalog.patches()[i].id, previous[i].id);
            EXPECT_EQ(catalog.patches()[i].program, previous[i].program);
            EXPECT_EQ(catalog.patches()[i].patch.text, previous[i].patch.text);
        }
    }
}

TEST(RarBank, FailedReadsKeepPreviousSnapshotAndInvalidSourcesStayVisible)
{
    testfiles::Directory directory;
    PatchLibrary bank;
    std::string error;
    ASSERT_TRUE(bank.openFile(utf8(fixtures / "rar/compressed-rar5.rar"), error)) << error;
    const auto previousName = bank.name(), previousText = bank.patches().front().text;
    for(const auto* name : {"encrypted-rar5.rar", "oversize-rar5.rar", "solid-rar4.rar",
                            "empty-patch.rar", "no-patches.rar", "oversize-skipped.rar"})
    {
        SCOPED_TRACE(name);
        const auto path = directory.path / name;
        fs::copy_file(fixtures / "rar" / name, path);
        EXPECT_FALSE(bank.openFile(utf8(path), error));
        EXPECT_FALSE(error.empty());
        EXPECT_EQ(bank.name(), previousName);
        ASSERT_EQ(bank.patches().size(), 3u);
        EXPECT_EQ(bank.patches().front().text, previousText);
    }
    for(const auto* name : {"truncated.rar", "checksum.rar", "not-rar.rar"})
    {
        auto data = bytes(fixtures / "rar/stored-rar4.rar");
        if(std::string(name) == "truncated.rar") data.resize(data.size() / 2);
        else if(std::string(name) == "checksum.rar")
        {
            const auto position = data.find("Bright lead");
            ASSERT_NE(position, std::string::npos);
            data[position] ^= 1;
        }
        else data = "Not a RAR archive";
        const auto path = directory.path / name;
        std::ofstream(path, std::ios::binary).write(data.data(), data.size());
        SCOPED_TRACE(name);
        EXPECT_FALSE(bank.openFile(utf8(path), error));
        EXPECT_FALSE(error.empty());
        EXPECT_EQ(bank.name(), previousName);
        EXPECT_EQ(bank.patches().front().text, previousText);
    }
    PresetFiles files(directory.path);
    ASSERT_TRUE(files.scan(error)) << error;
    ASSERT_EQ(files.entries().size(), 9u);
    for(const auto& patch : files.entries()) EXPECT_FALSE(patch.error.empty()) << patch.location;
}

TEST(SevenZipBank, FailedReadsKeepPreviousSnapshotAndInvalidSourcesStayVisible)
{
    testfiles::Directory directory;
    PatchLibrary bank;
    std::string error;
    ASSERT_TRUE(bank.openFile(utf8(fixtures / "7z/solid.7z"), error)) << error;
    const auto previousName = bank.name(), previousText = bank.patches().front().text;
    for(const auto* name : {"encrypted-headers.7z", "encrypted-data.7z",
                           "oversize-patch.7z", "oversize-skipped.7z", "empty-patch.7z",
                           "no-patches.7z", "unsupported-bzip2.7z", "unsupported-deflate.7z",
                           "large-dictionary.7z", "too-many-entries.7z"})
        fs::copy_file(fixtures / "7z" / name, directory.path / name);
    for(const auto* name : {"truncated.7z", "checksum.7z", "not-7z.7z"})
    {
        auto data = bytes(fixtures / "7z/stored.7z");
        if(std::string(name) == "truncated.7z") data.resize(data.size() / 2);
        else if(std::string(name) == "checksum.7z")
        {
            const auto position = data.find("Bright lead");
            ASSERT_NE(position, std::string::npos);
            data[position] ^= 1;
        }
        else data = "Not a 7z archive";
        std::ofstream(directory.path / name, std::ios::binary).write(data.data(), data.size());
    }
    for(const auto& entry : fs::directory_iterator(directory.path))
    {
        SCOPED_TRACE(entry.path().filename());
        EXPECT_FALSE(bank.openFile(utf8(entry.path()), error));
        EXPECT_FALSE(error.empty());
        EXPECT_EQ(bank.name(), previousName);
        ASSERT_EQ(bank.patches().size(), 3u);
        EXPECT_EQ(bank.patches().front().text, previousText);
    }
    PresetFiles files(directory.path);
    ASSERT_TRUE(files.scan(error)) << error;
    ASSERT_EQ(files.entries().size(), 13u);
    for(const auto& patch : files.entries()) EXPECT_FALSE(patch.error.empty()) << patch.location;
}
