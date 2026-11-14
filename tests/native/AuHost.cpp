#include "Module.h"
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioUnit/AudioUnitProperties.h>

namespace smoke
{
namespace
{
void checked(OSStatus result, const char* operation)
{
    require(result == noErr, std::string(operation) + " failed: " + std::to_string(result));
}

CFStringRef string(CFDictionaryRef dictionary, CFStringRef key)
{
    const auto value = CFDictionaryGetValue(dictionary, key);
    require(value && CFGetTypeID(value) == CFStringGetTypeID(), "Missing AU descriptor string");
    return static_cast<CFStringRef>(value);
}

OSType code(CFDictionaryRef dictionary, CFStringRef key)
{
    char text[5]{};
    require(CFStringGetCString(string(dictionary, key), text, sizeof(text), kCFStringEncodingASCII) &&
        std::char_traits<char>::length(text) == 4, "Invalid AU descriptor code");
    OSType result = 0;
    for (int i = 0; i < 4; ++i) result = (result << 8) | static_cast<unsigned char>(text[i]);
    return result;
}

class AuHost final : public Host
{
public:
    explicit AuHost(const std::filesystem::path& binary) : module(binary)
    {
        // Register this exact build's factory in-process; no system installation,
        // cached AudioComponent discovery, GUI or audio device is involved.
        const auto entries = CFBundleGetValueForInfoDictionaryKey(module.bundle, CFSTR("AudioComponents"));
        require(entries && CFGetTypeID(entries) == CFArrayGetTypeID() &&
            CFArrayGetCount(static_cast<CFArrayRef>(entries)) == 1, "AU descriptor array missing");
        const auto value = CFArrayGetValueAtIndex(static_cast<CFArrayRef>(entries), 0);
        require(CFGetTypeID(value) == CFDictionaryGetTypeID(), "AU descriptor missing");
        const auto descriptor = static_cast<CFDictionaryRef>(value);
        AudioComponentDescription description{};
        description.componentType = code(descriptor, CFSTR("type"));
        description.componentSubType = code(descriptor, CFSTR("subtype"));
        description.componentManufacturer = code(descriptor, CFSTR("manufacturer"));
        const auto factory = reinterpret_cast<AudioComponentFactoryFunction>(
            CFBundleGetFunctionPointerForName(module.bundle, string(descriptor, CFSTR("factoryFunction"))));
        require(factory != nullptr, "AU factory export missing");
        int32_t version = 0;
        const auto number = CFDictionaryGetValue(descriptor, CFSTR("version"));
        require(number && CFGetTypeID(number) == CFNumberGetTypeID() &&
            CFNumberGetValue(static_cast<CFNumberRef>(number), kCFNumberSInt32Type, &version),
            "AU version missing");
        const auto component = AudioComponentRegister(&description, string(descriptor, CFSTR("name")), version, factory);
        require(component != nullptr, "AU registration failed");
        checked(AudioComponentInstanceNew(component, &unit), "AudioComponentInstanceNew");
        AudioStreamBasicDescription format{};
        format.mSampleRate = sampleRate;
        format.mFormatID = kAudioFormatLinearPCM;
        format.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
        format.mBytesPerPacket = format.mBytesPerFrame = sizeof(float);
        format.mFramesPerPacket = 1;
        format.mChannelsPerFrame = 2;
        format.mBitsPerChannel = 32;
        checked(AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0,
            &format, sizeof(format)), "AU stream format");
        const UInt32 maximum = frames;
        checked(AudioUnitSetProperty(unit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0,
            &maximum, sizeof(maximum)), "AU maximum frames");
    }
    ~AuHost() override
    {
        if (initialized) AudioUnitUninitialize(unit);
        if (unit) AudioComponentInstanceDispose(unit);
    }
    void load(const State& state) override
    {
        if (initialized) checked(AudioUnitUninitialize(unit), "AU uninitialize");
        initialized = false;
        CFPropertyListRef current = nullptr;
        UInt32 size = sizeof(current);
        checked(AudioUnitGetProperty(unit, kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global, 0,
            &current, &size), "AU get ClassInfo");
        require(current && CFGetTypeID(current) == CFDictionaryGetTypeID(), "AU ClassInfo is not a dictionary");
        auto dictionary = CFDictionaryCreateMutableCopy(nullptr, 0, static_cast<CFDictionaryRef>(current));
        CFRelease(current);
        auto data = CFDataCreate(nullptr, state.data(), state.size());
        auto key = CFStringCreateWithCString(nullptr, kAUPresetDataKey, kCFStringEncodingUTF8);
        CFDictionarySetValue(dictionary, key, data);
        CFRelease(key);
        CFRelease(data);
        CFPropertyListRef property = dictionary;
        const auto result = AudioUnitSetProperty(unit, kAudioUnitProperty_ClassInfo, kAudioUnitScope_Global, 0,
            &property, sizeof(property));
        CFRelease(dictionary);
        checked(result, "AU set ClassInfo");
        checked(AudioUnitInitialize(unit), "AudioUnitInitialize");
        initialized = true;
        position = 0;
    }
    void render(Block& output, Midi midi) override
    {
        if (midi != Midi::none)
            checked(MusicDeviceMIDIEvent(unit, midi == Midi::noteOn ? 0x90 : 0x80, 69,
                midi == Midi::noteOn ? 127 : 0, 0), "AU MIDI event");
        struct Buffers { UInt32 count; AudioBuffer channels[2]; } buffers{
            2, {{1, UInt32(frames * sizeof(float)), output[0].data()},
                {1, UInt32(frames * sizeof(float)), output[1].data()}}};
        AudioTimeStamp time{};
        time.mFlags = kAudioTimeStampSampleTimeValid;
        time.mSampleTime = position;
        AudioUnitRenderActionFlags flags = 0;
        checked(AudioUnitRender(unit, &flags, &time, 0, frames,
            reinterpret_cast<AudioBufferList*>(&buffers)), "AudioUnitRender");
        position += frames;
    }
private:
    Module module;
    AudioUnit unit = nullptr;
    bool initialized = false;
    double position = 0;
};
}
std::unique_ptr<Host> makeAu(const std::filesystem::path& binary)
{
    return std::make_unique<AuHost>(binary);
}
}
