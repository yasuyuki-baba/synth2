#pragma once
#include "ProcessorBridge.h"
#include <array>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace synth2::host
{
struct UserPreset
{
    std::string id, name, location, archiveEntry, text, error;
    std::filesystem::path file;
    int program = 0;
};
enum class FileConflict
{
    replace,
    keepBoth,
    skip
};
struct ConflictChoice
{
    FileConflict action = FileConflict::keepBoth;
    bool applyToAll = false;
};
using ResolveConflict = std::function<ConflictChoice(const std::filesystem::path &)>;
struct PresetImportResult
{
    std::vector<std::string> ids;
    std::vector<std::string> errors;
    std::size_t imported = 0, skipped = 0;
    bool catalogReady = false;
};
// File operations are non-RT. Archives remain intact and are never extracted.
class PresetFiles
{
  public:
    explicit PresetFiles(std::filesystem::path root);
    bool scan(std::string &error, bool validateSources = true);
    PresetImportResult import(const std::filesystem::path &source, const ResolveConflict &resolve);
    bool save(const std::filesystem::path &relative, const std::string &text, bool replace, std::string &id,
              std::string &error);
    const std::vector<UserPreset> &entries() const { return items; }
    const std::filesystem::path &directory() const { return root; }
    // Recognize only actual legacy records, not folders merely named banks/tags/favorites.
    static bool isManagedBank(const std::filesystem::path& root, const std::filesystem::path& bank);
    static bool isManagedPatch(const std::filesystem::path& root, const std::filesystem::path& file);
    static std::filesystem::path defaultDirectory();
    static std::filesystem::path settingsDirectory();
    static std::filesystem::path configuredDirectory();
    static bool saveStandaloneState(const std::vector<uint8_t> &, std::string &error);
    static std::vector<uint8_t> loadStandaloneState();
    static bool configureDirectory(const std::filesystem::path &, std::string &error);

  private:
    std::filesystem::path root;
    std::vector<UserPreset> items;
};
// Instance-owned state; paths are relative to the independently configured root.
struct PresetBrowserState
{
    std::string query, selectedId, patchName = "Init", bankId, tag;
    bool favoritesOnly = false;
    bool restricted = false;
    std::vector<std::string> importedIds;
    std::vector<std::string> selectedTags;
    int scroll = 0, page = 0;
    std::array<double, ProcessorBridge::parameterCount> baseline{};
    void capture(const ProcessorBridge &);
    bool modified(const ProcessorBridge &) const;
    std::string serialize() const;
    static bool deserialize(const std::string &, PresetBrowserState &);
};
std::vector<uint8_t> savePresetSession(const ProcessorBridge &, const PresetBrowserState &);
// Accepts legacy wrapper v1; rejects the entire transaction on malformed input.
int restorePresetSession(const void *, int size, ProcessorBridge &, PresetBrowserState &);
} // namespace synth2::host
