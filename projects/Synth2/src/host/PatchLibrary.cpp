#include "PatchLibrary.h"
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
bool naturalLess(const std::string& a,const std::string& b)
{
    size_t i=0,j=0;
    while(i<a.size() && j<b.size())
    {
        if(std::isdigit(static_cast<unsigned char>(a[i])) && std::isdigit(static_cast<unsigned char>(b[j])))
        {
            auto endA=i,endB=j;while(endA<a.size() && std::isdigit(static_cast<unsigned char>(a[endA])))++endA;
            while(endB<b.size() && std::isdigit(static_cast<unsigned char>(b[endB])))++endB;
            auto firstA=i,firstB=j;while(firstA+1<endA && a[firstA]=='0')++firstA;while(firstB+1<endB && b[firstB]=='0')++firstB;
            if(endA-firstA!=endB-firstB)return endA-firstA<endB-firstB;
            const int order=a.compare(firstA,endA-firstA,b,firstB,endB-firstB);if(order)return order<0;
            i=endA;j=endB;continue;
        }
        const auto ac=std::tolower(static_cast<unsigned char>(a[i++])),bc=std::tolower(static_cast<unsigned char>(b[j++]));
        if(ac!=bc)return ac<bc;
    }
    return i==a.size() && j!=b.size();
}
}
bool PatchLibrary::openFile(const std::string& path,std::string& error)
{
    try
    {
        const auto file=std::filesystem::path(std::u8string(path.begin(),path.end()));const auto extension=lower(file.extension().string());
        if(extension!=".sy1" && extension!=".zip" && extension!=".rar"){error="Choose a .sy1 patch, ZIP or RAR bank.";return false;}
        const auto size=std::filesystem::file_size(file);
        if(size==0 || size>(extension==".sy1" ? maximumPatchBytes : maximumBankBytes))
        {error="The selected file is empty or exceeds the bank size limit.";return false;}
        std::ifstream input(file,std::ios::binary);std::string data(static_cast<size_t>(size),'\0');
        if(!input.read(data.data(),static_cast<std::streamsize>(size)) || input.peek()!=std::char_traits<char>::eof())
        {error="The selected file could not be read completely.";return false;}
        const auto filename=file.filename().u8string();const std::string name(filename.begin(),filename.end());
        if(extension==".zip")return openZip(data.data(),data.size(),name,error);
        if(extension==".rar")return openRar(data.data(),data.size(),name,error);
        std::vector<LibraryPatch> next{{title(data,name),data}};entries.swap(next);bankName=name;error.clear();return true;
    }
    catch(const std::exception& e){error=e.what();return false;}
}
bool PatchLibrary::openDirectory(const std::string& path,std::string& error)
{
    try {
        namespace fs=std::filesystem;
        const auto root=fs::path(std::u8string(path.begin(),path.end()));
        std::vector<fs::path> files;std::size_t total=0;
        for(const auto& file:fs::recursive_directory_iterator(root)) {
            if(!fs::is_regular_file(file.symlink_status()) || lower(file.path().extension().string())!=".sy1")continue;
            const auto bytes=file.file_size();
            if(files.size()>=maximumPatches || bytes==0 || bytes>maximumPatchBytes || bytes>maximumBankBytes-total)
                throw std::runtime_error("The folder contains an empty patch or exceeds the bank size limits.");
            total+=static_cast<std::size_t>(bytes);files.push_back(file.path());
        }
        const auto utf8=[](const fs::path& p){const auto s=p.generic_u8string();return std::string(s.begin(),s.end());};
        std::stable_sort(files.begin(),files.end(),[&](const auto& a,const auto& b){return naturalLess(utf8(a.lexically_relative(root)),utf8(b.lexically_relative(root)));});
        if(files.empty()){error="No .sy1 patches were found in this folder.";return false;}
        std::vector<LibraryPatch> next;next.reserve(files.size());total=0;
        for(const auto& file:files) {
            PatchLibrary single;if(!single.openFile(utf8(file),error))return false;
            auto patch=single.patches().front();total+=patch.text.size();
            if(total>maximumBankBytes)throw std::runtime_error("The folder exceeds the bank size limit.");
            patch.name=title(patch.text,utf8(file.lexically_relative(root)));next.push_back(std::move(patch));
        }
        auto label=root.filename();if(label.empty())label=root.parent_path().filename();
        entries.swap(next);bankName=label.empty() ? "Imported folder" : utf8(label);error.clear();return true;
    } catch(const std::exception& e){error=e.what();return false;}
}
bool PatchLibrary::openZip(const void* bytes,size_t size,const std::string& name,std::string& error)
{
    mz_zip_archive zip{};
    if(!mz_zip_reader_init_mem(&zip,bytes,size,0)){error="The ZIP bank could not be read.";return false;}
    struct Close{mz_zip_archive& zip;~Close(){mz_zip_reader_end(&zip);}} close{zip};
    struct Entry{mz_uint index;std::string name;size_t size;};std::vector<Entry> order;size_t total=0;
    for(mz_uint i=0;i<mz_zip_reader_get_num_files(&zip);++i)
    {
        mz_zip_archive_file_stat entry{};
        if(!mz_zip_reader_file_stat(&zip,i,&entry)){error="The ZIP directory is invalid.";return false;}
        const auto filenameBytes=mz_zip_reader_get_filename(&zip,i,nullptr,0);
        if(filenameBytes==0){error="The ZIP filename is invalid.";return false;}
        std::string filename(filenameBytes,'\0');
        if(mz_zip_reader_get_filename(&zip,i,filename.data(),filenameBytes)!=filenameBytes){error="The ZIP filename is invalid.";return false;}
        filename.resize(filenameBytes-1);
        const auto mode=(entry.m_external_attr>>16)&0170000;
        if(entry.m_is_directory || mode==0120000 || isMacMetadata(filename) || filename.size()<4 || lower(filename.substr(filename.size()-4))!=".sy1")continue;
        if(entry.m_uncomp_size==0 || entry.m_uncomp_size>maximumPatchBytes || entry.m_uncomp_size>maximumBankBytes-total || order.size()==maximumPatches)
        {error="The bank contains an empty patch or exceeds the bank size limits.";return false;}
        total+=static_cast<size_t>(entry.m_uncomp_size);order.push_back({i,filename,static_cast<size_t>(entry.m_uncomp_size)});
    }
    if(order.empty()){error="No .sy1 patches were found in the selected bank.";return false;}
    std::stable_sort(order.begin(),order.end(),[](const auto& a,const auto& b){return naturalLess(a.name,b.name);});
    std::vector<LibraryPatch> next;next.reserve(order.size());
    for(const auto& e:order)
    {
        std::string data(e.size,'\0');
        if(!mz_zip_reader_extract_to_mem(&zip,e.index,data.data(),data.size(),0))
        {error="A patch in the bank could not be read completely.";return false;}
        next.push_back({title(data,e.name),std::move(data),e.name});
    }
    entries.swap(next);bankName=name;error.clear();return true;
}
bool PatchLibrary::openRar(const void* bytes,size_t size,const std::string& name,std::string& error)
{
#ifndef _WIN32
    ArchiveLocale textLocale;
    if(!textLocale.previous){error="A UTF-8 thread locale is required to read RAR filenames.";return false;}
#endif
    std::unique_ptr<archive,decltype(&archive_read_free)> reader(archive_read_new(),archive_read_free);
    const auto fail=[&](const char* message) {
        error=message;
        if(reader && archive_error_string(reader.get()))error+=" "+std::string(archive_error_string(reader.get()));
        return false;
    };
    if(!reader || archive_read_support_format_rar(reader.get())!=ARCHIVE_OK ||
       archive_read_support_format_rar5(reader.get())!=ARCHIVE_OK ||
       archive_read_open_memory(reader.get(),bytes,size)!=ARCHIVE_OK)
        return fail("The RAR bank could not be read.");
    std::vector<LibraryPatch> next;
    size_t total=0,count=0;
    archive_entry* entry=nullptr;
    int status;
    while((status=archive_read_next_header(reader.get(),&entry))==ARCHIVE_OK)
    {
        if(++count>maximumPatches){error="The RAR bank contains too many entries.";return false;}
        if(archive_entry_is_encrypted(entry)>0)
        {error="Password-protected RAR banks are not supported.";return false;}
        const auto* path=archive_entry_pathname_utf8(entry);
        if(!path)return fail("The RAR filename could not be read as UTF-8.");
        const std::string filename(path);
        const auto length=archive_entry_size(entry);
        // Skipping a solid entry may decompress it to build the next entry's
        // dictionary. Bound all declared data, including non-patch files.
        if(!archive_entry_size_is_set(entry) || length<0 ||
           static_cast<uint64_t>(length)>maximumBankBytes-total)
        {error="The RAR bank exceeds the bank size limit.";return false;}
        total+=static_cast<size_t>(length);
        const bool patch=archive_entry_filetype(entry)==AE_IFREG &&
            !archive_entry_symlink(entry) && !archive_entry_hardlink(entry) &&
            !isMacMetadata(filename) && filename.size()>=4 &&
            lower(filename.substr(filename.size()-4))==".sy1";
        if(!patch)
        {
            if(archive_read_data_skip(reader.get())!=ARCHIVE_OK)
                return fail("An entry in the RAR bank could not be skipped.");
            continue;
        }
        if(length==0 || static_cast<uint64_t>(length)>maximumPatchBytes)
        {error="The bank contains an empty patch or exceeds the bank size limits.";return false;}
        std::string data(static_cast<size_t>(length),'\0');
        size_t offset=0;
        while(offset<data.size())
        {
            const auto n=archive_read_data(reader.get(),data.data()+offset,data.size()-offset);
            if(n<=0)return fail("A patch in the RAR bank could not be read completely.");
            offset+=static_cast<size_t>(n);
        }
        // Read through EOF: the decoder verifies the entry's checksum here.
        char extra;
        if(archive_read_data(reader.get(),&extra,1)!=0)
            return fail("A patch in the RAR bank failed size or checksum validation.");
        next.push_back({title(data,filename),std::move(data),filename});
    }
    if(status!=ARCHIVE_EOF)return fail("The RAR directory is invalid or incomplete.");
    if(next.empty()){error="No .sy1 patches were found in the selected bank.";return false;}
    std::stable_sort(next.begin(),next.end(),[](const auto& a,const auto& b){return naturalLess(a.entry,b.entry);});
    entries.swap(next);bankName=name;error.clear();return true;
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
