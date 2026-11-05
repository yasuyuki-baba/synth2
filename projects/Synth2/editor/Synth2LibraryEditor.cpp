#include "../Synth2.h"
#include "EditorControls.h"
#include "EditorControlTags.h"
#include "editor/EditorPages.h"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <chrono>
#include "../src/patch/PatchFormat.h"

using namespace iplug;
using namespace iplug::igraphics;
using namespace synth2::editor::native;

void Synth2::refreshBrowser()
{
    auto *graphics = GetUI();
    if (!graphics)
        return;
    if (catalogLoaded && !libraryFilter.bankId.empty() &&
        std::none_of(catalog.patches().begin(), catalog.patches().end(),
                     [&](const auto &p) { return p.bankId == libraryFilter.bankId; }))
        libraryFilter.bankId.clear();
    const auto matches = catalog.search(libraryFilter);
    if (catalogLoaded && resultOffset >= matches.size())
        resultOffset = matches.empty() ? 0 : ((matches.size() - 1) / visibleRows) * visibleRows;
    const auto setText = [&](int tag, const std::string &value)
    {
        if (auto *c = graphics->GetControlWithTag(tag))
        {
            c->As<ITextControl>()->SetStr(value.c_str());
            c->SetDirty(false);
        }
    };
    const auto setButton = [&](int tag, const std::string &value)
    {
        if (auto *c = graphics->GetControlWithTag(tag))
        {
            c->As<IVButtonControl>()->SetLabelStr(value.c_str());
            c->SetDirty(false);
        }
    };
    setText(bankTag, std::string(libraryFilter.registeredOnly ? "Registered / " : "") +
                         std::to_string(catalog.patches().size()) + " sounds in library");
    setButton(presetTag, currentPatch + (browser.modified(processor) ? " *" : ""));
    setText(statusTag,
            browserStatus.empty()
                ? (catalog.patches().empty() ? "No sounds yet. Use Add sounds... to import a bank or patch."
                                             : std::to_string(matches.size()) + " matching sounds")
                : browserStatus);
    setText(searchTag, libraryFilter.query);
    setText(librarySearchTag, libraryFilter.query);
    std::string bankLabel = "All banks";
    for (const auto &patch : catalog.patches())
        if (patch.bankId == libraryFilter.bankId)
        {
            bankLabel = patch.bankName;
            break;
        }
    setButton(bankFilterTag, bankLabel);
    setButton(tagFilterTag, libraryFilter.tag.empty() ? "All tags" : libraryFilter.tag);
    setButton(favoritesTag, libraryFilter.favoritesOnly ? "Favorites: on" : "Favorites: off");
    if (auto *c = graphics->GetControlWithTag(libraryListTag))
        c->As<CatalogListControl>()->setResults(matches, resultOffset);
    const auto selected = std::find_if(catalog.patches().begin(), catalog.patches().end(),
                                       [&](const auto &p) { return p.id == selectedPatchId; });
    const bool canEdit = selected != catalog.patches().end();
    setText(selectedNameTag,
            canEdit ? "Tags for " + selected->patch.name + " (comma-separated; press Enter to save)"
                    : "Select a sound to edit its tags");
    setText(tagsEditTag, canEdit ? synth2::host::PatchCatalog::tagText(selected->tags) : "");
    if (auto *c = graphics->GetControlWithTag(tagsEditTag))
        c->SetDisabled(!canEdit);
    if (auto *c = graphics->GetControlWithTag(4016))
        c->SetDisabled(libraryFilter.bankId.empty());
    const auto first = matches.empty() ? 0 : resultOffset + 1;
    setText(resultsTag, std::to_string(first) + " - " +
                            std::to_string(std::min(resultOffset + visibleRows, matches.size())) + " of " +
                            std::to_string(matches.size()) + " sounds  /  + marks a favorite");
}

void Synth2::changeFilter(const char *query)
{
    libraryFilter.query = query;
    resultOffset = 0;
    browserStatus.clear();
    showPage(synth2::editor::editorLibraryView);
    refreshBrowser();
}

void Synth2::showPresetMenu(IControl *) { showPage(synth2::editor::editorLibraryView); }

