#pragma once

#include "SmokeHost.h"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace smoke
{
class Module
{
public:
    explicit Module(const std::filesystem::path& binary)
    {
#ifdef _WIN32
        handle = LoadLibraryW(binary.c_str());
#else
        handle = dlopen(binary.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
        require(handle != nullptr, "Cannot load module: " + binary.string());
#ifdef __APPLE__
        auto root = binary.parent_path().parent_path().parent_path();
        auto url = CFURLCreateFromFileSystemRepresentation(nullptr,
            reinterpret_cast<const UInt8*>(root.c_str()), root.string().size(), true);
        bundle = CFBundleCreate(nullptr, url);
        CFRelease(url);
        require(bundle != nullptr, "Cannot open plugin bundle");
#endif
    }
    ~Module()
    {
#ifdef __APPLE__
        if (bundle) CFRelease(bundle);
#endif
#ifdef _WIN32
        if (handle) FreeLibrary(handle);
#else
        if (handle) dlclose(handle);
#endif
    }
    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;
    template<class T> T symbol(const char* name) const
    {
#ifdef _WIN32
        auto result = GetProcAddress(handle, name);
#else
        auto result = dlsym(handle, name);
#endif
        require(result != nullptr, std::string("Missing module export: ") + name);
        return reinterpret_cast<T>(result);
    }
    void* nativeHandle() const { return handle; }
#ifdef __APPLE__
    CFBundleRef bundle = nullptr;
#endif
private:
#ifdef _WIN32
    HMODULE handle = nullptr;
#else
    void* handle = nullptr;
#endif
};
}
