#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace synth2::host
{
struct LibraryPatch
{
    std::string name;
    std::string text;
    std::string entry;
};

// UI-side, immutable-after-open snapshot. No SDK types or paths are needed by
// the audio engine. ZIP entries are read in memory, never extracted to disk.
class PatchLibrary
{
public:
    static constexpr std::size_t maximumPatchBytes = 4 * 1024 * 1024;
    static constexpr std::size_t maximumBankBytes = 64 * 1024 * 1024;
    static constexpr std::size_t maximumPatches = 65536;

    bool openDirectory(const std::string& utf8Path, std::string& error);
    bool openFile(const std::string& utf8Path, std::string& error);
    const std::string& name() const noexcept { return bankName; }
    const std::vector<LibraryPatch>& patches() const noexcept { return entries; }
    // Case-insensitive title/entry-name search. Results index the full snapshot,
    // so filtering cannot accidentally select another patch.
    std::vector<std::size_t> search(const std::string& query) const;

private:
    bool openZip(const void* bytes, std::size_t size, const std::string& name, std::string& error);
    std::string bankName;
    std::vector<LibraryPatch> entries;
};
}
