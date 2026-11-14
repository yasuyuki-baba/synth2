#include "SmokeHost.h"
#include "host/PresetFiles.h"
#include "host/ProcessorBridge.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>

namespace
{
smoke::State patchState(const std::filesystem::path& file, int factor)
{
    std::ifstream input(file, std::ios::binary);
    smoke::require(input.good(), "Cannot read patch: " + file.string());
    const std::string text{std::istreambuf_iterator<char>(input), {}};
    synth2::host::ProcessorBridge processor;
    std::string error;
    smoke::require(processor.loadPatch(text, error), "Patch load failed: " + error);
    smoke::require(processor.setOversamplingFactor(factor), "Invalid quality factor");
    synth2::host::PresetBrowserState browser;
    browser.capture(processor);
    // This encodes the public session format, not a reference audio oracle.
    return synth2::host::savePresetSession(processor, browser);
}

struct Signal
{
    double energy = 0.0, peak = 0.0;
    int samples = 0;
    void add(const smoke::Block& block)
    {
        for (const auto& channel : block)
            for (const float value : channel)
            {
                smoke::require(std::isfinite(value), "Non-finite output sample");
                energy += double(value) * value;
                peak = std::max(peak, std::abs(double(value)));
                ++samples;
            }
    }
    double rms() const { return std::sqrt(energy / samples); }
};

Signal render(smoke::Host& host, int blocks, smoke::Midi midi)
{
    Signal result;
    for (int i = 0; i < blocks; ++i)
    {
        smoke::Block output{};
        host.render(output, i == 0 ? midi : smoke::Midi::none);
        result.add(output);
    }
    return result;
}

void check(const std::string& label, const Signal& signal, bool audible, double silenceLimit = 1e-8)
{
    std::cout << label << " rms=" << signal.rms() << " peak=" << signal.peak << std::endl;
    // Authored patch contract: zero Amp Level is silent; the open sustained
    // tone must be audible. No waveform fitting, alignment or golden recordings.
    smoke::require(audible ? signal.rms() > 1e-4 : signal.peak <= silenceLimit,
        label + (audible ? ": expected sound" : ": expected silence"));
}
}

int main(int argc, char** argv)
{
    try
    {
        smoke::require(argc == 4, "Usage: Synth2AudioSmoke <vst3|clap|au> <binary> <patch-directory>");
        const std::string format = argv[1];
        std::unique_ptr<smoke::Host> host;
#ifdef SYNTH2_SMOKE_VST3
        if (format == "vst3") host = smoke::makeVst3(argv[2]);
#endif
#ifdef SYNTH2_SMOKE_CLAP
        if (format == "clap") host = smoke::makeClap(argv[2]);
#endif
#ifdef SYNTH2_SMOKE_AU
        if (format == "au") host = smoke::makeAu(argv[2]);
#endif
        smoke::require(bool(host), "Unknown format: " + format);
        for (const int factor : {1, 8})
        {
            // Reuse the same instance: a successful cold-start alone would not
            // catch a later state load leaving the preceding patch sounding.
            for (const int patch : {0, 1, 0})
            {
                const bool audible = patch == 0;
                const auto name = audible ? "0-tone.sy1" : "1-mute.sy1";
                const auto label = format + " " + std::to_string(factor) + "x " + name;
                host->load(patchState(std::filesystem::path(argv[3]) / name, factor));
                // The native EQ silence graph deliberately emits a sub-1e-6
                // fill on an unmuted idle path (also covered by PlaybackBehavior).
                // Zero Amp Level must still suppress it entirely.
                const double silenceLimit = audible ? 1e-6 : 1e-8;
                check(label + " before MIDI", render(*host, 16, smoke::Midi::none), false, silenceLimit);
                check(label + " held note", render(*host, 96, smoke::Midi::noteOn), audible);
                render(*host, 96, smoke::Midi::noteOff); // Allow the zero-release envelope/FIR to drain.
                check(label + " released", render(*host, 16, smoke::Midi::none), false, silenceLimit);
            }
        }
        std::cout << "PASS: " << format << " patch/state/MIDI/audio smoke" << std::endl;
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << std::endl;
        return 1;
    }
}
