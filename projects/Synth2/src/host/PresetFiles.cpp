#include "PresetFiles.h"
#include "PatchLibrary.h"
#include "patch/PatchFormat.h"
#include "patch/PatchLoader.h"
#include <tinyxml2.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <stdio.h>
#endif

namespace synth2::host
{
namespace fs = std::filesystem;
namespace
{
std::string utf8(const fs::path &p)
{
    auto s = p.generic_u8string();
    return {s.begin(), s.end()};
}
std::string lower(std::string s)
{
    for (auto &c : s)
        if (static_cast<unsigned char>(c) < 128)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
bool supported(const fs::path &p)
{
    auto e = lower(utf8(p.extension()));
    return e == ".sy1" || e == ".zip";
}
std::string identity(const std::string &path, const std::string &entry = {})
{
    return std::to_string(path.size()) + ":" + path + entry;
}
bool inside(const fs::path &parent, const fs::path &child)
{
    auto p = fs::weakly_canonical(parent), c = fs::weakly_canonical(child);
    auto pi = p.begin(), ci = c.begin();
    for (; pi != p.end(); ++pi, ++ci)
        if (ci == c.end() || *pi != *ci)
            return false;
    return true;
}
bool naturalLess(const std::string &a, const std::string &b)
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
std::string validate(const std::string &text)
{
    ProcessorBridge validator;
    std::string error;
    if (!validator.loadPatch(text, error))
        return error;
    return {};
}
// A sibling temporary is renamed only after a complete write. Never truncate the
// existing destination. Exclusive temporary creation avoids sharing instances.
bool writeFile(const fs::path &destination, const std::string &bytes, bool replace, std::string &error)
{
    fs::path staging;
    try
    {
        fs::create_directories(destination.parent_path());
        static std::atomic<unsigned> serial{};
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            staging = destination.parent_path() /
                      (".synth2-write-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                       std::to_string(serial++));
            if (fs::create_directory(staging))
                break;
            staging.clear();
        }
        if (staging.empty())
            throw std::runtime_error("Cannot create temporary directory.");
        auto temporary = staging / "data";
        {
            std::ofstream out(temporary, std::ios::binary);
            out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            out.flush();
            if (!out)
                throw std::runtime_error("Cannot write the complete preset file.");
            out.close();
            if (!out)
                throw std::runtime_error("Cannot close the preset file.");
        }
#ifdef _WIN32
        const auto flags = MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0);
        if (!MoveFileExW(temporary.c_str(), destination.c_str(), flags))
            throw std::runtime_error("Cannot publish the destination file.");
#elif defined(__APPLE__)
        if (renamex_np(temporary.c_str(), destination.c_str(), replace ? 0 : RENAME_EXCL) != 0)
            throw std::runtime_error("Cannot publish the destination file.");
#else
        if (!replace)
        {
            // Publish without overwriting a concurrently created destination.
            fs::create_hard_link(temporary, destination);
        }
        else
            fs::rename(temporary, destination);
#endif
        std::error_code cleanupError;
        fs::remove_all(staging, cleanupError);
        error.clear();
        return true;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        if (!staging.empty())
        {
            std::error_code ignored;
            fs::remove_all(staging, ignored);
        }
        return false;
    }
}
std::string readFile(const fs::path &path)
{
    auto size = fs::file_size(path);
    if (size > PatchLibrary::maximumBankBytes)
        throw std::runtime_error("File exceeds the size limit.");
    std::ifstream in(path, std::ios::binary);
    std::string s(static_cast<std::size_t>(size), '\0');
    if (!in.read(s.data(), s.size()) || in.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Cannot read the complete file.");
    return s;
}
fs::path environmentPath(const char *name)
{
#ifdef _WIN32
    std::wstring key;
    while (*name)
        key += static_cast<wchar_t>(*name++);
    auto n = GetEnvironmentVariableW(key.c_str(), nullptr, 0);
    if (!n)
        return {};
    std::wstring value(n, L'\0');
    GetEnvironmentVariableW(key.c_str(), value.data(), n);
    value.resize(n - 1);
    return fs::path(value);
#else
    auto value = std::getenv(name);
    return value ? fs::path(value) : fs::path{};
#endif
}
} // namespace
PresetFiles::PresetFiles(fs::path directory) : root(fs::weakly_canonical(fs::absolute(std::move(directory))))
{
}
bool PresetFiles::scan(std::string &error)
{
    try
    {
        fs::create_directories(root);
        std::vector<UserPreset> next;
        std::size_t total = 0;
        auto append = [&](UserPreset item)
        {
            if (next.size() >= 100000 || item.text.size() > 512u * 1024u * 1024u - total)
                throw std::runtime_error("The library exceeds its size limit.");
            total += item.text.size();
            next.push_back(std::move(item));
        };
        for (const auto &item :
             fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied))
        {
            if (item.is_symlink() || !item.is_regular_file() || !supported(item.path()))
                continue;
            const auto relative = utf8(item.path().lexically_relative(root));
            const auto first = *item.path().lexically_relative(root).begin();
            if (first == "banks" || first == "tags" || first == "favorites" ||
                utf8(first).starts_with(".synth2-write-"))
                continue;
            PatchLibrary bank;
            std::string failure;
            if (!bank.openFile(utf8(item.path()), failure))
            {
                append(
                    {identity(relative), utf8(item.path().stem()), relative, {}, {}, failure, item.path()});
                continue;
            }
            bool zip = lower(utf8(item.path().extension())) == ".zip";
            int ordinal = 0;
            for (const auto &patch : bank.patches())
            {
                auto docName = patch.name;
                auto entry = zip ? patch.entry : std::string{};
                try
                {
                    const auto name = synth2::isContinuousPatch(patch.text)
                                          ? synth2::parseContinuousPatch(patch.text).name
                                          : parsePatchDocument(patch.text).name;
                    if (!name.empty())
                        docName = name;
                }
                catch (const std::exception &)
                {
                }
                auto problem = validate(patch.text);
                if (docName.size() > 16384)
                {
                    docName = utf8(item.path().filename());
                    problem = "The patch name exceeds the library name limit.";
                }
                append({identity(relative, entry), docName, relative + (zip ? " / " + entry : ""), entry,
                        patch.text, problem, item.path()});
                next.back().program = ordinal++;
            }
        }
        std::sort(next.begin(), next.end(),
                  [](const auto &a, const auto &b)
                  {
                      if (naturalLess(a.location, b.location))
                          return true;
                      if (naturalLess(b.location, a.location))
                          return false;
                      return a.id < b.id;
                  });
        items.swap(next);
        error.clear();
        return true;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return false;
    }
}
PresetImportResult PresetFiles::import(const fs::path &inputSource, const ResolveConflict &resolve)
{
    PresetImportResult result;
    std::vector<fs::path> files;
    std::vector<std::string> registered;
    try
    {
        const auto source = fs::weakly_canonical(inputSource);
        bool directory = fs::is_directory(source);
        if (directory && source != root && inside(source, root))
            throw std::runtime_error("The source folder contains the preset directory.");
        if (directory)
        {
            for (const auto &e :
                 fs::recursive_directory_iterator(source, fs::directory_options::skip_permission_denied))
                if (!e.is_symlink() && e.is_regular_file() && supported(e.path()))
                    files.push_back(e.path());
        }
        else
        {
            if (!supported(source))
                throw std::runtime_error("Choose a .sy1 file, ZIP or folder.");
            files.push_back(source);
        }
        std::sort(files.begin(), files.end());
        bool all = false;
        FileConflict policy = FileConflict::keepBoth;
        for (const auto &file : files)
        {
            try
            {
                fs::path destination;
                if (inside(root, file))
                    destination = fs::weakly_canonical(file);
                else
                {
                    auto relative =
                        directory ? source.filename() / file.lexically_relative(source) : file.filename();
                    destination = root / relative;
                    if (!inside(root, destination))
                        throw std::runtime_error("Destination escapes the preset directory.");
                    bool replace = false;
                    if (fs::exists(destination))
                    {
                        auto choice = all ? ConflictChoice{policy, true} : resolve(destination);
                        policy = choice.action;
                        all = choice.applyToAll;
                        if (policy == FileConflict::skip)
                        {
                            ++result.skipped;
                            continue;
                        }
                        if (policy == FileConflict::keepBoth)
                        {
                            auto original = destination;
                            for (unsigned n = 2; fs::exists(destination); ++n)
                                destination = original.parent_path() /
                                              (utf8(original.stem()) + " (" + std::to_string(n) + ")" +
                                               utf8(original.extension()));
                        }
                        else
                            replace = true;
                    }
                    std::string failure;
                    if (!writeFile(destination, readFile(file), replace, failure))
                        throw std::runtime_error(failure);
                }
                registered.push_back(utf8(destination.lexically_relative(root)));
                ++result.imported;
            }
            catch (const std::exception &e)
            {
                result.errors.push_back(utf8(file) + ": " + e.what());
            }
        }
    }
    catch (const std::exception &e)
    {
        result.errors.push_back(e.what());
    }
    std::string failure;
    if (!scan(failure))
        result.errors.push_back(failure);
    for (const auto &item : items)
        if (std::find(registered.begin(), registered.end(), utf8(item.file.lexically_relative(root))) !=
            registered.end())
        {
            result.ids.push_back(item.id);
            if (!item.error.empty())
                result.errors.push_back(item.location + ": " + item.error);
        }
    return result;
}
bool PresetFiles::save(const fs::path &relative, const std::string &text, bool replace, std::string &id,
                       std::string &error)
{
    try
    {
        if (relative.empty() || relative.is_absolute() || lower(utf8(relative.extension())) != ".sy1" ||
            !inside(root, root / relative))
            throw std::runtime_error("Save a .sy1 file inside the preset directory.");
        if (auto failure = validate(text); !failure.empty())
            throw std::runtime_error(failure);
        if (!writeFile(root / relative, text, replace, error))
            return false;
        id = identity(utf8((root / relative).lexically_normal().lexically_relative(root)));
        return scan(error);
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return false;
    }
}
std::vector<std::size_t> PresetFiles::search(const std::string &query, bool restricted,
                                             const std::vector<std::string> &ids) const
{
    std::istringstream words(lower(query));
    std::vector<std::string> terms;
    std::string word;
    while (words >> word)
        terms.push_back(word);
    std::vector<std::size_t> result;
    for (std::size_t i = 0; i < items.size(); ++i)
    {
        const auto &item = items[i];
        if (restricted && std::find(ids.begin(), ids.end(), item.id) == ids.end())
            continue;
        auto text = lower(item.name + " " + item.location);
        if (std::all_of(terms.begin(), terms.end(),
                        [&](const auto &t) { return text.find(t) != std::string::npos; }))
            result.push_back(i);
    }
    return result;
}
fs::path PresetFiles::settingsDirectory()
{
#ifdef _WIN32
    auto home = environmentPath("APPDATA");
#elif defined(__APPLE__)
    auto home = environmentPath("HOME");
    if (!home.empty())
        home /= "Library/Application Support";
#else
    auto home = environmentPath("XDG_DATA_HOME");
    if (home.empty())
    {
        home = environmentPath("HOME");
        if (!home.empty())
            home /= ".local/share";
    }
#endif
    if (home.empty())
        throw std::runtime_error("The user data directory is unavailable.");
    return home / "Synth2";
}
fs::path PresetFiles::defaultDirectory() { return settingsDirectory() / "Library"; }
fs::path PresetFiles::configuredDirectory()
{
    try
    {
        auto file = settingsDirectory() / "preset-directory";
        if (!fs::exists(file))
            return defaultDirectory();
        auto value = readFile(file);
        if (value.empty())
            return defaultDirectory();
        auto p = fs::path(std::u8string(value.begin(), value.end()));
        return p.is_absolute() ? p : defaultDirectory();
    }
    catch (const std::exception &)
    {
        return defaultDirectory();
    }
}

bool PresetFiles::configureDirectory(const fs::path &path, std::string &error)
{
    try
    {
        fs::create_directories(path);
        return writeFile(settingsDirectory() / "preset-directory",
                         utf8(fs::weakly_canonical(fs::absolute(path))), true, error);
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return false;
    }
}
void PresetBrowserState::capture(const ProcessorBridge &p)
{
    for (int i = 0; i < p.parameterCount; ++i)
        baseline[i] = p.normalizedValue(i);
}
bool PresetBrowserState::modified(const ProcessorBridge &p) const
{
    for (int i = 0; i < p.parameterCount; ++i)
        if (std::abs(baseline[i] - p.normalizedValue(i)) > 1e-7)
            return true;
    return false;
}
std::string PresetBrowserState::serialize() const
{
    tinyxml2::XMLDocument d;
    auto *e = d.NewElement("browser");
    d.InsertEndChild(e);
    e->SetAttribute("version", 1);
    e->SetAttribute("query", query.c_str());
    e->SetAttribute("bank", bankId.c_str());
    e->SetAttribute("tag", tag.c_str());
    e->SetAttribute("favorites", favoritesOnly);
    e->SetAttribute("selected", selectedId.c_str());
    e->SetAttribute("name", patchName.c_str());
    e->SetAttribute("restricted", restricted);
    e->SetAttribute("scroll", scroll);
    e->SetAttribute("page", page);
    for (const auto &id : importedIds)
    {
        auto *child = d.NewElement("item");
        child->SetAttribute("id", id.c_str());
        e->InsertEndChild(child);
    }
    std::ostringstream s;
    s.imbue(std::locale::classic());
    s << std::setprecision(17);
    for (double v : baseline)
        s << v << ' ';
    auto *b = d.NewElement("baseline");
    b->SetText(s.str().c_str());
    e->InsertEndChild(b);
    tinyxml2::XMLPrinter p;
    d.Print(&p);
    return p.CStr();
}
bool PresetBrowserState::deserialize(const std::string &text, PresetBrowserState &target)
{
    if (text.size() > 32 * 1024 * 1024)
        return false;
    tinyxml2::XMLDocument d;
    if (d.Parse(text.data(), text.size()) != tinyxml2::XML_SUCCESS)
        return false;
    auto *e = d.RootElement();
    if (!e || std::string(e->Name()) != "browser" || e->IntAttribute("version") != 1)
        return false;
    PresetBrowserState next;
    auto attr = [&](const char *name)
    {
        auto *s = e->Attribute(name);
        return s ? std::string(s) : std::string{};
    };
    next.query = attr("query");
    next.bankId = attr("bank");
    next.tag = attr("tag");
    next.favoritesOnly = e->BoolAttribute("favorites");
    next.selectedId = attr("selected");
    next.patchName = attr("name");
    next.restricted = e->BoolAttribute("restricted");
    next.scroll = std::max(0, e->IntAttribute("scroll"));
    next.page = std::clamp(e->IntAttribute("page"), 0, 2);
    for (auto *c = e->FirstChildElement("item"); c; c = c->NextSiblingElement("item"))
    {
        auto *id = c->Attribute("id");
        if (!id)
            return false;
        next.importedIds.emplace_back(id);
    }
    auto *b = e->FirstChildElement("baseline");
    if (!b || !b->GetText())
        return false;
    std::istringstream values(b->GetText());
    values.imbue(std::locale::classic());
    for (auto &v : next.baseline)
        if (!(values >> v) || !std::isfinite(v) || v < 0 || v > 1)
            return false;
    values >> std::ws;
    if (!values.eof())
        return false;
    target = std::move(next);
    return true;
}
bool PresetFiles::saveStandaloneState(const std::vector<uint8_t> &bytes, std::string &error)
{
    return writeFile(settingsDirectory() / "standalone.state", std::string(bytes.begin(), bytes.end()), true,
                     error);
}
std::vector<uint8_t> PresetFiles::loadStandaloneState()
{
    try
    {
        auto text = readFile(settingsDirectory() / "standalone.state");
        if (text.size() > 48 * 1024 * 1024)
            return {};
        return {text.begin(), text.end()};
    }
    catch (const std::exception &)
    {
        return {};
    }
}
std::vector<uint8_t> savePresetSession(const ProcessorBridge &processor, const PresetBrowserState &browser)
{
    const auto sound = processor.saveState();
    const auto view = browser.serialize();
    std::vector<uint8_t> result;
    auto put = [&](uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            result.push_back(static_cast<uint8_t>(v >> (i * 8)));
    };
    put(0x32504953);
    put(2);
    put(static_cast<uint32_t>(sound.size()));
    result.insert(result.end(), sound.begin(), sound.end());
    put(static_cast<uint32_t>(view.size()));
    result.insert(result.end(), view.begin(), view.end());
    return result;
}
int restorePresetSession(const void *data, int size, ProcessorBridge &processor, PresetBrowserState &browser)
{
    if (!data || size < 12)
        return -1;
    const auto *bytes = static_cast<const uint8_t *>(data);
    int pos = 0;
    auto get = [&](uint32_t &v)
    {
        if (pos > size - 4)
            return false;
        v = 0;
        for (int i = 0; i < 4; ++i)
            v |= uint32_t(bytes[pos++]) << (i * 8);
        return true;
    };
    uint32_t magic = 0, version = 0, soundSize = 0;
    if (!get(magic) || !get(version) || !get(soundSize) || magic != 0x32504953 ||
        (version != 1 && version != 2) || !soundSize || soundSize > 4 * 1024 * 1024 ||
        soundSize > static_cast<uint32_t>(size - pos))
        return -1;
    const int soundPosition = pos;
    pos += static_cast<int>(soundSize);
    PresetBrowserState next;
    if (version == 2)
    {
        uint32_t viewSize = 0;
        if (!get(viewSize) || viewSize > 32 * 1024 * 1024 || viewSize > static_cast<uint32_t>(size - pos))
            return -1;
        if (!PresetBrowserState::deserialize(
                std::string(reinterpret_cast<const char *>(bytes + pos), viewSize), next))
            return -1;
        pos += static_cast<int>(viewSize);
    }
    if (!processor.tryRestoreState(bytes + soundPosition, static_cast<int>(soundSize)))
        return -1;
    if (version == 1)
    {
        next.capture(processor);
        next.patchName = processor.currentPatchName();
    }
    browser = std::move(next);
    return pos;
}

} // namespace synth2::host
