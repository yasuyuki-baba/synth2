#pragma once
#include <cctype>
#include <filesystem>
#include <string>

namespace synth2::host
{
enum class LibraryFileKind
{
    unsupported,
    patch,
    zip,
    rar,
    sevenZip
};

inline LibraryFileKind libraryFileKind(const std::filesystem::path& path)
{
    const auto bytes = path.extension().u8string();
    std::string extension(bytes.begin(), bytes.end());
    for(auto& c : extension)
        if(static_cast<unsigned char>(c) < 128)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if(extension == ".sy1") return LibraryFileKind::patch;
    if(extension == ".zip") return LibraryFileKind::zip;
    if(extension == ".rar") return LibraryFileKind::rar;
    if(extension == ".7z") return LibraryFileKind::sevenZip;
    return LibraryFileKind::unsupported;
}

inline bool isLibraryArchive(LibraryFileKind kind)
{
    return kind == LibraryFileKind::zip || kind == LibraryFileKind::rar ||
           kind == LibraryFileKind::sevenZip;
}

// Preserve case-insensitive natural path ordering across loose and archived
// sounds, including stable ties between differently padded numeric names.
inline bool naturalLibraryPathLess(const std::string &a, const std::string &b)
{
    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size())
    {
        if (std::isdigit(static_cast<unsigned char>(a[i])) && std::isdigit(static_cast<unsigned char>(b[j])))
        {
            auto x = i, y = j;
            while (i < a.size() && std::isdigit(static_cast<unsigned char>(a[i])))
                ++i;
            while (j < b.size() && std::isdigit(static_cast<unsigned char>(b[j])))
                ++j;
            while (x + 1 < i && a[x] == '0')
                ++x;
            while (y + 1 < j && b[y] == '0')
                ++y;
            if (i - x != j - y)
                return i - x < j - y;
            int n = a.compare(x, i - x, b, y, j - y);
            if (n)
                return n < 0;
        }
        else
        {
            auto x = std::tolower(static_cast<unsigned char>(a[i++])),
                 y = std::tolower(static_cast<unsigned char>(b[j++]));
            if (x != y)
                return x < y;
        }
    }
    return i == a.size() && j != b.size();
}
}
