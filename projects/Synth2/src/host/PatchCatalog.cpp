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
fs::path metadataKey(const std::string& id) {
    if(id.starts_with("bank-")){auto key=id;std::replace(key.begin(),key.end(),'/', '_');return key;}
    const char* hex="0123456789abcdef";std::string key;for(unsigned char c:id){key+=hex[c>>4];key+=hex[c&15];}
    fs::path path="files";while(key.size()>100){path/=key.substr(0,100);key.erase(0,100);}return path/key;
}
fs::path favorites(const fs::path& root,const std::string& id) {auto path=root/"favorites"/metadataKey(id);path+=".fav";return path;}
fs::path tagPath(const fs::path& root,const std::string& id) {auto path=root/"tags"/metadataKey(id);path+=".tags";return path;}
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
    try{return PresetFiles::configuredDirectory();}catch(const std::exception&){return {};}
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
        if(!directory.empty() && fs::is_directory(fs::symlink_status(directory/"banks"))) {
            std::vector<fs::path> banks;
            for(const auto& bank:fs::directory_iterator(directory/"banks"))
                if(PresetFiles::isManagedBank(directory,bank.path()))banks.push_back(bank.path());
            std::sort(banks.begin(),banks.end());
            for(const auto& bank:banks) {
                const auto bankId=utf8(bank.filename()), bankName=read(bank/"name",16384);
                std::vector<fs::path> patches;
                for(const auto& file:fs::directory_iterator(bank))
                    if(PresetFiles::isManagedPatch(directory,file.path()))patches.push_back(file.path());
                std::sort(patches.begin(),patches.end());
                for(const auto& file:patches) {
                    const auto key=utf8(file.stem());
                    if(key.empty() || key.size()>std::to_string(PatchLibrary::maximumPatches-1).size() || key.find_first_not_of("0123456789")!=std::string::npos)continue;
                    const auto ordinal=std::stoi(key);if(ordinal>=static_cast<int>(PatchLibrary::maximumPatches))continue;
                    auto text=read(file,PatchLibrary::maximumPatchBytes);
                    if(text.empty())throw std::runtime_error("The library contains an empty patch.");
                    if(next.size()>=maximumCatalogPatches || text.size()>maximumCatalogBytes-total)throw std::runtime_error("The library exceeds its size limit.");
                    total+=text.size();const auto id=bankId+"/"+key;
                    next.push_back({id,bankId,bankName,{read(bank/(key+".name"),16384),std::move(text)},ordinal,favoriteAt(directory,id),tagsAt(directory,id)});
                }
            }
        }
        if(!directory.empty()) {
            PresetFiles files(directory);if(!files.scan(error))return false;
            for(const auto& item:files.entries()) {
                if(next.size()>=maximumCatalogPatches || item.text.size()>maximumCatalogBytes-total)throw std::runtime_error("The library exceeds its size limit.");
                total+=item.text.size();auto relative=item.file.lexically_relative(fs::weakly_canonical(directory));
                // Keep existing filter identities while displaying portable path separators.
                const auto bankId=utf8(relative);
                const auto displayPath=relative.generic_u8string();
                const std::string bankName(displayPath.begin(),displayPath.end());
                next.push_back({item.id,bankId,bankName,{item.name,item.text,item.archiveEntry},item.program,favoriteAt(directory,item.id),tagsAt(directory,item.id),item.file,item.archiveEntry,item.error});
            }
        }
        entries.swap(next);error.clear();return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}

bool PatchCatalog::registerFiles(const std::string& path,const ResolveConflict& resolve,PresetImportResult& result) {
    try {
        requireRoot(directory);PresetFiles files(directory);result=files.import(fs::path(std::u8string(path.begin(),path.end())),resolve);
        std::string error;result.catalogReady=reload(error);if(!result.catalogReady)result.errors.push_back(error);
        return result.errors.empty();
    }catch(const std::exception& e){result={};result.errors.push_back(e.what());return false;}
}

bool PatchCatalog::importFile(const std::string& path,ImportResult& result,std::string& error) {
    PresetImportResult registered;
    bool ok=registerFiles(path,[](const auto&){return ConflictChoice{};},registered);
    result={registered.ids.size(),registered.skipped};
    error=registered.errors.empty() ? "" : registered.errors.front();return ok;
}
bool PatchCatalog::saveFile(const fs::path& relative,const std::string& text,bool replace,std::string& id,std::string& error) {
    try {
        requireRoot(directory);PresetFiles files(directory);if(!files.save(relative,text,replace,id,error))return false;
        return reload(error);
    }catch(const std::exception& e){error=e.what();return false;}
}

bool PatchCatalog::setFavorite(const std::string& id,bool favorite,std::string& error) {
    try {
        requireRoot(directory);
        const auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& p){return p.id==id;});
        if(found==entries.end())throw std::runtime_error("This patch is no longer in the library. Refresh the list.");
        fs::create_directories(favorites(directory,id).parent_path());
        if(favorite)replaceText(favorites(directory,id),"favorite\n");else fs::remove(favorites(directory,id));
        found->favorite=favorite;error.clear();return true;
    } catch(const std::exception& e){error=e.what();return false;}
}
bool PatchCatalog::setTags(const std::string& id,const std::string& text,std::string& error) {
    try {
        requireRoot(directory);
        const auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& p){return p.id==id;});
        if(found==entries.end())throw std::runtime_error("This patch is no longer in the library. Refresh the list.");
        auto tags=splitTags(text);fs::create_directories(tagPath(directory,id).parent_path());
        if(tags.empty())fs::remove(tagPath(directory,id));else replaceText(tagPath(directory,id),tagText(tags));
        found->tags=std::move(tags);error.clear();return true;
    } catch(const std::exception& e){error=e.what();return false;}
}
bool PatchCatalog::removePatch(const std::string& id,std::string& error) {
    try {
        requireRoot(directory);
        const auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& p){return p.id==id;});
        if(found==entries.end())throw std::runtime_error("This patch is no longer in the library.");
        if(!found->file.empty()) {
            if(!found->archiveEntry.empty())throw std::runtime_error("Remove the ZIP bank to remove its contained sounds.");
            if(!fs::remove(found->file))throw std::runtime_error("The file was already removed. Refresh the library.");
            std::error_code ignored;fs::remove(favorites(directory,id),ignored);fs::remove(tagPath(directory,id),ignored);
            entries.erase(found);error.clear();return true;
        }
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
        if(!found->file.empty()) {
            if(!fs::remove(found->file))throw std::runtime_error("The bank was already removed. Refresh the library.");
            std::error_code ignored;for(const auto& p:entries)if(p.bankId==bankId){fs::remove(favorites(directory,p.id),ignored);fs::remove(tagPath(directory,p.id),ignored);}
            std::erase_if(entries,[&](const auto& p){return p.bankId==bankId;});error.clear();return true;
        }
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
        if(filter.registeredOnly && std::find(filter.registeredIds.begin(),filter.registeredIds.end(),p.id)==filter.registeredIds.end())continue;
        if((!filter.bankId.empty() && p.bankId!=filter.bankId) || (filter.favoritesOnly && !p.favorite))continue;
        if(!filter.tag.empty() && std::none_of(p.tags.begin(),p.tags.end(),[&](const auto& tag){return lower(tag)==lower(filter.tag);}))continue;
        const auto haystack=lower(p.patch.name+" "+p.bankName+" "+p.archiveEntry+" "+tagText(p.tags));
        if(std::all_of(terms.begin(),terms.end(),[&](const auto& term){return haystack.find(term)!=std::string::npos;}))result.push_back(i);
    }
    return result;
}
}
