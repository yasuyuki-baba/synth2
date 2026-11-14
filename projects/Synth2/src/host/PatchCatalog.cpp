#include "PatchCatalog.h"
#include "LibraryFiles.h"
#include "ProcessorBridge.h"
#include "PatchTagInference.h"
#include "patch/PatchFormat.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
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
std::string portablePath(const fs::path& path) {
    const auto text=path.generic_u8string();return {text.begin(),text.end()};
}
std::string filterPath(std::string path) {
#ifdef _WIN32
    // Read bank selections saved before portable directory bank IDs.
    std::replace(path.begin(),path.end(),'\\','/');
#endif
    return path;
}
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
struct TagOverrides {std::vector<std::string> manual,hidden;};
TagOverrides overridesAt(const fs::path& root,const std::string& id) {
    if(root.empty() || !fs::exists(tagPath(root,id)))return {};
    const auto text=read(tagPath(root,id),16384);
    const std::string header="Synth2 tag overrides v1\n";
    if(!text.starts_with(header))return {PatchCatalog::splitTags(text),{}};
    std::istringstream in(text.substr(header.size()));std::string manual,hidden,extra;
    if(!std::getline(in,manual) || !std::getline(in,hidden) || std::getline(in,extra))
        throw std::runtime_error("Invalid library tag overrides");
    return {PatchCatalog::splitTags(manual),PatchCatalog::splitTags(hidden)};
}
void storeOverrides(const fs::path& root,const std::string& id,const TagOverrides& tags) {
    fs::create_directories(tagPath(root,id).parent_path());
    if(tags.manual.empty() && tags.hidden.empty())fs::remove(tagPath(root,id));
    else replaceText(tagPath(root,id),"Synth2 tag overrides v1\n"+PatchCatalog::tagText(tags.manual)+"\n"+PatchCatalog::tagText(tags.hidden)+"\n");
}
void requireRoot(const fs::path& root) {
    if(root.empty())throw std::runtime_error("The user library folder is unavailable.");
}
}