void Synth2::showBankMenu(IControl *caller)
{
    std::vector<std::pair<std::string, std::string>> banks;
    std::set<std::string> seen;
    for (const auto &p : catalog.patches())
        if (seen.insert(p.bankId).second)
            banks.push_back({p.bankId, p.bankName});
    std::stable_sort(banks.begin(), banks.end(),
                     [](const auto &a, const auto &b) { return a.second < b.second; });
    presetMenu = std::make_unique<IPopupMenu>("Banks");
    presetMenu->AddItem("All banks");
    for (const auto &bank : banks)
        presetMenu->AddItem(bank.second.c_str());
    presetMenu->SetFunction(
        [this, banks](IPopupMenu *menu)
        {
            const int index = menu->GetChosenItemIdx();
            if (index < 0 || index > static_cast<int>(banks.size()))
                return;
            libraryFilter.registeredOnly = false;
            libraryFilter.registeredIds.clear();
            libraryFilter.bankId = index == 0 ? "" : banks[index - 1].first;
            resultOffset = 0;
            browserStatus.clear();
            refreshBrowser();
        });
    caller->GetUI()->CreatePopupMenu(*caller, *presetMenu, caller->GetRECT());
}
void Synth2::showTagMenu(IControl *caller)
{
    std::set<std::string> unique;
    auto scoped = libraryFilter;
    scoped.tag.clear();
    scoped.query.clear();
    for (auto i : catalog.search(scoped))
        for (const auto &tag : catalog.patches()[i].tags)
            unique.insert(tag);
    const std::vector<std::string> tags(unique.begin(), unique.end());
    presetMenu = std::make_unique<IPopupMenu>("Tags");
    presetMenu->AddItem("All tags");
    for (const auto &tag : tags)
        presetMenu->AddItem(tag.c_str());
    presetMenu->SetFunction(
        [this, tags](IPopupMenu *menu)
        {
            const int index = menu->GetChosenItemIdx();
            if (index < 0 || index > static_cast<int>(tags.size()))
                return;
            libraryFilter.tag = index == 0 ? "" : tags[index - 1];
            resultOffset = 0;
            browserStatus.clear();
            refreshBrowser();
        });
    caller->GetUI()->CreatePopupMenu(*caller, *presetMenu, caller->GetRECT());
}
void Synth2::stepPatch(int direction)
{
    const auto matches = catalog.search(libraryFilter);
    if (matches.empty())
        return;
    const auto found = std::find_if(matches.begin(), matches.end(),
                                    [&](auto i) { return catalog.patches()[i].id == selectedPatchId; });
    const auto next =
        found == matches.end()
            ? (direction > 0 ? 0 : static_cast<int>(matches.size()) - 1)
            : (static_cast<int>(found - matches.begin()) + direction + static_cast<int>(matches.size())) %
                  static_cast<int>(matches.size());
    resultOffset = (next / visibleRows) * visibleRows;
    loadLibraryPatch(matches[next]);
}

void Synth2::notifyHostOfPatch()
{
    syncParametersFromProcessor();
    // Reflect the transaction to the host and editor without invoking
    // SetParameterValue/OnParamChange for every already-loaded source word.
    for (int index = 0; index < NParams(); ++index)
    {
        BeginInformHostOfParamChange(index);
        InformHostOfParamChange(index, GetParam(index)->GetNormalized());
        EndInformHostOfParamChange(index);
    }
    SendCurrentParamValuesFromDelegate();
}

