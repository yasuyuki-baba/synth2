#include "PatchLibrary.h"
#include "patch/PatchLoader.h"
#include "patch/PatchFormat.h"
#include <miniz.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <memory>

namespace synth2::host
{
namespace
{
std::string lower(std::string s){for(auto& c:s)if(static_cast<unsigned char>(c)<128)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
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
        if(line.compare(0,4,"ver=")!=0 && line.find(',')==std::string::npos)return filename+"  -  "+line;
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
        if(extension!=".sy1" && extension!=".zip"){error="Choose a .sy1 patch or a .zip bank.";return false;}
        const auto size=std::filesystem::file_size(file);
        if(size==0 || size>(extension==".zip" ? maximumBankBytes : maximumPatchBytes))
        {error="The selected file is empty or exceeds the bank size limit.";return false;}
        std::ifstream input(file,std::ios::binary);std::string data(static_cast<size_t>(size),'\0');
        if(!input.read(data.data(),static_cast<std::streamsize>(size)) || input.peek()!=std::char_traits<char>::eof())
        {error="The selected file could not be read completely.";return false;}
        const auto filename=file.filename().u8string();const std::string name(filename.begin(),filename.end());
        if(extension==".zip")return openZip(data.data(),data.size(),name,error);
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
        const auto utf8=[](const fs::path& p){const auto s=p.u8string();return std::string(s.begin(),s.end());};
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
        if(entry.m_is_directory || mode==0120000 || filename.size()<4 || lower(filename.substr(filename.size()-4))!=".sy1")continue;
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
        next.push_back({title(data,e.name),std::move(data)});
    }
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