std::vector<std::string> CatalogPatch::visibleAutomaticTags() const {
    std::vector<std::string> result;
    for(const auto& tag:automaticTags) {
        const auto matches=[&](const auto& other){return lower(other)==lower(tag);};
        if(std::none_of(tags.begin(),tags.end(),matches) && std::none_of(hiddenTags.begin(),hiddenTags.end(),matches))result.push_back(tag);
    }
    return result;
}
std::vector<std::string> CatalogPatch::allTags() const {
    auto result=tags;const auto inferred=visibleAutomaticTags();result.insert(result.end(),inferred.begin(),inferred.end());return result;
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
                    next.push_back({id,bankId,bankName,{read(bank/(key+".name"),16384),std::move(text)},ordinal,false,{}});
                }
            }
        }
        if(!directory.empty()) {
            PresetFiles files(directory);if(!files.scan(error,false))return false;
            for(const auto& item:files.entries()) {
                if(next.size()>=maximumCatalogPatches || item.text.size()>maximumCatalogBytes-total)throw std::runtime_error("The library exceeds its size limit.");
                total+=item.text.size();auto relative=item.file.lexically_relative(files.directory());
                // Archive entries and loose files share one directory-bank hierarchy.
                // Sound IDs, original entry names and source program ordinals stay intact.
                auto bankId=portablePath(relative.parent_path());
                if(isLibraryArchive(libraryFileKind(relative))) {
                    bankId=portablePath(relative);
                    auto entry=item.archiveEntry;
                    std::replace(entry.begin(),entry.end(),'\\','/');
                    const auto slash=entry.find_last_of('/');
                    if(slash!=std::string::npos && slash>0)bankId+="/"+entry.substr(0,slash);
                }
                if(bankId.empty())bankId=".";
                const auto bankName=bankId=="." ? "Library root" : bankId;
                next.push_back({item.id,bankId,bankName,{item.name,item.text,item.archiveEntry},item.program,false,{},item.file,item.archiveEntry,item.error,item.location});
            }
        }
        // Cache exact source text to detect even same-size/same-timestamp edits.
        // The cache is bounded and disposable; user overrides live separately.
        const auto cachePath=directory/".synth2-tags.cache";
        const std::string cacheHeader="Synth2 tag cache "+std::to_string(patchTagCacheVersion)+"\n";
        constexpr std::size_t cacheLimit=64*1024*1024;
        struct CachedTags {std::string source,error;std::vector<std::string> tags;};
        std::map<std::string,CachedTags> cached;
        std::string oldCache;
        try {
            oldCache=read(cachePath,cacheLimit);
            if(oldCache.starts_with(cacheHeader)) {
                std::istringstream in(oldCache.substr(cacheHeader.size()));
                for(std::string id,source,problem,tags;in>>std::ws && !in.eof();) {
                    if(cached.size()>=maximumCatalogPatches || !(in>>std::quoted(id)>>std::quoted(source)>>std::quoted(problem)>>std::quoted(tags)))
                        throw std::runtime_error("Invalid tag cache");
                    if(id.size()>32768 || source.size()>PatchLibrary::maximumPatchBytes || problem.size()>16384)
                        throw std::runtime_error("Invalid tag cache record");
                    cached.emplace(std::move(id),CachedTags{std::move(source),std::move(problem),splitTags(tags)});
                }
            }
        }catch(...) {cached.clear();}
        // Metadata is sparse: discover override files once rather than issuing
        // multiple filesystem queries for every sound in a large archive.
        const auto metadataPathKey=[](const fs::path& path) {
            auto key=utf8(path.lexically_normal());
#ifdef _WIN32
            key=lower(key);
#endif
            return key;
        };
        const auto discoverMetadata=[&](const char* folder,const char* extension) {
            std::unordered_set<std::string> files;
            if(!directory.empty() && fs::is_directory(directory/folder))
                for(const auto& file:fs::recursive_directory_iterator(directory/folder))
                    if(fs::is_regular_file(file.symlink_status()) && lower(file.path().extension().string())==extension)
                        files.insert(metadataPathKey(file.path()));
            return files;
        };
        const auto overrideFiles=discoverMetadata("tags",".tags");
        const auto favoriteFiles=discoverMetadata("favorites",".fav");
        ProcessorBridge scratch;TagCacheStats stats;
        std::ostringstream updated;updated<<cacheHeader;
        for(auto& patch:next) {
            patch.favorite=favoriteFiles.contains(metadataPathKey(favorites(directory,patch.id)));
            if(overrideFiles.contains(metadataPathKey(tagPath(directory,patch.id)))) {
                auto overrides=overridesAt(directory,patch.id);
                patch.tags=std::move(overrides.manual);patch.hiddenTags=std::move(overrides.hidden);
            }
            if(!patch.error.empty())continue;
            const auto found=cached.find(patch.id);
            if(found!=cached.end() && found->second.source==patch.patch.text) {
                patch.automaticTags=found->second.tags;patch.error=found->second.error;++stats.reused;
            }else {
                ++stats.rebuilt;
                if(scratch.loadPatch(patch.patch.text,patch.error))
                    patch.automaticTags=inferPatchTags(scratch.currentPatchName(),parseContinuousPatch(scratch.savePatch()).values);
            }
            std::ostringstream record;
            record<<std::quoted(patch.id)<<' '<<std::quoted(patch.patch.text)<<' '
                  <<std::quoted(patch.error)<<' '<<std::quoted(tagText(patch.automaticTags))<<'\n';
            const auto bytes=record.str();
            if(static_cast<std::size_t>(updated.tellp())+bytes.size()<=cacheLimit)updated<<bytes;
        }
        // Missing IDs disappear from this snapshot, pruning deleted sources.
        const auto newCache=updated.str();
        if(newCache!=oldCache && !directory.empty()) {
            try {fs::create_directories(directory);replaceText(cachePath,newCache);}catch(...) { /* Read-only libraries still work. */ }
        }
        cacheStats=stats;
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
        auto tags=splitTags(text);
        storeOverrides(directory,id,{tags,found->hiddenTags});
        found->tags=std::move(tags);error.clear();return true;
    } catch(const std::exception& e){error=e.what();return false;}
}
bool PatchCatalog::hideAutomaticTag(const std::string& id,const std::string& tag,std::string& error) {
    try {
        requireRoot(directory);
        const auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& p){return p.id==id;});
        if(found==entries.end())throw std::runtime_error("This patch is no longer in the library.");
        if(std::find(found->automaticTags.begin(),found->automaticTags.end(),tag)==found->automaticTags.end())
            throw std::runtime_error("This automatic tag is no longer available.");
        auto hidden=found->hiddenTags;
        if(std::none_of(hidden.begin(),hidden.end(),[&](const auto& other){return lower(tag)==lower(other);}))hidden.push_back(tag);
        storeOverrides(directory,id,{found->tags,hidden});found->hiddenTags=std::move(hidden);error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool PatchCatalog::restoreAutomaticTags(const std::string& id,std::string& error) {
    try {
        requireRoot(directory);
        const auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& p){return p.id==id;});
        if(found==entries.end())throw std::runtime_error("This patch is no longer in the library.");
        storeOverrides(directory,id,{found->tags,{}});found->hiddenTags.clear();error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool PatchCatalog::removePatch(const std::string& id,std::string& error) {
    try {
        requireRoot(directory);
        const auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& p){return p.id==id;});
        if(found==entries.end())throw std::runtime_error("This patch is no longer in the library.");
        if(!found->file.empty()) {
            if(!found->archiveEntry.empty())throw std::runtime_error("Remove the archive bank to remove its contained sounds.");
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
        const auto key=filterPath(bankId);
        const auto found=std::find_if(entries.begin(),entries.end(),[&](const auto& p){
            return p.file.empty() ? p.bankId==key :
                portablePath(p.file.lexically_relative(fs::weakly_canonical(directory)))==key;
        });
        if(found==entries.end())throw std::runtime_error("This bank is no longer in the library.");
        if(!found->file.empty()) {
            if(!fs::remove(found->file))throw std::runtime_error("The bank was already removed. Refresh the library.");
            const auto file=found->file;
            std::error_code ignored;for(const auto& p:entries)if(p.file==file){fs::remove(favorites(directory,p.id),ignored);fs::remove(tagPath(directory,p.id),ignored);}
            std::erase_if(entries,[&](const auto& p){return p.file==file;});error.clear();return true;
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
std::vector<CatalogBank> PatchCatalog::banks() const {
    std::map<std::string,std::string> unique;
    for(const auto& patch:entries) {
        unique.emplace(patch.bankId,patch.bankName);
        if(patch.file.empty())continue;
        for(auto slash=patch.bankId.find('/');slash!=std::string::npos;slash=patch.bankId.find('/',slash+1)) {
            const auto parent=patch.bankId.substr(0,slash);
            if(!parent.empty())unique.emplace(parent,parent);
        }
    }
    std::vector<CatalogBank> result;
    for(const auto& [id,name]:unique)result.push_back({id,name});
    std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.name<b.name;});
    return result;
}
std::vector<std::size_t> PatchCatalog::search(const CatalogFilter& filter)const {
    const auto bank=filterPath(filter.bankId);
    std::istringstream input(lower(filter.query));std::vector<std::string> terms;
    for(std::string term;input>>term;)terms.push_back(std::move(term));
    std::vector<std::size_t> result;
    for(std::size_t i=0;i<entries.size();++i) {
        const auto& p=entries[i];
        if(filter.registeredOnly && std::find(filter.registeredIds.begin(),filter.registeredIds.end(),p.id)==filter.registeredIds.end())continue;
        if((!bank.empty() && p.bankId!=bank) || (filter.favoritesOnly && !p.favorite))continue;
        const auto tags=p.allTags();
        const auto contains=[&](const auto& selected) {
            return std::any_of(tags.begin(),tags.end(),[&](const auto& tag){return lower(tag)==lower(selected);});
        };
        if((!filter.tag.empty() && !contains(filter.tag)) ||
            !std::all_of(filter.selectedTags.begin(),filter.selectedTags.end(),contains))continue;
        const auto haystack=lower(p.patch.name+" "+p.bankName+" "+p.location+" "+p.archiveEntry+" "+tagText(tags));
        if(std::all_of(terms.begin(),terms.end(),[&](const auto& term){return haystack.find(term)!=std::string::npos;}))result.push_back(i);
    }
    return result;
}
}
