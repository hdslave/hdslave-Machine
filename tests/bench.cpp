// hdslave Machine's cost: renders patches through the CLAP entry for SECONDS at 48 kHz in 128-frame blocks, as
// Melodee plays them, and prints the time a block takes here. tools/bench.sh runs it under callgrind for instruction
// counts (exact and repeatable, unlike timings on a computer that is not the MPC).
//   bench [worst|fm|kit|stopped|terrain|phase|vox|modal|pluck|gendy|pulsar|organ|kick|flood] [SECONDS]
// worst: FM TONE's eight voices held, every operator busy, the filter resonant and swept by the LFO, drive on.
// fm: a three-note chord on FM Keys. kit: KIT playing a busy groove. stopped: nothing sounding. A machine's name:
// eight of its voices held (struck ones struck again every second; KICK a hit every sixteenth). flood: ORGAN's
// eight held voices again, but a host block a frame, so every render fragment is one sample long -- the shape
// the per-voice coefficient cache answers to (count its instructions per 128 samples against organ's).
#include "../src/machine.cpp"

#include <cctype>
#include <chrono>
#include <string>
#include <vector>

int main (int argc, char** argv)
{
    const std::string mode = argc > 1 ? argv[1] : "worst";
    const double seconds = argc > 2 ? std::atof (argv[2]) : 10.0;
    constexpr double rate = 48000.0;
    const uint32_t block = mode == "flood" ? 1u : 128u;

    static const clap_host host { CLAP_VERSION, nullptr, "bench", "", "", "1", [] (const clap_host*, const char*) -> const void* { return nullptr; },
                                  [] (const clap_host*) {}, [] (const clap_host*) {}, [] (const clap_host*) {} };
    clap_entry.init ("");
    const auto* factory = static_cast<const clap_plugin_factory*> (clap_entry.get_factory (CLAP_PLUGIN_FACTORY_ID));
    const clap_plugin* plugin = factory->create_plugin (factory, &host, "com.hdslave.machine");
    plugin->init (plugin);
    plugin->activate (plugin, rate, 1, block);
    plugin->start_processing (plugin);

    using namespace machine::ParamId;
    std::vector<std::pair<uint32_t, double>> values;
    if (mode == "worst")
        values = { { dataA, 0.0 }, { fmALevel, 0.8 }, { fmBLevel, 0.8 }, { fmAEnd, 1.0 }, { fmBEnd, 1.0 }, { dataE, 0.9 }, { dataG, 0.6 },
                   { dataH, 0.5 }, { filterReso, 0.8 }, { filterFreq, 0.6 }, { filterEnv, 0.5 }, { lfoDest, 1 }, { lfoDepth, 0.7 },
                   { lfoSpeed, 0.7 }, { ampDrive, 0.5 } };
    else if (mode == "kit")
        values = { { machine::ParamId::machine, 8 }, { filterFreq, 0.85 }, { filterReso, 0.3 } };
    int kind = -1;
    for (int k = 9; k < machine::kindCount; ++k)
    {
        std::string name = machine::kindNames[k];
        for (auto& c : name) c = (char) std::tolower ((unsigned char) c);
        if (name == mode) kind = k;
    }
    if (mode == "flood") kind = machine::organ; // a cached machine held, as organ holds it, a frame a block
    if (kind >= 0) values = { { machine::ParamId::machine, (double) kind }, { filterFreq, 0.8 }, { filterReso, 0.3 } };

    struct Events
    {
        std::vector<clap_event_param_value> params;
        std::vector<clap_event_note> notes;
        std::vector<const clap_event_header*> order;
        clap_input_events in { this, [] (const clap_input_events* e) { return (uint32_t) static_cast<const Events*> (e->ctx)->order.size(); },
                               [] (const clap_input_events* e, uint32_t i) { return static_cast<const Events*> (e->ctx)->order[i]; } };
    };
    auto note = [] (uint32_t time, int16_t key, double velocity) {
        clap_event_note n {};
        n.header = { sizeof (n), time, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_NOTE_ON, 0 };
        n.note_id = -1, n.port_index = 0, n.channel = 0, n.key = key, n.velocity = velocity;
        return n;
    };
    if (mode == "fm")
    {
        const auto* presets = static_cast<const clap_plugin_preset_load*> (plugin->get_extension (plugin, CLAP_EXT_PRESET_LOAD));
        presets->from_location (plugin, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr, "fm-keys");
    }
    clap_output_events out { nullptr, [] (const clap_output_events*, const clap_event_header*) { return true; } };

    std::vector<float> left (block), right (block);
    float* outs[2] = { left.data(), right.data() };
    clap_audio_buffer buffer {};
    buffer.data32 = outs;
    buffer.channel_count = 2;
    const uint64_t blocks = (uint64_t) (seconds * rate / block);
    const int groove[16] = { 36, 39, 40, 39, 37, 39, 40, 39, 36, 36, 41, 39, 37, 42, 43, 38 };
    double sum = 0.0;
    const auto t0 = std::chrono::steady_clock::now();
    for (uint64_t b = 0; b < blocks; ++b)
    {
        Events e;
        e.params.reserve (values.size());
        e.notes.reserve (8);
        if (b == 0)
            for (auto& [id, v] : values)
            {
                clap_event_param_value p {};
                p.header = { sizeof (p), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0 };
                p.param_id = id;
                p.note_id = p.port_index = p.channel = p.key = -1;
                p.value = v;
                e.params.push_back (p);
            }
        if (mode == "worst" && b == 1)
            for (int k = 0; k < 8; ++k) e.notes.push_back (note (0, (int16_t) (36 + k * 5), 1.0));
        if (kind >= 0 && kind != machine::kick && (b == 1 || (machine::struck ((uint8_t) kind) && b % 375 == 1)))
            for (int k = 0; k < 8; ++k) e.notes.push_back (note (0, (int16_t) (40 + k * 5), 0.9));
        if (kind == machine::kick && b % 47 == 1) e.notes.push_back (note (0, 36, 0.9));
        if (mode == "fm" && b % 375 == 1)
            for (int k : { 57, 60, 64 }) e.notes.push_back (note (0, (int16_t) k, 0.8));
        if (mode == "kit" && b % 47 == 1) // sixteenths at 120 bpm, two hits on the downbeats
        {
            const int step = (int) (b / 47);
            e.notes.push_back (note (0, (int16_t) groove[step % 16], 0.9));
            if (step % 4 == 0) e.notes.push_back (note (0, 39, 0.6));
        }
        for (auto& p : e.params) e.order.push_back (&p.header);
        for (auto& n : e.notes) e.order.push_back (&n.header);
        clap_process p {};
        p.frames_count = block;
        p.audio_outputs = &buffer;
        p.audio_outputs_count = 1;
        p.in_events = &e.in;
        p.out_events = &out;
        plugin->process (plugin, &p);
        sum += left[0];
    }
    const double us = std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("%s: %llu blocks, %.2f us a block here (checksum %.6f)\n", mode.c_str(), (unsigned long long) blocks, us / (double) blocks, sum);
    plugin->stop_processing (plugin);
    plugin->deactivate (plugin);
    plugin->destroy (plugin);
    clap_entry.deinit();
    return 0;
}
