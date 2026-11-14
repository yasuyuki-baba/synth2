#pragma once
#include "PatchLibrary.h"
#include "PresetFiles.h"
#include <filesystem>
#include <utility>

namespace synth2::host {
struct CatalogPatch {
    std::string id, bankId, bankName;
    LibraryPatch patch;
    int program = 0;
    bool favorite = false;
    std::vector<std::string> tags;
    std::filesystem::path file;
    std::string archiveEntry, error, location;
    std::vector<std::string> automaticTags, hiddenTags;
    std::vector<std::string> visibleAutomaticTags() const;
    std::vector<std::string> allTags() const;

};
struct CatalogBank { std::string id, name; };
struct CatalogFilter {
    std::string query, bankId, tag;
    bool favoritesOnly = false;
    bool registeredOnly = false;
    std::vector<std::string> registeredIds;
    std::vector<std::string> selectedTags; // All selected tags must match.
};
struct TagCacheStats { std::size_t reused=0, rebuilt=0; };

// UI-only persistent library. Filesystem work never enters ProcessorBridge or
// the render callback. Callers supply the platform's user application-data path.
class PatchCatalog {
public:
    explicit PatchCatalog(std::filesystem::path directory = {}) : directory(std::move(directory)) {}
    static constexpr std::size_t maximumCatalogPatches = 100000;
    static constexpr std::size_t maximumCatalogBytes = 512 * 1024 * 1024;
    static std::filesystem::path userDirectory();
    static std::vector<std::string> splitTags(const std::string& text);
    static std::string tagText(const std::vector<std::string>& tags);
    bool reload(std::string& error);
    bool registerFiles(const std::string&, const ResolveConflict&, PresetImportResult&);
    bool saveFile(const std::filesystem::path&, const std::string&, bool replace, std::string& id, std::string& error);
    bool setFavorite(const std::string& id, bool favorite, std::string& error);
    bool setTags(const std::string& id, const std::string& tags, std::string& error);
    bool hideAutomaticTag(const std::string& id,const std::string& tag,std::string& error);
    bool restoreAutomaticTags(const std::string& id,std::string& error);
    const std::vector<CatalogPatch>& patches() const noexcept { return entries; }
    const TagCacheStats& tagCacheStats() const noexcept { return cacheStats; }
    const std::filesystem::path& root() const noexcept { return directory; }
    std::vector<CatalogBank> banks() const;
    std::vector<std::size_t> search(const CatalogFilter& filter) const;
private:
    std::filesystem::path directory;
    std::vector<CatalogPatch> entries;
    TagCacheStats cacheStats;
};
}
