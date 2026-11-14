#include "Module.h"
#include <clap/clap.h>
#include <algorithm>
#include <cstring>

namespace smoke
{
namespace
{
class ClapHost final : public Host
{
public:
    explicit ClapHost(const std::filesystem::path& binary) : module(binary)
    {
        entry = module.symbol<const clap_plugin_entry_t*>("clap_entry");
        auto pluginPath = binary;
#ifdef __APPLE__
        pluginPath = binary.parent_path().parent_path().parent_path();
#endif
        const auto path = pluginPath.u8string();
        require(entry->init(reinterpret_cast<const char*>(path.c_str())), "CLAP entry init failed");
        const auto* factory = static_cast<const clap_plugin_factory_t*>(
            entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
        require(factory && factory->get_plugin_count(factory) == 1, "Expected one CLAP plugin");
        const auto* descriptor = factory->get_plugin_descriptor(factory, 0);
        require(descriptor != nullptr, "CLAP descriptor missing");
        plugin = factory->create_plugin(factory, &host, descriptor->id);
        require(plugin && plugin->init(plugin), "CLAP plugin init failed");
        state = static_cast<const clap_plugin_state_t*>(plugin->get_extension(plugin, CLAP_EXT_STATE));
        require(state != nullptr, "CLAP state extension missing");
    }
    ~ClapHost() override
    {
        stop();
        if (plugin) plugin->destroy(plugin);
        if (entry) entry->deinit();
    }
    void load(const State& bytes) override
    {
        stop();
        struct Reader { const State& bytes; size_t position = 0; } reader{bytes};
        clap_istream_t stream{};
        stream.ctx = &reader;
        stream.read = [](const clap_istream_t* stream, void* buffer, uint64_t count) -> int64_t {
            auto& source = *static_cast<Reader*>(stream->ctx);
            const auto n = std::min<uint64_t>(count, source.bytes.size() - source.position);
            std::memcpy(buffer, source.bytes.data() + source.position, size_t(n));
            source.position += size_t(n);
            return int64_t(n);
        };
        require(state->load(plugin, &stream), "CLAP state load failed");
        require(plugin->activate(plugin, sampleRate, frames, frames), "CLAP activate failed");
        active = true;
        require(plugin->start_processing(plugin), "CLAP start_processing failed");
        processing = true;
        position = 0;
    }
    void render(Block& output, Midi midi) override
    {
        clap_event_midi_t event{};
        event.header.size = sizeof(event);
        event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        event.header.type = CLAP_EVENT_MIDI;
        event.data[0] = midi == Midi::noteOn ? 0x90 : 0x80;
        event.data[1] = 69;
        event.data[2] = midi == Midi::noteOn ? 127 : 0;
        clap_input_events_t input{};
        input.ctx = midi == Midi::none ? nullptr : &event;
        input.size = [](const clap_input_events_t* list) -> uint32_t { return list->ctx ? 1 : 0; };
        input.get = [](const clap_input_events_t* list, uint32_t index) -> const clap_event_header_t* {
            return index == 0 && list->ctx ? &static_cast<clap_event_midi_t*>(list->ctx)->header : nullptr;
        };
        clap_output_events_t events{};
        events.try_push = [](const clap_output_events_t*, const clap_event_header_t*) { return true; };
        float* channels[]{output[0].data(), output[1].data()};
        clap_audio_buffer_t audio{};
        audio.data32 = channels;
        audio.channel_count = 2;
        clap_event_transport_t transport{};
        transport.header.size = sizeof(transport);
        transport.header.type = CLAP_EVENT_TRANSPORT;
        transport.flags = CLAP_TRANSPORT_HAS_TEMPO;
        transport.tempo = 120.0;
        clap_process_t process{};
        process.steady_time = position;
        process.frames_count = frames;
        process.transport = &transport;
        process.audio_outputs = &audio;
        process.audio_outputs_count = 1;
        process.in_events = &input;
        process.out_events = &events;
        require(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR, "CLAP process failed");
        position += frames;
    }
private:
    void stop()
    {
        if (processing) plugin->stop_processing(plugin);
        if (active) plugin->deactivate(plugin);
        processing = active = false;
    }
    Module module;
    const clap_plugin_entry_t* entry = nullptr;
    const clap_plugin_t* plugin = nullptr;
    const clap_plugin_state_t* state = nullptr;
    bool active = false, processing = false;
    int64_t position = 0;
    clap_host_t host{
        CLAP_VERSION, this, "Synth2 audio smoke", "Synth2", "https://github.com/yasuyuki-baba/synth2", "1",
        [](const clap_host_t*, const char*) -> const void* { return nullptr; },
        [](const clap_host_t*) {}, [](const clap_host_t*) {}, [](const clap_host_t*) {}};
};
}
std::unique_ptr<Host> makeClap(const std::filesystem::path& binary)
{
    return std::make_unique<ClapHost>(binary);
}
}