void Synth2::loadLibraryPatch(std::size_t index)
{
    if (index >= catalog.patches().size() || libraryJob.valid())
        return;
    const auto &item = catalog.patches()[index];
    std::string error;
    if (!item.error.empty())
    {
        browserStatus = item.error;
        refreshBrowser();
        return;
    }
    if (!processor.loadProgram(item.patch.text, item.program, error, item.patch.name))
    {
        browserStatus = error;
        refreshBrowser();
        return;
    }
    selectedPatchId = item.id;
    currentPatch = item.patch.name;
    browserStatus.clear();
    stateRestored.store(false);
    browser.capture(processor);
    notifyHostOfPatch();
    refreshBrowser();
}
void Synth2::reloadCatalog()
{
    if (libraryJob.valid())
        return;
    auto root = catalog.root();
    libraryJob = std::async(std::launch::async,
                            [root]
                            {
                                LibraryJob job{synth2::host::PatchCatalog(root)};
                                job.catalog.reload(job.error);
                                return job;
                            });
    browserStatus = "Scanning library...";
    refreshBrowser();
}
void Synth2::importBank(const std::string &path)
{
    if (libraryJob.valid())
    {
        browserStatus = "Please wait for the current library operation.";
        refreshBrowser();
        return;
    }
    importPrompt = std::make_shared<ImportPrompt>();
    auto prompt = importPrompt;
    auto root = catalog.root();
    auto generation = browserGeneration.load();
    libraryJob = std::async(
        std::launch::async,
        [root, path, prompt, generation]
        {
            LibraryJob job{synth2::host::PatchCatalog(root)};
            job.importing = true;
            job.generation = generation;
            job.catalog.registerFiles(
                path,
                [prompt](const auto &destination)
                {
                    std::unique_lock lock(prompt->mutex);
                    if (!prompt->alive)
                        return synth2::host::ConflictChoice{synth2::host::FileConflict::skip, true};
                    prompt->path = destination;
                    prompt->pending = true;
                    prompt->answered = false;
                    prompt->changed.wait(lock, [&] { return prompt->answered || !prompt->alive; });
                    return prompt->alive
                               ? prompt->choice
                               : synth2::host::ConflictChoice{synth2::host::FileConflict::skip, true};
                },
                job.imported);
            return job;
        });
    browserStatus = "Registering sounds...";
    showPage(synth2::editor::editorLibraryView);
    refreshBrowser();
}
void Synth2::showImportMenu(IControl *caller)
{
    presetMenu = std::make_unique<IPopupMenu>("Add sounds");
    presetMenu->AddItem("Add .zip bank or .sy1 file...");
    presetMenu->AddItem("Add folder of .sy1 sounds...");
    presetMenu->AddItem("Open library folder");
    presetMenu->AddItem("Save");
    presetMenu->AddItem("Save as...");
    presetMenu->AddItem("Show all sounds");
    presetMenu->AddItem("Choose library folder...");
    presetMenu->SetFunction(
        [this](IPopupMenu *menu)
        {
            const auto index = menu->GetChosenItemIdx();
            if (index == 0)
                promptForBank();
            else if (index == 1)
                promptForFolder();
            else if (index == 3)
                savePreset(false);
            else if (index == 4)
                savePreset(true);
            else if (index == 5)
            {
                libraryFilter = {};
                resultOffset = 0;
                browserStatus.clear();
                refreshBrowser();
            }
            else if (index == 6)
            {
                if (libraryJob.valid())
                    return;
                const std::weak_ptr<int> lifetime = dialogLifetime;
                WDL_String path;
                GetUI()->PromptForDirectory(
                    path,
                    [this, lifetime](const WDL_String &, const WDL_String &chosen)
                    {
                        if (lifetime.expired() || !chosen.GetLength())
                            return;
                        auto root = std::filesystem::path(
                            std::u8string(chosen.Get(), chosen.Get() + chosen.GetLength()));
                        if (synth2::host::PresetFiles::configureDirectory(root, browserStatus))
                        {
                            catalog = synth2::host::PatchCatalog(root);
                            catalogLoaded = false;
                            libraryFilter = {};
                            selectedPatchId.clear();
                            resultOffset = 0;
                            reloadCatalog();
                        }
                        refreshBrowser();
                    });
            }
            else if (index == 2)
            {
                try
                {
                    if (catalog.root().empty())
                        throw std::runtime_error("The user library folder is unavailable.");
                    std::filesystem::create_directories(catalog.root());
                    const auto utf8 = catalog.root().u8string();
                    const std::string path(utf8.begin(), utf8.end());
                    WDL_String folder(path.c_str());
                    if (auto *ui = GetUI())
                        if (!ui->RevealPathInExplorerOrFinder(folder))
                            browserStatus = "The library folder could not be opened.";
                }
                catch (const std::exception &e)
                {
                    browserStatus = e.what();
                }
                refreshBrowser();
            }
        });
    caller->GetUI()->CreatePopupMenu(*caller, *presetMenu, caller->GetRECT());
}
void Synth2::promptForFolder()
{
    auto *graphics = GetUI();
    if (!graphics)
        return;
    const std::weak_ptr<int> lifetime = dialogLifetime;
    WDL_String path;
    graphics->PromptForDirectory(path,
                                 [this, lifetime](const WDL_String &, const WDL_String &chosen)
                                 {
                                     if (lifetime.expired() || !chosen.GetLength())
                                         return;
                                     importBank(chosen.Get());
                                 });
}
void Synth2::promptForBank()
{
    auto *graphics = GetUI();
    if (!graphics)
        return;
    const std::weak_ptr<int> lifetime = dialogLifetime;
    WDL_String file, path;
    graphics->PromptForFile(file, path, EFileAction::Open, "sy1 zip",
                            [this, lifetime](const WDL_String &chosen, const WDL_String &)
                            {
                                if (lifetime.expired() || !chosen.GetLength())
                                    return;
                                importBank(chosen.Get());
                            });
}
void Synth2::removeLibraryPatch(const std::string &id)
{
    auto *graphics = GetUI();
    if (!graphics)
        return;
    const auto found = std::find_if(catalog.patches().begin(), catalog.patches().end(),
                                    [&](const auto &p) { return p.id == id; });
    if (found == catalog.patches().end())
        return;
    const auto message = "Remove " + found->patch.name +
                         " from the library? The current sound and saved DAW projects are preserved.";
    const std::weak_ptr<int> lifetime = dialogLifetime;
    graphics->ShowMessageBox(message.c_str(), "Remove sound", kMB_YESNO,
                             [this, lifetime, id](EMsgBoxResult result)
                             {
                                 if (lifetime.expired() || result != kYES)
                                     return;
                                 std::string error;
                                 if (!catalog.removePatch(id, error))
                                     browserStatus = error;
                                 else
                                     browserStatus = "Sound removed from library.";
                                 refreshBrowser();
                             });
}
void Synth2::removeLibraryBank()
{
    auto *graphics = GetUI();
    if (!graphics || libraryFilter.bankId.empty())
        return;
    const auto id = libraryFilter.bankId;
    const std::weak_ptr<int> lifetime = dialogLifetime;
    graphics->ShowMessageBox("Remove the selected bank from the library? The original file, current sound "
                             "and saved DAW projects are preserved.",
                             "Remove bank", kMB_YESNO,
                             [this, lifetime, id](EMsgBoxResult result)
                             {
                                 if (lifetime.expired() || result != kYES)
                                     return;
                                 std::string error;
                                 if (!catalog.removeBank(id, error))
                                     browserStatus = error;
                                 else
                                 {
                                     libraryFilter.bankId.clear();
                                     browserStatus = "Bank removed from library.";
                                 }
                                 refreshBrowser();
                             });
}

