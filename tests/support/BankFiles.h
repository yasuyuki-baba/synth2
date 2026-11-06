#pragma once
#include <miniz.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace testfiles {
struct Directory {
    std::filesystem::path path;
    Directory(){static std::atomic<unsigned> next{};path=std::filesystem::temp_directory_path()/("synth2-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(next++));if(!std::filesystem::create_directory(path))throw std::runtime_error("Cannot create test directory");}
    ~Directory(){std::error_code e;std::filesystem::remove_all(path,e);}
};
inline bool zip(const std::filesystem::path& path,const std::vector<std::pair<std::string,std::string>>& files) {
    mz_zip_archive zip{};if(!mz_zip_writer_init_file(&zip,path.string().c_str(),0))return false;
    bool ok=true;for(const auto& [name,text]:files)ok=ok && mz_zip_writer_add_mem(&zip,name.c_str(),text.data(),text.size(),MZ_BEST_COMPRESSION);
    ok=ok && mz_zip_writer_finalize_archive(&zip);mz_zip_writer_end(&zip);return ok;
}
}
