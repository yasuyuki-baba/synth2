#include "PatchLibrary.h"
#include "LibraryFiles.h"
#include "patch/PatchLoader.h"
#include "patch/PatchFormat.h"
#include <miniz.h>
#include <archive.h>
#include <archive_entry.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <memory>
#include <optional>
#ifndef _WIN32
#include <locale.h>
#ifdef __APPLE__
#include <xlocale.h>
#endif
#endif

namespace synth2::host
{
namespace
{
#ifndef _WIN32
// libarchive's RAR4 pathname conversion uses the current locale. A plugin
// must not change its host's process locale; use a scoped thread locale.
struct ArchiveLocale
{
    ArchiveLocale() = default;
    ArchiveLocale(const ArchiveLocale&) = delete;
    ArchiveLocale& operator=(const ArchiveLocale&) = delete;
#ifdef __APPLE__
    locale_t locale=newlocale(LC_CTYPE_MASK,"en_US.UTF-8",nullptr);
#else
    locale_t locale=newlocale(LC_CTYPE_MASK,"C.UTF-8",nullptr);
#endif
    locale_t previous=locale ? uselocale(locale) : nullptr;
    ~ArchiveLocale(){if(previous)uselocale(previous);if(locale)freelocale(locale);}
};
#endif
std::string lower(std::string s){for(auto& c:s)if(static_cast<unsigned char>(c)<128)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
bool isMacMetadata(const std::string& filename)
{
    for(size_t start=0;start<filename.size();)
    {
        const auto end=filename.find_first_of("/\\",start);
        const auto component=filename.substr(start,end==std::string::npos ? end : end-start);
        if(component=="__MACOSX" || component.starts_with("._"))return true;
        if(end==std::string::npos)break;
        start=end+1;
    }
    return false;
}
std::string title(const std::string& text,const std::string& filename)
{
    if(isContinuousPatch(text))
    {
        try {return filename+"  -  "+parseContinuousPatch(text).name;}
        catch(const std::exception&) {return filename;}
    }
    std::istringstream lines(text);std::string line;
    while(std::getline(lines,line))
    {
        line=trimPatchLine(line);if(line.empty())continue;
        if(hasPatchTitle(text))return filename+"  -  "+line;
        break;
    }
    return filename;
}
bool isArchivedPatch(const std::string& name, bool regular)
{
    return regular && !isMacMetadata(name) && name.size() >= 4 &&
           lower(name.substr(name.size() - 4)) == ".sy1";
}

// Backends account declared sizes before allocating or decompressing patch
// data. ZIP can preflight its entire index; solid readers advance sequentially.
// Both feed the same snapshot builder and publish only after complete success.
class PatchBankBuilder
{
public:
    bool accountPatch(std::uint64_t bytes, std::string& error)
    {
        if(bytes == 0 || bytes > PatchLibrary::maximumPatchBytes ||
           bytes > PatchLibrary::maximumBankBytes - patchBytes ||
           patchCount == PatchLibrary::maximumPatches)
        {
            error = "The bank contains an empty patch or exceeds the bank size limits.";
            return false;
        }
        patchBytes += static_cast<std::size_t>(bytes);
        ++patchCount;
        return true;
    }

    void append(const std::string& filename, std::string text, bool archived)
    {
        auto name = title(text, filename);
        patches.push_back({std::move(name), std::move(text), archived ? filename : std::string{}});
    }

    bool finish(std::vector<LibraryPatch>& result, std::string& error)
    {
        if(patches.empty())
        {
            error = "No .sy1 patches were found in the selected bank.";
            return false;
        }
        std::stable_sort(patches.begin(), patches.end(), [](const auto& a, const auto& b) {
            return naturalLibraryPathLess(a.entry, b.entry);
        });
        result.swap(patches);
        return true;
    }

private:
    std::vector<LibraryPatch> patches;
    std::size_t patchCount = 0, patchBytes = 0;
};

bool readZip(const void* bytes, std::size_t size, PatchBankBuilder& bank, std::string& error)
{
    mz_zip_archive zip{};
    if(!mz_zip_reader_init_mem(&zip, bytes, size, 0))
    {
        error = "The ZIP bank could not be read.";
        return false;
    }
    struct Close
    {
        mz_zip_archive& zip;
        ~Close() { mz_zip_reader_end(&zip); }
    } close{zip};
    struct Entry
    {
        mz_uint index;
        std::string name;
        std::size_t size;
    };
    std::vector<Entry> entries;
    for(mz_uint i = 0; i < mz_zip_reader_get_num_files(&zip); ++i)
    {
        mz_zip_archive_file_stat entry{};
        if(!mz_zip_reader_file_stat(&zip, i, &entry))
        {
            error = "The ZIP directory is invalid.";
            return false;
        }
        const auto length = mz_zip_reader_get_filename(&zip, i, nullptr, 0);
        if(length == 0)
        {
            error = "The ZIP filename is invalid.";
            return false;
        }
        std::string filename(length, '\0');
        if(mz_zip_reader_get_filename(&zip, i, filename.data(), length) != length)
        {
            error = "The ZIP filename is invalid.";
            return false;
        }
        filename.resize(length - 1);
        const auto mode = (entry.m_external_attr >> 16) & 0170000;
        if(!isArchivedPatch(filename, !entry.m_is_directory && mode != 0120000)) continue;
        if(!bank.accountPatch(entry.m_uncomp_size, error)) return false;
        entries.push_back({i, std::move(filename), static_cast<std::size_t>(entry.m_uncomp_size)});
    }
    for(const auto& entry : entries)
    {
        std::string data(entry.size, '\0');
        if(!mz_zip_reader_extract_to_mem(&zip, entry.index, data.data(), data.size(), 0))
        {
            error = "A patch in the bank could not be read completely.";
            return false;
        }
        bank.append(entry.name, std::move(data), true);
    }
    return true;
}

bool readArchive(const void* bytes, std::size_t size, PatchBankBuilder& bank, std::string& error)
{
    std::unique_ptr<archive, decltype(&archive_read_free)> reader(archive_read_new(), archive_read_free);
    const auto fail = [&](const char* message) {
        error = message;
        if(reader && archive_error_string(reader.get()))
            error += " " + std::string(archive_error_string(reader.get()));
        return false;
    };
    if(!reader || archive_read_support_format_rar(reader.get()) != ARCHIVE_OK ||
       archive_read_support_format_rar5(reader.get()) != ARCHIVE_OK ||
       archive_read_support_format_7zip(reader.get()) != ARCHIVE_OK ||
       archive_read_open_memory(reader.get(), bytes, size) != ARCHIVE_OK)
        return fail("The archive bank could not be read.");

    std::size_t total = 0, count = 0;
    archive_entry* entry = nullptr;
    int status;
    while((status = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK)
    {
        if(++count > PatchLibrary::maximumPatches)
        {
            error = "The archive bank contains too many entries.";
            return false;
        }
        if(archive_entry_is_encrypted(entry) > 0)
        {
            error = "Password-protected archive banks are not supported.";
            return false;
        }
        const auto* path = archive_entry_pathname_utf8(entry);
        if(!path) return fail("The archive filename could not be read as UTF-8.");
        const std::string filename(path);
        const auto length = archive_entry_size(entry);
        // Skipping solid data may decompress it for the following entry's
        // dictionary. This backend must also bound ignored non-patch data.
        if(!archive_entry_size_is_set(entry) || length < 0 ||
           static_cast<std::uint64_t>(length) > PatchLibrary::maximumBankBytes - total)
        {
            error = "The archive bank exceeds the bank size limit.";
            return false;
        }
        total += static_cast<std::size_t>(length);
        const bool regular = archive_entry_filetype(entry) == AE_IFREG &&
                             !archive_entry_symlink(entry) && !archive_entry_hardlink(entry);
        if(!isArchivedPatch(filename, regular))
        {
            if(archive_read_data_skip(reader.get()) != ARCHIVE_OK)
                return fail("An entry in the archive bank could not be skipped.");
            continue;
        }
        if(!bank.accountPatch(static_cast<std::uint64_t>(length), error)) return false;
        std::string data(static_cast<std::size_t>(length), '\0');
        std::size_t offset = 0;
        while(offset < data.size())
        {
            const auto n = archive_read_data(reader.get(), data.data() + offset, data.size() - offset);
            if(n <= 0) return fail("A patch in the archive bank could not be read completely.");
            offset += static_cast<std::size_t>(n);
        }
        // Read through EOF: the decoder verifies the entry's checksum here.
        char extra;
        if(archive_read_data(reader.get(), &extra, 1) != 0)
            return fail("A patch in the archive bank failed size or checksum validation.");
        bank.append(filename, std::move(data), true);
    }
    if(status != ARCHIVE_EOF) return fail("The archive directory is invalid or incomplete.");
    return true;
}
}

bool PatchLibrary::openFile(const std::string& path, std::string& error)
{
    try
    {
        const auto file = std::filesystem::path(std::u8string(path.begin(), path.end()));
        const auto kind = libraryFileKind(file);
        if(kind == LibraryFileKind::unsupported)
        {
            error = "Choose a .sy1 patch, ZIP, RAR or 7z bank.";
            return false;
        }
        const auto size = std::filesystem::file_size(file);
        const auto limit = kind == LibraryFileKind::patch ? maximumPatchBytes : maximumBankBytes;
        if(size == 0 || size > limit)
        {
            error = "The selected file is empty or exceeds the bank size limit.";
            return false;
        }
        std::ifstream input(file, std::ios::binary);
        std::string data(static_cast<std::size_t>(size), '\0');
        if(!input.read(data.data(), static_cast<std::streamsize>(size)) ||
           input.peek() != std::char_traits<char>::eof())
        {
            error = "The selected file could not be read completely.";
            return false;
        }
        const auto filename = file.filename().u8string();
        std::string name(filename.begin(), filename.end());
#ifndef _WIN32
        // Keep pathname conversion and natural ordering in the same thread
        // locale for libarchive, restoring the host's locale on every exit.
        std::optional<ArchiveLocale> textLocale;
        if(kind == LibraryFileKind::rar || kind == LibraryFileKind::sevenZip)
        {
            textLocale.emplace();
            if(!textLocale->previous)
            {
                error = "A UTF-8 thread locale is required to read archive filenames.";
                return false;
            }
        }
#endif
        PatchBankBuilder bank;
        switch(kind)
        {
        case LibraryFileKind::patch:
            if(!bank.accountPatch(size, error)) return false;
            bank.append(name, std::move(data), false);
            break;
        case LibraryFileKind::zip:
            if(!readZip(data.data(), data.size(), bank, error)) return false;
            break;
        case LibraryFileKind::rar:
        case LibraryFileKind::sevenZip:
            if(!readArchive(data.data(), data.size(), bank, error)) return false;
            break;
        case LibraryFileKind::unsupported:
            return false; // Rejected before opening the file.
        }
        if(!bank.finish(entries, error)) return false;
        bankName.swap(name);
        error.clear();
        return true;
    }
    catch(const std::exception& e)
    {
        error = e.what();
        return false;
    }
}
std::vector<size_t> PatchLibrary::search(const std::string& query)const
{
    // Whitespace trimming here is independent of .sy1 comment syntax.
    auto term=lower(query);const auto first=term.find_first_not_of(" \t\r\n");
    term=first==std::string::npos ? "" : term.substr(first,term.find_last_not_of(" \t\r\n")-first+1);
    std::vector<size_t> result;
    for(size_t i=0;i<entries.size();++i)if(term.empty() || lower(entries[i].name).find(term)!=std::string::npos)result.push_back(i);
    return result;
}
}