void Synth2::savePreset(bool saveAs)
{
    if (libraryJob.valid())
    {
        browserStatus = "Please wait for the current library operation.";
        refreshBrowser();
        return;
    }
    const auto found = std::find_if(catalog.patches().begin(), catalog.patches().end(),
                                    [this](const auto &p) { return p.id == selectedPatchId; });
    if (!saveAs && found != catalog.patches().end() && !found->file.empty() && found->archiveEntry.empty())
    {
        writePreset(found->file, true, currentPatch);
        return;
    }
    auto *ui = GetUI();
    if (!ui)
        return;
    std::string name = currentPatch;
    for (auto &c : name)
        if (std::string("/\\:*?\"<>|\r\n").find(c) != std::string::npos)
            c = '_';
    const auto root = catalog.root().u8string();
    WDL_String file((name + ".sy1").c_str()), path(reinterpret_cast<const char *>(root.c_str()));
    const std::weak_ptr<int> lifetime = dialogLifetime;
    ui->PromptForFile(
        file, path, EFileAction::Save, "sy1",
        [this, lifetime](const WDL_String &chosen, const WDL_String &)
        {
            if (lifetime.expired() || !chosen.GetLength())
                return;
            writePreset(std::filesystem::path(std::u8string(chosen.Get(), chosen.Get() + chosen.GetLength())),
                        false);
        });
}
void Synth2::writePreset(const std::filesystem::path &destination, bool confirm, const std::string &title)
{
    auto save = [this, destination, title]
    {
        if (libraryJob.valid())
            return;
        try
        {
            auto patch = synth2::parseContinuousPatch(processor.savePatch());
            auto stem = destination.stem().u8string();
            const std::string name = title.empty() ? std::string(stem.begin(), stem.end()) : title;
            patch.name = name;
            const auto text = synth2::formatContinuousPatch(patch);
            const auto root = catalog.root();
            const auto relative = std::filesystem::weakly_canonical(destination)
                                      .lexically_relative(std::filesystem::weakly_canonical(catalog.root()));
            auto snapshot = browser;
            snapshot.capture(processor);
            auto generation = browserGeneration.load();
            libraryJob = std::async(std::launch::async,
                                    [root, relative, text, name, snapshot, generation]
                                    {
                                        LibraryJob job{synth2::host::PatchCatalog(root)};
                                        job.saving = true;
                                        job.generation = generation;
                                        job.savedName = name;
                                        job.savedBaseline = snapshot.baseline;
                                        job.catalog.saveFile(relative, text, true, job.savedId, job.error);
                                        return job;
                                    });
            browserStatus = "Saving preset...";
            refreshBrowser();
        }
        catch (const std::exception &e)
        {
            browserStatus = e.what();
            refreshBrowser();
        }
    };
    if (confirm && std::filesystem::exists(destination))
    {
        const std::weak_ptr<int> lifetime = dialogLifetime;
        GetUI()->ShowMessageBox("Replace the existing preset file?", "Save preset", kMB_YESNO,
                                [save, lifetime](EMsgBoxResult result)
                                {
                                    if (!lifetime.expired() && result == kYES)
                                        save();
                                });
    }
    else
        save();
}

void Synth2::showSaveMenu(IControl *caller)
{
    presetMenu = std::make_unique<IPopupMenu>("Save preset");
    presetMenu->AddItem("Save");
    presetMenu->AddItem("Save as...");
    presetMenu->SetFunction(
        [this](IPopupMenu *menu)
        {
            int i = menu->GetChosenItemIdx();
            if (i >= 0)
                savePreset(i == 1);
        });
    caller->GetUI()->CreatePopupMenu(*caller, *presetMenu, caller->GetRECT());
}
