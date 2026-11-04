#include "PatchCatalog.h"
#include "ProcessorBridge.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace synth2::host {
namespace {
namespace fs = std::filesystem;
std::string utf8(const fs::path& p) { const auto s=p.u8string();return {s.begin(),s.end()}; }
std::string lower(std::string s) {
    for(auto& c:s)if(static_cast<unsigned char>(c)<128)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string read(const fs::path& path, std::size_t limit) {
    if(!fs::is_regular_file(fs::symlink_status(path)))throw std::runtime_error("A library file is missing or is not a regular file.");
    const auto size=fs::file_size(path);
    if(size>limit)throw std::runtime_error("A library file exceeds the size limit.");
    std::string text(static_cast<std::size_t>(size),'\0');std::ifstream in(path,std::ios::binary);
    if(!in.read(text.data(),static_cast<std::streamsize>(size)) || in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("A library file could not be read completely.");
    return text;
}
void write(const fs::path& path, const std::string& text) {
    std::ofstream out(path,std::ios::binary|std::ios::trunc);
    out.write(text.data(),static_cast<std::streamsize>(text.size()));out.close();
    if(!out)throw std::runtime_error("The library could not be saved. Check the folder permissions and free space.");
}
void replaceText(const fs::path& path,const std::string& text) {
    auto temporary=path;temporary+=".tmp-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    try {
        write(temporary,text);
#ifdef _WIN32
        if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("The library metadata could not be replaced.");
#else
        fs::rename(temporary,path);
#endif
    } catch(...) {std::error_code ignored;fs::remove(temporary,ignored);throw;}
}
fs::path favorites(const fs::path& root,const std::string& id) {
    auto filename=id;std::replace(filename.begin(),filename.end(),'/', '_');return root/"favorites"/(filename+".fav");
}
fs::path tagPath(const fs::path& root,const std::string& id) {
    auto filename=id;std::replace(filename.begin(),filename.end(),'/', '_');return root/"tags"/(filename+".tags");
}
std::vector<std::string> tagsAt(const fs::path& root,const std::string& id) {
    if(root.empty() || !fs::exists(tagPath(root,id)))return {};
    return PatchCatalog::splitTags(read(tagPath(root,id),16384));
}
bool favoriteAt(const fs::path& root,const std::string& id) {
    return !root.empty() && fs::is_regular_file(fs::symlink_status(favorites(root,id)));
}
void requireRoot(const fs::path& root) {
    if(root.empty())throw std::runtime_error("The user library folder is unavailable.");
}
}

std::filesystem::path PatchCatalog::userDirectory() {
#ifdef _WIN32
    if(const auto* base=_wgetenv(L"APPDATA"))return fs::path(base)/"Synth2"/"Library";
#elif defined(__APPLE__)
    if(const auto* base=std::getenv("HOME"))return fs::path(base)/"Library"/"Application Support"/"Synth2"/"Library";
#else
    if(const auto* base=std::getenv("XDG_DATA_HOME"))return fs::path(base)/"Synth2"/"Library";
    if(const auto* base=std::getenv("HOME"))return fs::path(base)/".local"/"share"/"Synth2"/"Library";
#endif
    return {};
}
std::vector<std::string> PatchCatalog::splitTags(const std::string& text) {
    std::istringstream input(text);std::vector<std::string> tags;std::unordered_set<std::string> seen;
    for(std::string tag;std::getline(input,tag,',');) {
        const auto begin=tag.find_first_not_of(" \t\r\n");if(begin==std::string::npos)continue;
        tag=tag.substr(begin,tag.find_last_not_of(" \t\r\n")-begin+1);
        if(tag.size()>128 || tag.find_first_of("\r\n")!=std::string::npos || tags.size()>=32)throw std::runtime_error("Use up to 32 comma-separated tags, each up to 128 UTF-8 bytes.");
        if(seen.insert(lower(tag)).second)tags.push_back(std::move(tag));
    }
    return tags;
}
std::string PatchCatalog::tagText(const std::vector<std::string>& tags) {
    std::string result;for(const auto& tag:tags){if(!result.empty())result+=", ";result+=tag;}return result;
}

bool PatchCatalog::reload(std::string& error) {
    try {
        std::vector<CatalogPatch> next;
        std::size_t total=0;
        if(!directory.empty() && fs::exists(directory/"banks")) {
            std::vector<fs::path> banks;
            for(const auto& bank:fs::directory_iterator(directory/"banks"))
                if(fs::is_directory(bank.symlink_status()) && utf8(bank.path().filename()).starts_with("bank-"))banks.push_back(bank.path());
            std::sort(banks.begin(),banks.end());
            for(const auto& bank:banks) {
                const auto bankId=utf8(bank.filename()), bankName=read(bank/"name",16384);
                std::vector<fs::path> patches;
                for(const auto& file:fs::directory_iterator(bank))
                    if(file.path().extension()==".sy1" && fs::is_regular_file(file.symlink_status()))patches.push_back(file.path());
                std::sort(patches.begin(),patches.end());
                for(const auto& file:patches) {
                    const auto key=utf8(file.stem());
                    if(key.empty() || key.size()>4 || key.find_first_not_of("0123456789")!=std::string::npos)continue;
                    const auto ordinal=std::stoi(key);if(ordinal>=static_cast<int>(PatchLibrary::maximumPatches))continue;
                    auto text=read(file,PatchLibrary::maximumPatchBytes);
                    if(text.empty())throw std::runtime_error("The library contains an empty patch.");
                    if(next.size()>=maximumCatalogPatches || text.size()>maximumCatalogBytes-total)throw std::runtime_error("The library exceeds its size limit.");
                    total+=text.size();const auto id=bankId+"/"+key;
                    next.push_back({id,bankId,bankName,{read(bank/(key+".name"),16384),std::move(text)},ordinal,favoriteAt(directory,id),tagsAt(directory,id)});
                }
            }
        }
        entries.swap(next);error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}

bool PatchCatalog::importFile(const std::string& path,ImportResult& result,std::string& error) {
    result={};fs::path staging;
    try {
        requireRoot(directory);
        // Refresh before mutation so separate plugin instances see committed banks.
        if(!reload(error))return false;
        PatchLibrary source;
        const auto inputPath=fs::path(std::u8string(path.begin(),path.end()));
        if(fs::is_directory(inputPath)) {if(!source.openDirectory(path,error))return false;}
        else if(!source.openFile(path,error))return false;
        if(source.name().size()>16384)throw std::runtime_error("The bank name exceeds the library name limit.");
        std::unordered_set<std::string_view> known;std::size_t bytes=0;
        for(const auto& p:entries){known.insert(p.patch.text);bytes+=p.patch.text.size();}
        std::vector<std::size_t> additions;ProcessorBridge validator;
        for(std::size_t i=0;i<source.patches().size();++i) {
            const auto& p=source.patches()[i];
            if(p.name.size()>16384)throw std::runtime_error("A patch name exceeds the library name limit.");
            if(!validator.loadPatch(p.text,error)){error="Cannot add "+p.name+": "+error;return false;}
            if(!known.insert(p.text).second){++result.duplicates;continue;}
            if(entries.size()+additions.size()>=maximumCatalogPatches || p.text.size()>maximumCatalogBytes-bytes)
                throw std::runtime_error("The library exceeds its size limit.");
            bytes+=p.text.size();additions.push_back(i);
        }
        if(additions.empty()){error.clear();return true;}
        fs::create_directories(directory/"banks");
        // Exclusive directory creation also separates imports from different instances.
        std::string token;
        for(int attempt=0;attempt<100;++attempt) {
            token=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(attempt);
            staging=directory/"banks"/(".import-"+token);
            if(fs::create_directory(staging))break;
            staging.clear();
        }
        if(staging.empty())throw std::runtime_error("Could not create a library import folder.");
        write(staging/"name",source.name());
        for(const auto i:additions) {
            auto key=std::to_string(i);key=std::string(4-key.size(),'0')+key;
            write(staging/(key+".sy1"),source.patches()[i].text);
            write(staging/(key+".name"),source.patches()[i].name);
        }
        const auto committed=directory/"banks"/("bank-"+token);
        fs::rename(staging,committed);staging.clear();
        if(!reload(error))return false;
        result.added=additions.size();return true;
    } catch(const std::exception& e) {
        if(!staging.empty()){std::error_code ignored;fs::remove_all(staging,ignored);}
        error=e.what();return false;
    }
}

bool PatchCatalog::setFavorite(const std::string& id,bool favorite,std::string& error) {
    try {
        requireRoot(directory);
        const auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& p){return p.id==id;});
        if(found==entries.end())throw std::runtime_error("This patch is no longer in the library. Refresh the list.");
        fs::create_directories(directory/"favorites");
        if(favorite)replaceText(favorites(directory,id),"favorite\n");else fs::remove(favorites(directory,id));
        found->favorite=favorite;error.clear();return true;
    } catch(const std::exception& e){error=e.what();return false;}
}
bool PatchCatalog::setTags(const std::string& id,const std::string& text,std::string& error) {
    try {
        requireRoot(directory);
        const auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& p){return p.id==id;});
        if(found==entries.end())throw std::runtime_error("This patch is no longer in the library. Refresh the list.");
        auto tags=splitTags(text);fs::create_directories(directory/"tags");
        if(tags.empty())fs::remove(tagPath(directory,id));else replaceText(tagPath(directory,id),tagText(tags));
        found->tags=std::move(tags);error.clear();return true;
    } catch(const std::exception& e){error=e.what();return false;}
}
bool PatchCatalog::removePatch(const std::string& id,std::string& error) {
    try {
        requireRoot(directory);
        const auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& p){return p.id==id;});
        if(found==entries.end())throw std::runtime_error("This patch is no longer in the library.");
        const auto bank=directory/"banks"/found->bankId;
        if(!fs::is_directory(fs::symlink_status(bank)))throw std::runtime_error("The bank folder is unavailable.");
        const auto key=id.substr(id.find('/')+1);
        if(!fs::remove(bank/(key+".sy1")))throw std::runtime_error("This patch was already removed. Refresh the list.");
        std::error_code ignored;fs::remove(bank/(key+".name"),ignored);fs::remove(favorites(directory,id),ignored);fs::remove(tagPath(directory,id),ignored);
        entries.erase(found);error.clear();return true;
    } catch(const std::exception& e){error=e.what();return false;}
}
bool PatchCatalog::removeBank(const std::string& bankId,std::string& error) {
    try {
        requireRoot(directory);
        const auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& p){return p.bankId==bankId;});
        if(found==entries.end())throw std::runtime_error("This bank is no longer in the library.");
        const auto bank=directory/"banks"/bankId;
        if(!fs::is_directory(fs::symlink_status(bank)))throw std::runtime_error("The bank folder is unavailable.");
        // Unpublish first; refreshes cannot observe a half-deleted bank.
        const auto trash=directory/"banks"/(".removed-"+bankId);
        fs::rename(bank,trash);
        std::error_code ignored;fs::remove_all(trash,ignored);
        for(const auto& p:entries)if(p.bankId==bankId){fs::remove(favorites(directory,p.id),ignored);fs::remove(tagPath(directory,p.id),ignored);}
        std::erase_if(entries,[&](const auto& p){return p.bankId==bankId;});error.clear();return true;
    } catch(const std::exception& e){error=e.what();return false;}
}
std::vector<std::size_t> PatchCatalog::search(const CatalogFilter& filter)const {
    std::istringstream input(lower(filter.query));std::vector<std::string> terms;
    for(std::string term;input>>term;)terms.push_back(std::move(term));
    std::vector<std::size_t> result;
    for(std::size_t i=0;i<entries.size();++i) {
        const auto& p=entries[i];
        if((!filter.bankId.empty() && p.bankId!=filter.bankId) || (filter.favoritesOnly && !p.favorite))continue;
        if(!filter.tag.empty() && std::none_of(p.tags.begin(),p.tags.end(),[&](const auto& tag){return lower(tag)==lower(filter.tag);}))continue;
        const auto haystack=lower(p.patch.name+" "+p.bankName+" "+tagText(p.tags));
        if(std::all_of(terms.begin(),terms.end(),[&](const auto& term){return haystack.find(term)!=std::string::npos;}))result.push_back(i);
    }
    return result;
}
}
