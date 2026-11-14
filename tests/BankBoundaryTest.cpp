#include <gtest/gtest.h>
#include "support/BankFiles.h"
#include "host/PatchLibrary.h"
#include "host/ProcessorBridge.h"
#include <algorithm>
#include <cstdint>
#include <iterator>

namespace
{
using synth2::host::PatchLibrary;
const std::string patch="Owned boundary\nver=113\n19,91\n29,73\n";
bool repeatedZip(const std::filesystem::path& path,size_t count,const std::string& payload,bool extraByte=false)
{
    mz_zip_archive zip{};
    if(!mz_zip_writer_init_file(&zip,path.string().c_str(),0))return false;
    bool ok=true;
    for(size_t i=0;i<count;++i)
        ok=ok && mz_zip_writer_add_mem(&zip,(std::to_string(i)+".sy1").c_str(),payload.data(),payload.size(),MZ_BEST_COMPRESSION);
    if(extraByte)ok=ok && mz_zip_writer_add_mem(&zip,"extra.sy1","x",1,MZ_BEST_COMPRESSION);
    ok=ok && mz_zip_writer_finalize_archive(&zip);mz_zip_writer_end(&zip);return ok;
}
std::vector<uint8_t> read(const std::filesystem::path& path)
{
    std::ifstream stream(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
}
bool write(const std::filesystem::path& path,const std::vector<uint8_t>& bytes)
{
    std::ofstream stream(path,std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(stream);
}
uint32_t word(const std::vector<uint8_t>& bytes,size_t at,int count)
{
    if(at+count>bytes.size())throw std::runtime_error("Invalid generated ZIP offset");
    uint32_t result=0;for(int i=0;i<count;++i)result|=uint32_t(bytes[at+i])<<(8*i);return result;
}
void makeSymlink(std::vector<uint8_t>& bytes,const std::string& name)
{
    // Mutate only the generated central-directory Unix file type; no host
    // symlink privileges or filesystem extraction are involved.
    const auto end=bytes.size()-22;
    if(word(bytes,end,4)!=0x06054b50)throw std::runtime_error("Missing generated ZIP footer");
    size_t at=word(bytes,end+16,4);
    const auto count=word(bytes,end+10,2);
    for(unsigned i=0;i<count;++i)
    {
        if(word(bytes,at,4)!=0x02014b50)throw std::runtime_error("Invalid generated central directory");
        const auto length=word(bytes,at+28,2),extra=word(bytes,at+30,2),comment=word(bytes,at+32,2);
        const std::string filename(bytes.begin()+at+46,bytes.begin()+at+46+length);
        if(filename==name)
        {
            bytes[at+5]=3; // Unix creator OS.
            const uint32_t attributes=uint32_t{0120777}<<16;
            for(int j=0;j<4;++j)bytes[at+38+j]=static_cast<uint8_t>(attributes>>(8*j));
            return;
        }
        at+=46+length+extra+comment;
    }
    throw std::runtime_error("Missing generated link entry");
}
}

TEST(BankBoundary, ExactEntryLimitLoadsAndOneExtraPreservesTheFullSnapshot)
{
    testfiles::Directory directory;
    const auto exact=directory.path/"exact.zip",overflow=directory.path/"overflow.zip";
    ASSERT_TRUE(repeatedZip(exact,PatchLibrary::maximumPatches,patch));
    ASSERT_TRUE(repeatedZip(overflow,PatchLibrary::maximumPatches+1,patch));
    PatchLibrary library;std::string error;
    ASSERT_TRUE(library.openFile(exact.string(),error))<<error;
    ASSERT_EQ(library.patches().size(),PatchLibrary::maximumPatches);
    const auto name=library.patches().front().name;
    EXPECT_FALSE(library.openFile(overflow.string(),error)); EXPECT_FALSE(error.empty());
    ASSERT_EQ(library.patches().size(),PatchLibrary::maximumPatches); EXPECT_EQ(library.patches().front().name,name);
    for(size_t i=0;i<library.patches().size();++i)
    {
        ASSERT_EQ(library.patches()[i].text,patch);
        ASSERT_TRUE(library.patches()[i].name.starts_with(std::to_string(i)+".sy1  -  "));
    }
}

TEST(BankBoundary, LargeNestedCollectionIgnoresMacMetadataAndRecallsLateSelection)
{
    testfiles::Directory directory;const auto file=directory.path/"collection.zip";
    mz_zip_archive zip{};ASSERT_TRUE(mz_zip_writer_init_file(&zip,file.string().c_str(),0));
    for(size_t i=0;i<25000;++i)
    {
        const auto name="Patches/Bank"+std::to_string(i/128)+"/"+std::to_string(i)+".sy1";
        ASSERT_TRUE(mz_zip_writer_add_mem(&zip,name.c_str(),patch.data(),patch.size(),MZ_BEST_COMPRESSION));
    }
    // Resource forks are not patches, even when empty or under nested paths.
    for(const char* name:{"__MACOSX/Patches/._0.sy1","Patches/Bank0/._0.sy1",
                          "Patches/__MACOSX/fake.sy1","Patches\\__MACOSX\\fake.sy1"})
        ASSERT_TRUE(mz_zip_writer_add_mem(&zip,name,"",0,MZ_BEST_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_finalize_archive(&zip));ASSERT_TRUE(mz_zip_writer_end(&zip));
    PatchLibrary library;std::string error;
    ASSERT_TRUE(library.openFile(file.string(),error))<<error;
    ASSERT_EQ(library.patches().size(),25000u);
    EXPECT_EQ(library.patches().back().name,"Patches/Bank195/24999.sy1  -  Owned boundary");
    synth2::host::ProcessorBridge selected,recalled;
    ASSERT_TRUE(selected.loadProgram(library.patches()[24999].text,24999,error))<<error;
    EXPECT_EQ(selected.currentProgram(),24999);
    const auto state=selected.saveState();ASSERT_TRUE(std::filesystem::remove(file));
    recalled.restoreState(state.data(),static_cast<int>(state.size()));
    EXPECT_EQ(recalled.currentProgram(),24999);EXPECT_EQ(recalled.saveState(),state);
}

TEST(BankBoundary, ExactExpandedByteLimitsLoadAndOneExtraByteIsRejectedAtomically)
{
    testfiles::Directory directory;
    const auto exact=directory.path/"exact.zip",overflow=directory.path/"overflow.zip";
    std::string large=patch+"#";
    large.resize(PatchLibrary::maximumPatchBytes,'x');
    const auto count=PatchLibrary::maximumBankBytes/PatchLibrary::maximumPatchBytes;
    ASSERT_TRUE(repeatedZip(exact,count,large));
    ASSERT_TRUE(repeatedZip(overflow,count,large,true));
    PatchLibrary library;std::string error;
    ASSERT_TRUE(library.openFile(exact.string(),error))<<error;
    ASSERT_EQ(library.patches().size(),count);
    EXPECT_FALSE(library.openFile(overflow.string(),error)); EXPECT_FALSE(error.empty());
    ASSERT_EQ(library.patches().size(),count); EXPECT_TRUE(library.patches().front().name.starts_with("0.sy1"));
    for(const auto& entry:library.patches()) ASSERT_EQ(entry.text,large);
}

TEST(BankBoundary, CorruptPayloadCrcRejectsTheBankWithoutReplacingThePreviousSelection)
{
    testfiles::Directory directory;
    const auto initial=directory.path/"initial.zip",corrupt=directory.path/"corrupt.zip";
    ASSERT_TRUE(testfiles::zip(initial,{{"seed.sy1",patch}}));
    const std::string target="CRC target patch\nver=113\n19,33\n";
    mz_zip_archive zip{};ASSERT_TRUE(mz_zip_writer_init_file(&zip,corrupt.string().c_str(),0));
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip,"first.sy1",patch.data(),patch.size(),MZ_NO_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_add_mem(&zip,"target.sy1",target.data(),target.size(),MZ_NO_COMPRESSION));
    ASSERT_TRUE(mz_zip_writer_finalize_archive(&zip));ASSERT_TRUE(mz_zip_writer_end(&zip));
    auto bytes=read(corrupt);
    const auto found=std::search(bytes.begin(),bytes.end(),target.begin(),target.end());
    ASSERT_NE(found,bytes.end()); *found^=1;
    ASSERT_TRUE(write(corrupt,bytes));
    PatchLibrary library;std::string error;
    ASSERT_TRUE(library.openFile(initial.string(),error));
    EXPECT_FALSE(library.openFile(corrupt.string(),error)); EXPECT_FALSE(error.empty());
    ASSERT_EQ(library.patches().size(),1u); EXPECT_EQ(library.patches().front().entry,"seed.sy1");
    EXPECT_EQ(library.patches()[0].text,patch);
}

TEST(BankBoundary, UnixSymlinkEntriesAreIgnoredAndLinkOnlyBanksPreserveTheSnapshot)
{
    testfiles::Directory directory;
    const auto mixed=directory.path/"mixed.zip",only=directory.path/"only.zip";
    ASSERT_TRUE(testfiles::zip(mixed,{{"real.sy1",patch},{"link.sy1","target.sy1"}}));
    ASSERT_TRUE(testfiles::zip(only,{{"link.sy1","target.sy1"}}));
    for(const auto& path:{mixed,only})
    { auto bytes=read(path);makeSymlink(bytes,"link.sy1");ASSERT_TRUE(write(path,bytes)); }
    PatchLibrary library;std::string error;
    ASSERT_TRUE(library.openFile(mixed.string(),error))<<error;
    ASSERT_EQ(library.patches().size(),1u);EXPECT_TRUE(library.patches()[0].name.starts_with("real.sy1"));
    EXPECT_FALSE(library.openFile(only.string(),error)); EXPECT_FALSE(error.empty());
    ASSERT_EQ(library.patches().size(),1u); EXPECT_EQ(library.patches().front().entry,"real.sy1");
    EXPECT_EQ(library.patches()[0].text,patch);
    EXPECT_FALSE(std::filesystem::exists(directory.path/"target.sy1"));
}
