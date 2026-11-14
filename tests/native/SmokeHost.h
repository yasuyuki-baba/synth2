#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace smoke
{
inline constexpr int frames = 128;
inline constexpr double sampleRate = 48000.0;
using State = std::vector<uint8_t>;
using Block = std::array<std::array<float, frames>, 2>;
enum class Midi { none, noteOn, noteOff };

inline void require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

// Test hosts use only public format APIs; no production test hooks or editor.
class Host
{
public:
    virtual ~Host() = default;
    virtual void load(const State&) = 0;
    virtual void render(Block&, Midi) = 0;
};

std::unique_ptr<Host> makeVst3(const std::filesystem::path&);
std::unique_ptr<Host> makeClap(const std::filesystem::path&);
#ifdef __APPLE__
std::unique_ptr<Host> makeAu(const std::filesystem::path&);
#endif
}
