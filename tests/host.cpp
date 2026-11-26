// Loads Machine.clap as a host does and checks it: description and ports, parameters and their text, state, presets,
// FM TONE (pitch, ratios, envelopes, every algorithm, polyphony, release, bend), every drum machine (it sounds, it ends,
// it ignores note-off, a new hit chokes the last), a new machine resetting its Data, KIT (its keys, pan, a closed hat
// choking an open one), the same output whatever the block size, and finite output at random and extreme settings.
// Writes demo renders as WAV files into the output folder.
//   host Machine.clap OUT_FOLDER
#include "wav.h"

#include <clap/clap.h>
#include "../src/melodee_screen.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <functional>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace
{
int failures = 0, checks = 0;
#define CHECK(c) do { ++checks; if (! (c)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

const clap_plugin_entry* entry = nullptr;
const clap_plugin_factory* factory = nullptr;

unsigned screenRuns = 0;
int rescans = 0, presetsLoaded = 0, presetErrors = 0, processRequests = 0, flushRequests = 0;
const void* hostExtension (const clap_host*, const char* id)
{
    static const clap_host_log log { [] (const clap_host*, clap_log_severity s, const char* m) { std::printf ("  [plug-in log %d] %s\n", (int) s, m); } };
    static const clap_host_params params { [] (const clap_host*, clap_param_rescan_flags f) { rescans += (f & CLAP_PARAM_RESCAN_VALUES) != 0; },
                                           [] (const clap_host*, clap_id, clap_param_clear_flags) {}, [] (const clap_host*) { ++flushRequests; } };
    static const clap_host_preset_load presetLoad {
        [] (const clap_host*, uint32_t, const char*, const char*, int32_t, const char*) { ++presetErrors; },
        [] (const clap_host*, uint32_t, const char*, const char*) { ++presetsLoaded; },
    };
    static const melodee_screen_host_t screenHost { [] (const void*, uint32_t flags) { screenRuns += (flags & MELODEE_SCREEN_RUN) != 0; } };
    if (std::strcmp (id, MELODEE_SCREEN_CLAP_HOST) == 0) return &screenHost;
    if (std::strcmp (id, CLAP_EXT_LOG) == 0) return &log;
    if (std::strcmp (id, CLAP_EXT_PARAMS) == 0) return &params;
    if (std::strcmp (id, CLAP_EXT_PRESET_LOAD) == 0) return &presetLoad;
    return nullptr;
}

clap_host host { CLAP_VERSION, nullptr, "Machine test host", "tests", "", "1", hostExtension, [] (const clap_host*) {}, [] (const clap_host*) { ++processRequests; },
                 [] (const clap_host*) {} };

struct Events
{
    struct Any { union { clap_event_header header; clap_event_param_value param; clap_event_note note; clap_event_midi midi; }; };
    std::vector<Any> list;
    clap_input_events in { this, [] (const clap_input_events* e) { return (uint32_t) static_cast<const Events*> (e->ctx)->list.size(); },
                           [] (const clap_input_events* e, uint32_t i) { return &static_cast<const Events*> (e->ctx)->list[i].header; } };
    void param (uint32_t time, clap_id id, double value)
    {
        Any a {};
        a.param = { { sizeof (clap_event_param_value), time, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0 }, id, nullptr, -1, -1, -1, -1, value };
        list.push_back (a);
    }
    void note (uint32_t time, uint16_t type, int16_t key, double velocity, int32_t noteId = -1)
    {
        Any a {};
        a.note = { { sizeof (clap_event_note), time, CLAP_CORE_EVENT_SPACE_ID, type, 0 }, noteId, 0, 0, key, velocity };
        list.push_back (a);
    }
    void midi (uint32_t time, uint8_t s, uint8_t d1, uint8_t d2)
    {
        Any a {};
        a.midi = { { sizeof (clap_event_midi), time, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI, 0 }, 0, { s, d1, d2 } };
        list.push_back (a);
    }
    void sort() { std::stable_sort (list.begin(), list.end(), [] (const Any& a, const Any& b) { return a.header.time < b.header.time; }); }
};

struct OutEvents
{
    std::vector<std::pair<clap_id, double>> values;
    clap_output_events out { this, [] (const clap_output_events* o, const clap_event_header* h) {
        if (h->space_id == CLAP_CORE_EVENT_SPACE_ID && h->type == CLAP_EVENT_PARAM_VALUE)
        {
            const auto* v = reinterpret_cast<const clap_event_param_value*> (h);
            static_cast<OutEvents*> (o->ctx)->values.push_back ({ v->param_id, v->value });
        }
        return true;
    } };
};

// A note or a parameter at a frame of the whole render (a note's id -1: none, as Melodee sends).
struct At
{
    uint64_t frame;
    enum { on, off, param, midi, choke } kind;
    int key = 60;
    double value = 1.0;
    std::string name = {};
    uint8_t m[3] {};
    int32_t id = -1;
};

struct Instance
{
    const clap_plugin* plugin = nullptr;
    const clap_plugin_params* params = nullptr;
    const clap_plugin_state* state = nullptr;
    const clap_plugin_preset_load* presets = nullptr;
    std::map<std::string, clap_id> ids;
    double rate = 48000.0;
    clap_process_status last = CLAP_PROCESS_CONTINUE;

    explicit Instance (double sampleRate = 48000.0) : rate (sampleRate)
    {
        const auto* d = factory->get_plugin_descriptor (factory, 0);
        plugin = factory->create_plugin (factory, &host, d->id);
        CHECK (plugin != nullptr && plugin->init (plugin));
        params = static_cast<const clap_plugin_params*> (plugin->get_extension (plugin, CLAP_EXT_PARAMS));
        state = static_cast<const clap_plugin_state*> (plugin->get_extension (plugin, CLAP_EXT_STATE));
        presets = static_cast<const clap_plugin_preset_load*> (plugin->get_extension (plugin, CLAP_EXT_PRESET_LOAD));
        for (uint32_t i = 0; params && i < params->count (plugin); ++i)
        {
            clap_param_info info {};
            params->get_info (plugin, i, &info);
            ids[info.name] = info.id;
        }
        CHECK (plugin->activate (plugin, rate, 1, 4096));
        CHECK (plugin->start_processing (plugin));
    }
    ~Instance()
    {
        plugin->stop_processing (plugin);
        plugin->deactivate (plugin);
        plugin->destroy (plugin);
    }
    clap_id id (const std::string& name)
    {
        const auto it = ids.find (name);
        if (it == ids.end())
        {
            std::printf ("FAIL no parameter named \"%s\"\n", name.c_str());
            ++failures;
            return CLAP_INVALID_ID;
        }
        return it->second;
    }
    double value (const std::string& name)
    {
        double v = 0.0;
        params->get_value (plugin, id (name), &v);
        return v;
    }
    std::string text (const std::string& name, double v)
    {
        char t[64] = {};
        CHECK (params->value_to_text (plugin, id (name), v, t, sizeof (t)));
        return t;
    }
    std::string text (const std::string& name) { return text (name, value (name)); }
    bool loadPreset (const char* key) { return presets != nullptr && presets->from_location (plugin, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr, key); }

    // Renders `seconds` in blocks of `block` with the timed events, into `left` and `right`.
    void render (double seconds, const std::vector<At>& events, std::vector<float>& left, std::vector<float>& right, uint32_t block = 128,
                 OutEvents* out = nullptr)
    {
        const uint64_t frames = (uint64_t) (seconds * rate);
        left.assign (frames, 0.0f);
        right.assign (frames, 0.0f);
        OutEvents ignored;
        for (uint64_t at = 0; at < frames; at += block)
        {
            const uint32_t n = (uint32_t) std::min<uint64_t> (block, frames - at);
            Events e;
            for (const auto& a : events)
            {
                if (a.frame < at || a.frame >= at + n) continue;
                const uint32_t t = (uint32_t) (a.frame - at);
                if (a.kind == At::on) e.note (t, CLAP_EVENT_NOTE_ON, (int16_t) a.key, a.value, a.id);
                else if (a.kind == At::off) e.note (t, CLAP_EVENT_NOTE_OFF, (int16_t) a.key, 0.0, a.id);
                else if (a.kind == At::choke) e.note (t, CLAP_EVENT_NOTE_CHOKE, (int16_t) a.key, 0.0, a.id);
                else if (a.kind == At::param) e.param (t, id (a.name), a.value);
                else e.midi (t, a.m[0], a.m[1], a.m[2]);
            }
            e.sort();
            float* outs[2] = { left.data() + at, right.data() + at };
            clap_audio_buffer buffer {};
            buffer.data32 = outs;
            buffer.channel_count = 2;
            clap_process p {};
            p.steady_time = (int64_t) at;
            p.frames_count = n;
            p.audio_outputs = &buffer;
            p.audio_outputs_count = 1;
            p.in_events = &e.in;
            p.out_events = out ? &out->out : &ignored.out;
            last = plugin->process (plugin, &p);
            CHECK (last != CLAP_PROCESS_ERROR);
        }
    }
    // Settings before anything plays: one block of their events.
    void setup (const std::vector<std::pair<std::string, double>>& values, OutEvents* out = nullptr)
    {
        std::vector<At> list;
        for (const auto& [name, v] : values) list.push_back ({ 0, At::param, 0, v, name });
        std::vector<float> l, r;
        render (32.0 / rate, list, l, r, 32, out);
    }
};

double peak (const std::vector<float>& x, size_t from = 0, size_t to = SIZE_MAX)
{
    double p = 0.0;
    for (size_t i = from; i < std::min (to, x.size()); ++i) p = std::max (p, (double) std::fabs (x[i]));
    return p;
}
double rms (const std::vector<float>& x, size_t from = 0, size_t to = SIZE_MAX)
{
    double s = 0.0;
    size_t n = 0;
    for (size_t i = from; i < std::min (to, x.size()); ++i) s += (double) x[i] * x[i], ++n;
    return n ? std::sqrt (s / (double) n) : 0.0;
}
bool finite (const std::vector<float>& x)
{
    for (float v : x)
        if (! std::isfinite (v)) return false;
    return true;
}
// Frequency by counting upward zero crossings between `from` and `to` (a clean tone's).
double frequency (const std::vector<float>& x, size_t from, size_t to, double rate)
{
    int crossings = 0;
    size_t first = 0, last = 0;
    for (size_t i = from + 1; i < std::min (to, x.size()); ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
        {
            if (crossings == 0) first = i;
            last = i;
            ++crossings;
        }
    return crossings > 1 ? (double) (crossings - 1) * rate / (double) (last - first) : 0.0;
}
// Upward crossings a second: brightness for a noisy or rich sound.
double crossingRate (const std::vector<float>& x, size_t from, size_t to, double rate)
{
    int crossings = 0;
    for (size_t i = from + 1; i < std::min (to, x.size()); ++i) crossings += x[i - 1] < 0.0f && x[i] >= 0.0f;
    return (double) crossings * rate / (double) (std::min (to, x.size()) - from);
}
uint64_t hash (const std::vector<float>& l, const std::vector<float>& r)
{
    uint64_t h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < l.size(); ++i)
        for (float v : { l[i], r[i] })
        {
            uint32_t bits;
            std::memcpy (&bits, &v, 4);
            for (int b = 0; b < 4; ++b) h = (h ^ ((bits >> (8 * b)) & 0xff)) * 0x100000001b3ull;
        }
    return h;
}

const char* const machines[] = { "FM TONE", "BD", "SD", "CP", "HH", "CY/CB", "EFM DRUM", "GND", "KIT",
                                 "TERRAIN", "PHASE", "VOX", "MODAL", "PLUCK", "GENDY", "PULSAR", "ORGAN", "KICK" };
constexpr int machineCount = 18;
int dataOf (clap_id id)
{
    if (id >= 1 && id <= 8) return (int) id - 1;
    if (id == 33 || id == 34) return 8 + (int) id - 33;
    if (id >= 100 && id < 228)
    {
        const int f = (int) (id - 100) % 16;
        if (f >= 1 && f <= 8) return f - 1;
        if (f == 11 || f == 12) return 8 + f - 11;
    }
    return -1;
}
const char* const dataNames[] = { "Data A", "Data B", "Data C", "Data D", "Data E", "Data F", "Data G", "Data H" };

// A pure sine from FM TONE: no modulation, the carrier alone.
const std::vector<std::pair<std::string, double>> pureSine { { "Data A", 0.0 }, { "FM A Level", 0.0 }, { "FM B Level", 0.0 },
                                                             { "Data E", 0.5 }, { "Data F", 0.0 }, { "Data G", 0.0 }, { "Data H", 0.0 } };

void description()
{
    const auto* d = factory->get_plugin_descriptor (factory, 0);
    CHECK (factory->get_plugin_count (factory) == 1);
    CHECK (std::string (d->id) == "com.hdslave.machine" && std::string (d->name) == "hdslave Machine");
    Instance m;
    const auto* audio = static_cast<const clap_plugin_audio_ports*> (m.plugin->get_extension (m.plugin, CLAP_EXT_AUDIO_PORTS));
    const auto* notes = static_cast<const clap_plugin_note_ports*> (m.plugin->get_extension (m.plugin, CLAP_EXT_NOTE_PORTS));
    CHECK (audio && audio->count (m.plugin, true) == 0 && audio->count (m.plugin, false) == 1);
    clap_audio_port_info port {};
    CHECK (audio->get (m.plugin, 0, false, &port) && port.channel_count == 2);
    CHECK (notes && notes->count (m.plugin, true) == 1 && notes->count (m.plugin, false) == 0);
}

void parameters()
{
    Instance m;
    const uint32_t count = m.params->count (m.plugin);
    CHECK (count == 139);
    std::set<clap_id> seen;
    for (uint32_t i = 0; i < count; ++i)
    {
        clap_param_info info {};
        CHECK (m.params->get_info (m.plugin, i, &info));
        CHECK (seen.insert (info.id).second);
        CHECK (info.default_value >= info.min_value && info.default_value <= info.max_value);
        CHECK ((info.flags & CLAP_PARAM_IS_AUTOMATABLE) != 0);
        double v = 0.0;
        CHECK (m.params->get_value (m.plugin, info.id, &v) && v == info.default_value);
    }
    // Fixed ids: a project keeps its values by them.
    CHECK (m.id ("Machine") == 0 && m.id ("Data A") == 1 && m.id ("Data H") == 8 && m.id ("FM A Attack") == 9 && m.id ("Filter Freq") == 17);
    CHECK (m.id ("Amp Attack") == 23 && m.id ("LFO Speed") == 29 && m.id ("LFO Dest") == 32);
    CHECK (m.id ("Kit 1 Machine") == 100 && m.id ("Kit 1 Data A") == 101 && m.id ("Kit 1 Level") == 109 && m.id ("Kit 1 Pan") == 110);
    CHECK (m.id ("Kit 8 Pan") == 100 + 7 * 16 + 10);
    CHECK (m.id ("Data I") == 33 && m.id ("Data J") == 34 && m.id ("Kit 1 Data I") == 111 && m.id ("Kit 8 Data J") == 100 + 7 * 16 + 12);
    CHECK (m.text ("Machine", 9) == "TERRAIN" && m.text ("Machine", 16) == "ORGAN" && m.text ("Machine", 17) == "KICK");
    CHECK (m.text ("Kit 1 Machine", 11) == "KICK" && m.text ("Kit 1 Machine", 7) == "MODAL");
    // Text: the machine's names, Data under the machine in use, a choice read back by its name.
    CHECK (m.text ("Machine", 0) == "FM TONE" && m.text ("Machine", 6) == "EFM DRUM" && m.text ("Machine", 8) == "KIT");
    CHECK (m.text ("Data A") == "3"); // FM TONE's ALGO 3
    CHECK (m.text ("Data B") == "1.00" && m.text ("Data C") == "2.00");
    CHECK (m.text ("Amp Decay") == "INF" && m.text ("Volume") == "+0.0dB" && m.text ("Filter Type", 2) == "BP");
    CHECK (m.text ("Kit 4 Data A") == "CLOSED" && m.text ("Kit 5 Data A") == "OPEN" && m.text ("Kit 1 Machine") == "BD");
    double v = 0.0;
    CHECK (m.params->text_to_value (m.plugin, m.id ("LFO Wave"), "S+H", &v) && v == 3.0);
    CHECK (m.params->text_to_value (m.plugin, m.id ("Machine"), "kit", &v) && v == 8.0);
    CHECK (m.params->text_to_value (m.plugin, m.id ("Amp Release"), "250ms", &v) && std::fabs (v - m.value ("Amp Release")) < 1e-3);
    // Not a number, not a knob: the Data's, KIT's and the shared section's texts refuse them.
    for (const char* name : { "Data A", "Data E", "Kit 1 Data A", "Kit 3 Level", "Filter Freq", "Amp Decay", "Volume" })
        for (const char* t : { "nan", "NaN", "inf", "-inf" })
        {
            const bool word = (std::string (name) == "Amp Decay" && std::string (t) == "inf") || (std::string (name) == "Volume" && std::string (t) == "-inf");
            if (! word) CHECK (! m.params->text_to_value (m.plugin, m.id (name), t, &v)); // (INF and -INF are those two's own words)
        }
    // VOLUME's quietest steps still fit seven characters.
    CHECK (m.text ("Volume", 0.003) == "-96dB" || m.text ("Volume", 0.003).size() <= 7);
    CHECK (m.text ("Volume", 0.0001).size() <= 7 && m.text ("Volume", 0.0001) != "-INF");
    // Every other parameter's text read back gives the same text (Volume's dB, the LFO's Hz, ENV's octaves...).
    m.setup ({ { "Machine", 0 } });
    for (uint32_t i = 0; i < count; ++i)
    {
        clap_param_info info {};
        m.params->get_info (m.plugin, i, &info);
        if (dataOf (info.id) >= 0) continue;
        for (double x : { 0.0, 0.07, 0.25, 0.5, 0.75, 0.93, 1.0 })
        {
            const double value = info.min_value + x * (info.max_value - info.min_value);
            const std::string first = m.text (info.name, value);
            double back = -1.0;
            const bool read = m.params->text_to_value (m.plugin, info.id, first.c_str(), &back);
            const std::string again = read ? m.text (info.name, back) : "unread";
            if (again != first) std::printf ("FAIL %s: \"%s\" read back as \"%s\"\n", info.name, first.c_str(), again.c_str()), ++failures;
            ++checks;
        }
    }
    // Data text read back: under every machine, each Data knob's text gives a value whose text is the same.
    for (int machine = 0; machine < machineCount; ++machine)
    {
        if (machine == 8) continue; // KIT's own Data are plain numbers
        m.setup ({ { "Machine", (double) machine } });
        for (uint32_t k = 0; k < 10; ++k)
        {
            const std::string name = k < 8 ? dataNames[k] : (k == 8 ? "Data I" : "Data J");
            for (double x : { 0.0, 0.07, 0.25, 0.5, 0.73, 0.9, 1.0 })
            {
                const std::string first = m.text (name, x);
                if (first == "-") continue; // a Data the machine leaves unused
                double back = -1.0;
                const bool read = m.params->text_to_value (m.plugin, m.id (name), first.c_str(), &back);
                const std::string again = read ? m.text (name, back) : "";
                const bool near = first[0] == '~' && again == first.substr (1); // "~BAR" (near a bar) reads as the bar
                if (! read || (again != first && ! near))
                    std::printf ("FAIL %s %s: \"%s\" read back as %.4f (\"%s\")\n", machines[machine], name.c_str(), first.c_str(), back,
                                 read ? m.text (name, back).c_str() : "unread"), ++failures;
                ++checks;
            }
        }
    }
    // Every value text at most 7 characters (the machine's names 8), for the screen's rows.
    for (int machine = 0; machine < machineCount; ++machine)
    {
        m.setup ({ { "Machine", (double) machine } });
        for (uint32_t i = 0; i < count; ++i)
        {
            clap_param_info info {};
            m.params->get_info (m.plugin, i, &info);
            for (double x : { 0.0, 0.003, 0.1, 0.25, 0.5, 0.75, 0.9, 0.999, 1.0 })
            {
                const double value = info.min_value + x * (info.max_value - info.min_value);
                char t[64] = {};
                m.params->value_to_text (m.plugin, info.id, value, t, sizeof (t));
                const size_t most = std::string (info.name).find ("Machine") != std::string::npos ? 8 : 7;
                if (std::strlen (t) > most)
                {
                    ++failures;
                    std::printf ("FAIL text too long: %s = \"%s\" under %s\n", info.name, t, machines[machine]);
                }
            }
        }
    }
}

void fmTone (const char* out)
{
    Instance m;
    std::vector<float> l, r;
    // A pure sine at the key's pitch, RATIO C its multiple.
    m.setup (pureSine);
    m.render (1.0, { { 0, At::on, 69, 1.0 } }, l, r);
    CHECK (finite (l) && peak (l) > 0.3 && peak (l) < 0.5);
    CHECK (std::fabs (frequency (l, 2400, 48000, 48000) - 440.0) < 1.0);
    CHECK (l == r); // the main machine sits in the middle
    const auto letGo = [&] (int key) { m.render (1.0, { { 0, At::off, key } }, l, r); CHECK (peak (l, 24000) == 0.0); };
    letGo (69);
    m.setup ({ { "Data B", 0.0 } }); // RATIO C 0.25: two octaves down
    m.render (1.0, { { 0, At::on, 69, 1.0 } }, l, r);
    CHECK (std::fabs (frequency (l, 2400, 48000, 48000) - 110.0) < 0.5);
    letGo (69);
    m.setup ({ { "Data B", 6.0 / 22.0 } }); // 2.00
    m.render (1.0, { { 0, At::on, 69, 1.0 } }, l, r);
    CHECK (std::fabs (frequency (l, 2400, 48000, 48000) - 880.0) < 2.0);
    letGo (69);
    m.setup ({ { "Data B", 3.0 / 22.0 } });
    // Bend: +2 semitones at the top of the wheel.
    m.render (1.0, { { 0, At::on, 69, 1.0 }, { 0, At::midi, 0, 0, "", { 0xe0, 0x7f, 0x7f } } }, l, r);
    CHECK (std::fabs (frequency (l, 4800, 48000, 48000) - 440.0 * std::pow (2.0, 2.0 * 8191.0 / 8192.0 / 12.0)) < 1.5);
    m.render (0.1, { { 0, At::midi, 0, 0, "", { 0xe0, 0x00, 0x40 } } }, l, r); // centred again
    letGo (69);

    // Held, it sustains (AMP DECAY INF); let go, it fades over RELEASE and stops.
    m.render (2.0, { { 0, At::on, 60, 1.0 }, { 48000, At::off, 60 } }, l, r);
    CHECK (rms (l, 40000, 47000) > 0.2 && rms (l, 30000, 37000) > 0.2);
    CHECK (std::fabs (rms (l, 40000, 47000) / rms (l, 10000, 17000) - 1.0) < 0.02);
    CHECK (peak (l, 48000 + 48000 * 6 / 10) < 0.002); // 250 ms release: -60 dB by 250 ms, quiet well before 600
    CHECK (m.last == CLAP_PROCESS_CONTINUE_IF_NOT_QUIET);
    // Velocity scales it.
    m.render (0.5, { { 0, At::on, 60, 0.5 } }, l, r);
    const double half = rms (l, 4800, 20000);
    letGo (60);
    m.render (0.5, { { 0, At::on, 60, 1.0 } }, l, r);
    CHECK (std::fabs (rms (l, 4800, 20000) / half - 2.0) < 0.05);
    letGo (60);

    // FM ENV A: the modulator brightens the tone, then falls back to END.
    m.setup ({ { "FM A Level", 0.8 }, { "Data A", 0.0 }, { "FM A Decay", 0.45 }, { "FM A End", 0.0 } });
    m.render (2.0, { { 0, At::on, 57, 1.0 } }, l, r);
    const double early = crossingRate (l, 0, 960, 48000), late = crossingRate (l, 72000, 96000, 48000);
    CHECK (early > late * 1.5);
    CHECK (std::fabs (late - 220.0) < 3.0); // A's level down to END 0: a plain sine again
    letGo (57);

    // Every algorithm sounds, stays bounded, and differs from the others.
    std::set<uint64_t> sounds;
    m.setup ({ { "FM A Level", 0.6 }, { "FM B Level", 0.6 }, { "FM A End", 0.5 }, { "FM B End", 0.5 }, { "Data H", 0.5 }, { "Data G", 0.4 } });
    for (int a = 0; a < 8; ++a)
    {
        m.setup ({ { "Data A", a / 7.0 } });
        m.render (0.5, { { 0, At::on, 48, 1.0 }, { 0, At::on, 55, 0.8 } }, l, r);
        CHECK (finite (l) && peak (l) > 0.1 && peak (l) < 2.5);
        sounds.insert (hash (l, r));
        m.render (1.0, { { 0, At::off, 48 }, { 0, At::off, 55 } }, l, r);
    }
    CHECK (sounds.size() == 8);

    // Polyphony: ten keys at once sound as eight; nothing breaks.
    std::vector<At> chord;
    for (int k = 0; k < 10; ++k) chord.push_back ({ (uint64_t) k * 100, At::on, 48 + k * 3, 0.7 });
    m.render (1.0, chord, l, r);
    CHECK (finite (l) && peak (l) < 4.0);
    m.render (1.0, { { 0, At::off, 48 } }, l, r);
    std::vector<At> offs;
    for (int k = 0; k < 10; ++k) offs.push_back ({ 0, At::off, 48 + k * 3 });
    m.render (1.0, offs, l, r);
    CHECK (peak (l, 30000) < 0.001);

    // Demo: a chord progression on FM Keys.
    Instance demo;
    CHECK (demo.loadPreset ("fm-keys"));
    std::vector<At> song;
    const int chords[4][3] = { { 57, 60, 64 }, { 53, 57, 60 }, { 55, 59, 62 }, { 52, 55, 59 } };
    for (int c = 0; c < 4; ++c)
        for (int k = 0; k < 3; ++k)
        {
            song.push_back ({ (uint64_t) c * 48000, At::on, chords[c][k], 0.8 });
            song.push_back ({ (uint64_t) c * 48000 + 40000, At::off, chords[c][k] });
        }
    demo.render (5.0, song, l, r);
    CHECK (finite (l) && peak (l) > 0.1);
    writeWav ((std::string (out) + "/demo-fm-keys.wav").c_str(), l, r, 48000);
}

void drums (const char* out)
{
    for (int machine = 1; machine <= 7; ++machine)
    {
        Instance m;
        OutEvents events;
        std::vector<float> l, r;
        m.setup ({ { "Machine", (double) machine } }, &events);
        // A new machine: Data A-H take its defaults, and the host is told.
        int told = 0;
        for (const auto& [id, value] : events.values)
            if (id >= 1 && id <= 8)
            {
                ++told;
                CHECK (std::fabs (value - m.value (dataNames[id - 1])) < 1e-6);
            }
        CHECK (told == 8);
        // It sounds (a note-off at once changes nothing: drums ring out), ends, and goes quiet.
        m.render (6.0, { { 0, At::on, 60, 1.0 }, { 10, At::off, 60 } }, l, r);
        CHECK (finite (l) && peak (l) > 0.1 && peak (l) < 2.0);
        CHECK (rms (l, 0, 4800) > 0.01);
        CHECK (peak (l, 5 * 48000) < 0.001);
        CHECK (m.last == CLAP_PROCESS_CONTINUE_IF_NOT_QUIET);
        // A new hit chokes the last, without a click: the jump between samples stays small.
        m.render (1.0, { { 0, At::on, 60, 1.0 }, { 4800, At::on, 60, 0.3 } }, l, r);
        double jump = 0.0;
        for (size_t i = 4790; i < 4800 + 200; ++i) jump = std::max (jump, (double) std::fabs (l[i] - l[i - 1]));
        CHECK (finite (l));
        if (machine == 1) CHECK (jump < 0.3); // the kick: no step as the last one goes (its click is noise up to 0.1)
        // Each machine's own render, for the ears.
        std::vector<At> groove;
        for (int b = 0; b < 8; ++b) groove.push_back ({ (uint64_t) b * 12000, At::on, 60, b % 2 ? 0.6 : 1.0 });
        m.render (3.0, groove, l, r);
        std::string name = machines[machine];
        std::replace (name.begin(), name.end(), '/', '-');
        std::replace (name.begin(), name.end(), ' ', '-');
        writeWav ((std::string (out) + "/demo-" + std::to_string (machine) + "-" + name + ".wav").c_str(), l, r, 48000);
    }

    // BD lands on TUNE once its sweep is done.
    Instance bd;
    std::vector<float> l, r;
    bd.setup ({ { "Machine", 1 } });
    bd.setup ({ { "Data A", 0.5 }, { "Data B", 0.85 }, { "Data E", 0.0 }, { "Data F", 0.0 } }); // TUNE 0.5: sqrt (28 * 140) Hz, long, no click
    bd.render (1.0, { { 0, At::on, 60, 1.0 } }, l, r);
    CHECK (std::fabs (frequency (l, 9600, 24000, 48000) - std::sqrt (28.0 * 140.0)) < 1.5);
    CHECK (crossingRate (l, 0, 480, 48000) > crossingRate (l, 9600, 24000, 48000) * 2.0); // the sweep starts high

    // HH: open rings longer than closed.
    Instance hh;
    hh.setup ({ { "Machine", 4 } });
    hh.render (1.0, { { 0, At::on, 60, 1.0 } }, l, r);
    const double closed = rms (l, 9600, 14400);
    hh.setup ({ { "Data A", 1.0 } });
    hh.render (1.0, { { 0, At::on, 60, 1.0 } }, l, r);
    CHECK (rms (l, 9600, 14400) > closed * 4.0);
}

void kit (const char* out)
{
    Instance m;
    OutEvents events;
    std::vector<float> l, r;
    m.setup ({ { "Machine", 8 } }, &events);
    CHECK (events.values.empty()); // KIT keeps Data A-H as they were (its sounds are the slots')
    // Each of its keys plays its slot; other keys nothing.
    for (int k = 36; k < 44; ++k)
    {
        m.render (0.4, { { 0, At::on, k, 1.0 } }, l, r);
        CHECK (peak (l) > 0.02 && finite (l));
        m.render (2.0, {}, l, r);
    }
    m.render (0.4, { { 0, At::on, 44, 1.0 }, { 0, At::on, 60, 1.0 }, { 0, At::on, 35, 1.0 } }, l, r);
    CHECK (peak (l) == 0.0);
    // Pan: hard left leaves the right side silent.
    m.setup ({ { "Kit 1 Pan", -1.0 } });
    m.render (0.4, { { 0, At::on, 36, 1.0 } }, l, r);
    CHECK (peak (l) > 0.1 && peak (r) == 0.0);
    m.setup ({ { "Kit 1 Pan", 0.0 } });
    m.render (2.0, {}, l, r);
    // A slot's new machine: its Data reset, the host told.
    OutEvents slotEvents;
    m.setup ({ { "Kit 3 Machine", 5 } }, &slotEvents); // EFM DRUM (BD 0 .. GND 6)
    int told = 0;
    for (const auto& [id, value] : slotEvents.values)
        if (id >= 100 + 2 * 16 + 1 && id <= 100 + 2 * 16 + 8) ++told;
    CHECK (told == 8 && m.text ("Kit 3 Data A").find ("Hz") != std::string::npos); // PITCH, EFM DRUM's
    m.setup ({ { "Kit 3 Machine", 2 } });
    // The closed hat (slot 4) chokes the open one (slot 5).
    m.render (1.0, { { 0, At::on, 40, 1.0 } }, l, r);
    const double ringing = rms (l, 9600, 14400);
    m.render (2.0, {}, l, r);
    m.render (1.0, { { 0, At::on, 40, 1.0 }, { 2400, At::on, 39, 0.01 } }, l, r);
    CHECK (rms (l, 9600, 14400) < ringing * 0.1);
    m.render (2.0, {}, l, r);
    // A groove for the ears.
    std::vector<At> groove;
    const int pattern[16] = { 36, 39, 40, 39, 37, 39, 40, 39, 36, 36, 41, 39, 37, 42, 43, 38 };
    for (int b = 0; b < 32; ++b) groove.push_back ({ (uint64_t) b * 6000, At::on, pattern[b % 16], b % 4 == 0 ? 1.0 : 0.7 });
    m.render (4.5, groove, l, r);
    CHECK (finite (l) && peak (l) > 0.2 && peak (l) < 3.0);
    writeWav ((std::string (out) + "/demo-kit.wav").c_str(), l, r, 48000);
}

// The repeating period by autocorrelation: the lag (in samples) between `lo` and `hi` that best matches the window.
double period (const std::vector<float>& x, size_t from, size_t len, int lo, int hi)
{
    // Normalized correlation at each lag; the shortest lag within 2 % of the best (a period, not two of them).
    std::vector<double> score ((size_t) (hi + 1), -1.0);
    double best = -1.0;
    double e0 = 0.0;
    for (size_t i = from; i < from + len; ++i) e0 += (double) x[i] * x[i];
    for (int lag = lo; lag <= hi; ++lag)
    {
        double c = 0.0, e = 0.0;
        for (size_t i = from; i < from + len; ++i) c += (double) x[i] * x[i + (size_t) lag], e += (double) x[i + (size_t) lag] * x[i + (size_t) lag];
        score[(size_t) lag] = e > 0.0 && e0 > 0.0 ? c / std::sqrt (e * e0) : -1.0;
        best = std::max (best, score[(size_t) lag]);
    }
    for (int lag = lo; lag <= hi; ++lag)
        if (score[(size_t) lag] >= best - 0.02 && (lag == hi || score[(size_t) lag] >= score[(size_t) lag + 1]))
            return (double) lag;
    return (double) lo;
}

// The second barrage: TERRAIN, PHASE, VOX, MODAL, PLUCK, GENDY, PULSAR, ORGAN, and KICK.
void barrage (const char* out)
{
    std::vector<float> l, r;
    for (int machine = 9; machine < machineCount; ++machine)
    {
        Instance m;
        OutEvents events;
        m.setup ({ { "Machine", (double) machine } }, &events);
        int told = 0;
        for (const auto& [id, value] : events.values) told += (id >= 1 && id <= 8) || id == 33 || id == 34;
        CHECK (told == 10); // a new machine: Data A-J take its defaults, the host told
        const int key = machine == 17 ? 36 : 57;
        m.render (1.5, { { 0, At::on, key, 1.0 }, { 36000, At::off, key } }, l, r);
        if (! finite (l) || peak (l) < 0.05 || peak (l) > 2.0)
            std::printf ("FAIL %s: peak %.3f\n", machines[machine], peak (l)), ++failures;
        m.render (4.0, {}, l, r); // let go: released, damped or decayed, it ends
        if (peak (l, 3 * 48000) > 0.001 || m.last != CLAP_PROCESS_CONTINUE_IF_NOT_QUIET)
            std::printf ("FAIL %s does not end: peak %.5f\n", machines[machine], peak (l, 3 * 48000)), ++failures;
        std::vector<At> phrase;
        const int notes[8] = { 45, 52, 57, 60, 64, 57, 52, 48 };
        for (int b = 0; b < 8; ++b)
        {
            const int k = machine == 17 ? 36 + (b % 4) * 2 : notes[b];
            phrase.push_back ({ (uint64_t) b * 18000, At::on, k, b % 2 ? 0.7 : 1.0 });
            phrase.push_back ({ (uint64_t) b * 18000 + 15000, At::off, k });
        }
        m.render (5.0, phrase, l, r);
        CHECK (finite (l) && peak (l) < 2.5);
        writeWav ((std::string (out) + "/demo-" + std::to_string (machine) + "-" + machines[machine] + ".wav").c_str(), l, r, 48000);
    }

    // On the note: ORGAN's 8' alone, PHASE bent nowhere (a cosine), GENDY's points still (a sine of sixteen), MODAL's
    // string, PLUCK's string, and by their periods TERRAIN, VOX and PULSAR.
    auto pitched = [&] (int machine, const std::vector<std::pair<std::string, double>>& values, int key, double want, double tolerance,
                        bool byPeriod, double from = 0.3) {
        Instance m;
        m.setup ({ { "Machine", (double) machine } });
        m.setup (values);
        m.render (1.2, { { 0, At::on, key, 1.0 } }, l, r);
        const size_t at = (size_t) (from * 48000);
        const double hz = byPeriod ? 48000.0 / period (l, at, 4096, 40, 960) : frequency (l, at, at + 24000, 48000);
        if (std::fabs (hz - want) > want * tolerance) std::printf ("FAIL %s pitch %.2f Hz, wanted %.2f\n", machines[machine], hz, want), ++failures;
        ++checks;
    };
    pitched (16, { { "Data A", 0 }, { "Data B", 0 }, { "Data C", 1 } }, 57, 220.0, 0.005, false);
    pitched (10, { { "Data A", 0.5 }, { "Data B", 0.5 / 6.0 }, { "Data C", 0.5 }, { "Data E", 0 }, { "Data F", 0 } }, 57, 220.0, 0.005, false);
    pitched (14, { { "Data A", 1.0 }, { "Data B", 0 }, { "Data C", 0 }, { "Data E", 1 } }, 57, 220.0, 0.01, false);
    pitched (12, { { "Data B", 0 }, { "Data C", 0 }, { "Data D", 0.2 }, { "Data E", 0.5 }, { "Data F", 0.3 }, { "Data G", 0 } }, 57, 220.0, 0.02, false);
    pitched (13, { { "Data C", 0.3 } }, 45, 110.0, 0.01, true);
    pitched (9, {}, 57, 220.0, 0.01, true);
    pitched (11, {}, 45, 110.0, 0.01, true);
    pitched (15, {}, 57, 220.0, 0.01, true);
    pitched (11, { { "Data A", 36.0 / 48.0 } }, 45, 220.0, 0.01, true); // VOX's TUNE: +12 semitones
    // KICK follows the note from C2 an octave at a time once its bend is done.
    {
        Instance m;
        m.setup ({ { "Machine", 17 } });
        m.setup ({ { "Data D", 0.8 }, { "Data H", 0 } }); // long, clean
        m.render (1.0, { { 0, At::on, 36, 1.0 } }, l, r);
        const double low = frequency (l, 14400, 30000, 48000);
        m.render (1.0, { { 0, At::on, 48, 1.0 } }, l, r);
        const double high = frequency (l, 14400, 30000, 48000);
        CHECK (std::fabs (high / low - 2.0) < 0.06);
        CHECK (std::fabs (low - 30.0 * std::pow (250.0 / 30.0, 0.35)) < 2.0); // PITCH's default at C2
        // ACCENT: at 0 every hit the same, at full the velocity reaches all the way.
        m.setup ({ { "Data J", 0.0 } });
        m.render (0.5, { { 0, At::on, 36, 0.3 } }, l, r);
        const double soft = rms (l, 0, 9600);
        m.render (0.5, { { 0, At::on, 36, 1.0 } }, l, r);
        CHECK (std::fabs (rms (l, 0, 9600) / soft - 1.0) < 0.01);
        m.setup ({ { "Data J", 1.0 } });
        m.render (0.5, { { 0, At::on, 36, 0.3 } }, l, r);
        const double accented = rms (l, 0, 9600);
        m.render (0.5, { { 0, At::on, 36, 1.0 } }, l, r);
        CHECK (accented / rms (l, 0, 9600) < 0.5);
        CHECK (m.text ("Data I").back() == 'k' && m.text ("Data J") == "100" && m.text ("Data A").find ("Hz") != std::string::npos);
    }
    // PULSAR at full DENSITY: pulses eight octaves below the note, a rhythm with silence between.
    {
        Instance m;
        m.setup ({ { "Machine", 15 } });
        m.setup ({ { "Data C", 1.0 } });
        m.render (3.0, { { 0, At::on, 69, 1.0 } }, l, r);
        int loud = 0, windows = 0;
        for (size_t at = 0; at + 480 <= l.size(); at += 480, ++windows) loud += rms (l, at, at + 480) > 0.01;
        CHECK (loud > 0 && loud < windows / 5);
    }
    // KIT's new slot machines: KICK, MODAL, PLUCK, VOX and PULSAR, each sounding; VOX and PULSAR stopped by note-off,
    // MODAL ringing on (a slot never damps).
    {
        Instance m;
        OutEvents told;
        m.setup ({ { "Machine", 8 } });
        m.setup ({ { "Kit 1 Machine", 11 }, { "Kit 5 Machine", 7 }, { "Kit 6 Machine", 8 }, { "Kit 7 Machine", 9 }, { "Kit 8 Machine", 10 } }, &told);
        int data = 0;
        for (const auto& [id, value] : told.values) data += dataOf (id) >= 0;
        CHECK (data == 50);
        CHECK (m.text ("Kit 1 Machine") == "KICK" && m.text ("Kit 1 Data I").back() == 'k');
        for (int k : { 36, 40, 41, 42, 43 })
        {
            m.render (0.6, { { 0, At::on, k, 1.0 }, { 2400, At::off, k } }, l, r);
            if (peak (l) < 0.02 || ! finite (l)) std::printf ("FAIL KIT slot key %d: peak %.3f\n", k, peak (l)), ++failures;
            if (k == 40) CHECK (rms (l, 14400, 19200) > 0.001); // MODAL rings on
            if (k >= 42) CHECK (peak (l, 24000) < 0.001);       // VOX and PULSAR released
            m.render (3.0, {}, l, r);
        }
    }
}

// Compare the same strike through each amp stage. A fresh instance repeats the mallet or pick exactly.
void struckAmpEnvelopes()
{
    constexpr uint64_t lift = 2407; // a pad lifted about 50 ms in, between control updates
    for (int machine : { 12, 13 }) // MODAL, PLUCK
    {
        auto render = [=] (std::vector<std::pair<std::string, const char*>> settings, bool liftPad, bool kitSlot = false) {
            Instance m;
            if (kitSlot)
                m.setup ({ { "Machine", 8 }, { "Kit 1 Machine", machine == 12 ? 7.0 : 8.0 } });
            else
                m.setup ({ { "Machine", machine } });
            std::vector<std::pair<std::string, double>> values;
            for (const auto& [name, text] : settings)
            {
                double v = 0.0;
                CHECK (m.params->text_to_value (m.plugin, m.id (name), text, &v));
                values.push_back ({ name, v });
            }
            m.setup (values);
            const int key = kitSlot ? 36 : 48;
            std::vector<At> notes { { 0, At::on, key } };
            if (liftPad) notes.push_back ({ lift, At::off, key });
            std::vector<float> l, r;
            m.render (0.4, notes, l, r);
            return l;
        };
        const auto held = render ({}, false);
        const auto attack = render ({ { "Amp Attack", "100ms" } }, false);
        const auto decay = render ({ { "Amp Decay", "40ms" } }, false);
        const auto hold = render ({ { "Amp Hold", "120ms" }, { "Amp Decay", "40ms" } }, false);
        CHECK (rms (attack, 240, 480) < 0.11 * rms (held, 240, 480));
        CHECK (rms (decay, 3840, 4800) == 0.0);
        CHECK (std::equal (hold.begin(), hold.begin() + 4800, held.begin()));
        CHECK (rms (hold, 3840, 4800) > 0.001 && peak (hold, 9600) == 0.0);

        const auto fast = render ({ { "Amp Release", "20ms" } }, true);
        const auto slow = render ({ { "Amp Release", "2s" } }, true);
        CHECK (std::equal (fast.begin(), fast.begin() + lift, held.begin()));
        CHECK (std::equal (slow.begin(), slow.begin() + lift, held.begin()));
        CHECK (peak (fast, lift + 1920) == 0.0);
        // The 2 s release falls by about 6 dB in 200 ms, on top of the string's own decay.
        const double natural = rms (held, lift + 9600, lift + 10080);
        const double tail = rms (slow, lift + 9600, lift + 10080);
        CHECK (natural > 0.001 && tail > 0.47 * natural && tail < 0.52 * natural);

        const auto hit = render ({ { "Amp Release", "20ms" } }, false, true);
        CHECK (render ({ { "Amp Release", "20ms" } }, true, true) == hit);
    }
}

// Melodee's screen (when built with ui/): it paints, K1-K3 follow the cursor, an edit changes a parameter and tells
// the host, the machine changes from its own row, and KIT's SLOT row turns no knob.
void screen()
{
    Instance m;
    const auto* ext = static_cast<const melodee_screen_t*> (m.plugin->get_extension (m.plugin, MELODEE_SCREEN_CLAP));
#if MACHINE_SCREEN
    CHECK (ext != nullptr);
#endif
    if (ext == nullptr)
    {
        std::printf ("  (built without the screen: its checks skipped)\n");
        return;
    }
    std::vector<uint32_t> pixels ((size_t) MELODEE_SCREEN_WIDTH * MELODEE_SCREEN_PAGE_HEIGHT, 0);
    melodee_screen_frame_t canvas {};
    canvas.pixels = pixels.data();
    canvas.width = canvas.stride = MELODEE_SCREEN_WIDTH;
    canvas.height = MELODEE_SCREEN_PAGE_HEIGHT;
    canvas.area = MELODEE_SCREEN_PAGE;
    canvas.colour = 0x46d9b0u;
    canvas.redraw_all = true;
    for (auto& pot : canvas.pots) pot = -1;
    canvas.area = MELODEE_SCREEN_STRIP;
    CHECK (!ext->draw (m.plugin, &canvas));
    canvas.area = MELODEE_SCREEN_PAGE;
    --canvas.stride;
    CHECK (!ext->draw (m.plugin, &canvas));
    ++canvas.stride;
    CHECK (ext->draw (m.plugin, &canvas));
    std::set<uint32_t> colours (pixels.begin(), pixels.end());
    CHECK (colours.size() > 8);
    canvas.redraw_all = false;
    clap_id ids[3] {};
    ext->controls (m.plugin, ids);
    CHECK (ids[0] == 0 && ids[1] == 1 && ids[2] == 2); // MACHINE, ALGO, RATIO C
    auto in = [&] (uint32_t kind, int value) {
        const melodee_screen_input_t input { kind, value, false };
        return ext->input (m.plugin, &input);
    };
    const unsigned beforeRun = screenRuns;
    CHECK (in (MELODEE_SCREEN_WHEEL, 1) && screenRuns == beforeRun);
    ext->controls (m.plugin, ids);
    CHECK (ids[0] == 1 && ids[1] == 2 && ids[2] == 3);
    const int before = rescans, wakes = processRequests, flushes = flushRequests;
    CHECK (in (MELODEE_SCREEN_PUSH, 1) && in (MELODEE_SCREEN_WHEEL, 2)); // ALGO 3 to 5
    CHECK (std::fabs (m.value ("Data A") - 4.0 / 7.0) < 1e-6 && m.text ("Data A") == "5" && rescans > before);
    // An edit also wakes a sleeping plug-in (Melodee does not call process while it sleeps) and asks for a flush.
    CHECK (processRequests > wakes && flushRequests > flushes && screenRuns > beforeRun);
    CHECK (in (MELODEE_SCREEN_PUSH, 1) && in (MELODEE_SCREEN_WHEEL, -1) && in (MELODEE_SCREEN_PUSH, 1) && in (MELODEE_SCREEN_WHEEL, 17));
    CHECK (m.value ("Machine") == 17 && m.text ("Data A").find ("Hz") != std::string::npos); // KICK, its Data reset
    std::vector<float> l, r;
    m.render (0.5, { { 0, At::on, 36, 1.0 } }, l, r);
    CHECK (peak (l) > 0.1);
    CHECK (ext->draw (m.plugin, &canvas));
    CHECK (in (MELODEE_SCREEN_WHEEL, -9)); // still editing MACHINE (its own edit keeps it): KICK to KIT
    CHECK (m.value ("Machine") == 8);
    CHECK (in (MELODEE_SCREEN_PUSH, 1));   // done
    ext->controls (m.plugin, ids);
    CHECK (ids[0] == 0 && ids[1] == CLAP_INVALID_ID && ids[2] == 100); // MACHINE, SLOT (no knob), slot 1's MACH
    canvas.redraw_all = true;
    CHECK (ext->draw (m.plugin, &canvas));
}

// The host's copy of a value: a machine and a Data set in the same block leave the host the Data's value, not the
// reset's default; a machine's Data survive a visit to KIT; a choke takes its own key alone.
void hostCopies()
{
    Instance m;
    OutEvents out;
    std::vector<float> l, r;
    m.render (0.01, { { 0, At::param, 0, 1, "Machine" }, { 0, At::param, 0, 0.9, "Data B" } }, l, r, 128, &out);
    double lastB = -1.0;
    for (const auto& [id, value] : out.values)
        if (id == 2) lastB = value;
    CHECK (std::fabs (lastB - 0.9) < 1e-6 && std::fabs (m.value ("Data B") - 0.9) < 1e-6);
    m.setup ({ { "Data A", 0.9 } });          // BD tuned
    m.setup ({ { "Machine", 8 } });          // a look at KIT
    m.setup ({ { "Machine", 1 } });          // back: as it was
    CHECK (std::fabs (m.value ("Data A") - 0.9) < 1e-6);
    m.setup ({ { "Machine", 8 } });
    m.setup ({ { "Machine", 2 } });          // to another machine: its own defaults
    CHECK (std::fabs (m.value ("Data A") - 0.4) < 1e-6);
    Instance fm;
    fm.render (0.3, { { 0, At::on, 60, 1.0 }, { 0, At::on, 67, 1.0 } }, l, r);
    Events e;
    e.list.push_back ({});
    e.list.back().note = { { sizeof (clap_event_note), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_NOTE_CHOKE, 0 }, -1, 0, 0, 60, 0.0 };
    std::vector<float> l2 (4800, 0.0f), r2 (4800, 0.0f);
    float* outs[2] = { l2.data(), r2.data() };
    clap_audio_buffer buffer {};
    buffer.data32 = outs;
    buffer.channel_count = 2;
    clap_process p {};
    p.frames_count = 4800;
    p.audio_outputs = &buffer;
    p.audio_outputs_count = 1;
    p.in_events = &e.in;
    OutEvents ignored;
    p.out_events = &ignored.out;
    fm.plugin->process (fm.plugin, &p);
    CHECK (std::fabs (frequency (l2, 1200, 4800, 48000) - 392.0) < 4.0); // 60 choked, 67 (G4) sounding
}

// The engine review's findings, each held to: a machine changed while notes sound (every pair, and KIT's slots)
// stays finite and bounded; MODAL in tune in the bass at 48 and 96 kHz; the same brightness at 96 kHz as at 48; no
// click from the filter coming back from bypass, nor from a stolen voice; PLUCK's low notes at 96 kHz; all notes off
// releases, all sound off chokes.
double maxJump (const std::vector<float>& x, size_t from, size_t to)
{
    double jump = 0.0;
    for (size_t i = std::max<size_t> (from, 1); i < std::min (to, x.size()); ++i) jump = std::max (jump, (double) std::fabs (x[i] - x[i - 1]));
    return jump;
}
void engineFindings()
{
    std::vector<float> l, r;
    for (int from = 0; from < machineCount; ++from)
        for (int to = 0; to < machineCount; ++to)
        {
            if (from == to) continue;
            Instance m;
            m.setup ({ { "Machine", (double) from } });
            const int key = from == 8 ? 36 : 57;
            m.render (0.25, { { 0, At::on, key, 1.0 }, { 600, At::on, from == 8 ? 40 : 64, 1.0 }, { 4000, At::param, 0, (double) to, "Machine" },
                              { 4100, At::on, to == 8 ? 38 : 60, 1.0 } }, l, r);
            if (! finite (l) || peak (l) > 4.0)
                std::printf ("FAIL %s to %s while sounding: peak %.3g\n", machines[from], machines[to], peak (l)), ++failures;
            ++checks;
        }
    {
        Instance m;
        m.setup ({ { "Machine", 8 }, { "Kit 7 Machine", 9 }, { "Kit 4 Machine", 7 } }); // VOX and MODAL slots
        for (int slotMachine = 0; slotMachine < 12; ++slotMachine)
        {
            m.render (0.1, { { 0, At::on, 42, 1.0 }, { 0, At::on, 39, 1.0 }, { 1000, At::param, 0, (double) slotMachine, "Kit 7 Machine" },
                             { 1500, At::param, 0, (double) ((slotMachine + 5) % 12), "Kit 4 Machine" } }, l, r);
            if (! finite (l) || peak (l) > 4.0) std::printf ("FAIL KIT slot to %d while sounding: peak %.3g\n", slotMachine, peak (l)), ++failures;
            ++checks;
        }
    }
    // MODAL's string in tune at C1 (48 kHz) and C1 (96 kHz): within 10 cents.
    for (double rate : { 48000.0, 96000.0 })
    {
        Instance m (rate);
        m.setup ({ { "Machine", 12 } });
        m.setup ({ { "Data B", 0 }, { "Data C", 0 }, { "Data D", 0.1 }, { "Data E", 0.5 }, { "Data F", 0.3 }, { "Data G", 0 } });
        m.render (1.5, { { 0, At::on, 24, 1.0 } }, l, r);
        const double want = 440.0 * std::pow (2.0, (24 - 69) / 12.0);
        const int lag = (int) std::lround (rate / want);
        const double hz = rate / period (l, (size_t) (0.4 * rate), 8192, lag - 200, lag + 200);
        if (std::fabs (1200.0 * std::log2 (hz / want)) > 10.0) std::printf ("FAIL MODAL C1 at %.0f Hz: %.2f Hz, wanted %.2f\n", rate, hz, want), ++failures;
        ++checks;
    }
    // FM TONE's sine at 8 kHz as loud at 96 kHz as at 48 (the open filter out of the way at both).
    {
        double level[2];
        int at = 0;
        for (double rate : { 48000.0, 96000.0 })
        {
            Instance m (rate);
            m.setup (pureSine);
            m.render (0.5, { { 0, At::on, 120, 1.0 } }, l, r);
            level[at++] = rms (l, (size_t) (0.1 * rate), (size_t) (0.4 * rate));
        }
        CHECK (std::fabs (level[1] / level[0] - 1.0) < 0.01);
    }
    // The filter coming back from bypass: no jump beyond what the tone itself makes.
    {
        double jumps[2];
        for (int withLfo = 0; withLfo < 2; ++withLfo)
        {
            Instance m;
            m.setup ({ { "Machine", 16 } });
            m.setup ({ { "Data A", 0 }, { "Data B", 0 }, { "Data C", 1 }, { "LFO Dest", 1 }, { "LFO Wave", 2 }, { "LFO Speed", 0.66 },
                       { "LFO Depth", withLfo ? 0.05 : 0.0 } });
            m.render (1.0, { { 0, At::on, 48, 1.0 } }, l, r);
            jumps[withLfo] = maxJump (l, 2400, l.size());
        }
        CHECK (jumps[1] < jumps[0] * 1.5);
    }
    // Eight held notes, then eight new ones: the voices that make room fade, none cut.
    {
        double jumps[2];
        for (int held = 0; held < 2; ++held)
        {
            Instance m;
            m.setup (pureSine);
            std::vector<At> notes;
            if (held)
                for (int k = 0; k < 8; ++k) notes.push_back ({ 0, At::on, 40 + k * 2, 0.8 });
            for (int k = 0; k < 8; ++k) notes.push_back ({ 24000, At::on, 64 + k * 2, 0.8 });
            m.render (0.6, notes, l, r);
            jumps[held] = maxJump (l, 23990, 24400);
        }
        CHECK (jumps[1] < std::max (jumps[0] * 2.0, 0.05));
    }
    // PLUCK below 23 Hz at 96 kHz (a line longer than 48 kHz's): TUNE -24 from key 41 is 21.83 Hz.
    {
        Instance m (96000.0);
        m.setup ({ { "Machine", 13 } });
        m.setup ({ { "Data A", 0.0 }, { "Data B", 0.95 }, { "Data C", 0.3 } });
        m.render (1.2, { { 0, At::on, 41, 1.0 } }, l, r);
        const double want = 440.0 * std::pow (2.0, (41 - 24 - 69) / 12.0);
        const double hz = 96000.0 / period (l, 28800, 16384, 4000, 5000);
        if (std::fabs (hz - want) > want * 0.01) std::printf ("FAIL PLUCK at 96 kHz: %.2f Hz, wanted %.2f\n", hz, want), ++failures;
        ++checks;
    }
    // CC 123 releases (the note fades over RELEASE), CC 120 chokes (gone at once).
    {
        Instance a, b;
        a.setup (pureSine);
        b.setup (pureSine);
        a.render (0.2, { { 0, At::on, 60, 1.0 }, { 4800, At::midi, 0, 0, "", { 0xb0, 123, 0 } } }, l, r);
        CHECK (rms (l, 5000, 6000) > 0.05);
        b.render (0.2, { { 0, At::on, 60, 1.0 }, { 4800, At::midi, 0, 0, "", { 0xb0, 120, 0 } } }, l, r);
        CHECK (peak (l, 5200, 9600) < 1e-4);
    }
}

void stateAndPresets()
{
    std::vector<uint8_t> saved;
    clap_ostream os { &saved, [] (const clap_ostream* s, const void* d, uint64_t n) -> int64_t {
        auto* v = static_cast<std::vector<uint8_t>*> (s->ctx);
        v->insert (v->end(), (const uint8_t*) d, (const uint8_t*) d + n);
        return (int64_t) n;
    } };
    {
        Instance m;
        m.setup ({ { "Machine", 3 } });
        m.setup ({ { "Data A", 1.0 }, { "Data F", 0.9 }, { "Kit 2 Pan", 0.5 }, { "LFO Dest", 4 } });
        CHECK (m.state->save (m.plugin, &os));
    }
    struct Reader { const std::vector<uint8_t>* data; size_t at = 0; } reader { &saved };
    clap_istream is { &reader, [] (const clap_istream* s, void* d, uint64_t n) -> int64_t {
        auto* r = static_cast<Reader*> (s->ctx);
        const uint64_t left = r->data->size() - r->at, take = std::min (left, n);
        std::memcpy (d, r->data->data() + r->at, take);
        r->at += take;
        return (int64_t) take;
    } };
    Instance back;
    CHECK (back.state->load (back.plugin, &is));
    CHECK (back.value ("Machine") == 3 && back.value ("Data A") == 1.0 && std::fabs (back.value ("Data F") - 0.9) < 1e-6); // not reset
    CHECK (back.value ("Kit 2 Pan") == 0.5 && back.value ("LFO Dest") == 4);
    std::vector<float> l, r;
    back.render (0.5, { { 0, At::on, 60, 1.0 } }, l, r);
    CHECK (back.value ("Data A") == 1.0); // still, once it plays
    // Presets: listed, each loads, each sounds.
    const auto* discovery = static_cast<const clap_preset_discovery_factory*> (entry->get_factory (CLAP_PRESET_DISCOVERY_FACTORY_ID));
    CHECK (discovery && discovery->count (discovery) == 1);
    const char* const keys[] = { "default", "fm-keys", "fm-bell", "fm-bass", "fm-pad", "acid-fm", "kick", "long-kick", "snare", "clap",
                                 "closed-hat", "open-hat", "cymbal", "cowbell", "fm-tom", "ground-sine", "kit", "terrain-drone",
                                 "terrain-bass", "phase-lead", "phase-acid", "vox-choir", "vox-knock", "marimba", "gong", "pluck-harp",
                                 "pluck-bass", "gendy-insects", "gendy-buzz", "pulsar-rhythm", "pulsar-formant", "drawbar-organ",
                                 "full-organ", "resonator-kick", "boom-kick", "kit-explorer" };
    for (const char* key : keys)
    {
        Instance m;
        const int loaded = presetsLoaded;
        CHECK (m.loadPreset (key) && presetsLoaded == loaded + 1);
        const bool isKit = std::string (key).rfind ("kit", 0) == 0;
        m.render (1.5, { { 0, At::on, isKit ? 36 : 60, 1.0 }, { 12000, At::on, isKit ? 38 : 64, 1.0 }, { 40000, At::off, 60 }, { 40000, At::off, 64 } }, l, r);
        if (! finite (l) || peak (l) < 0.05 || peak (l) > 2.5) std::printf ("FAIL preset %s: peak %.3f\n", key, peak (l)), ++failures;
    }
    Instance m;
    CHECK (! m.loadPreset ("nothing") && presetErrors == 1);
    CHECK (m.loadPreset ("snare") && m.value ("Machine") == 2 && m.text ("Data A") == "171Hz"); // SD's TUNE
}

// KIT keeps Data A-J for the machine it was entered from, and only for it: through a preset (nobody's), a state (kept
// with it), and a state from before it was kept.
void kitAndStates()
{
    struct Bytes { std::vector<uint8_t> data; size_t at = 0; };
    auto save = [] (Instance& m) {
        Bytes b;
        clap_ostream os { &b, [] (const clap_ostream* s, const void* d, uint64_t n) -> int64_t {
            auto* v = &static_cast<Bytes*> (s->ctx)->data;
            v->insert (v->end(), (const uint8_t*) d, (const uint8_t*) d + n);
            return (int64_t) n;
        } };
        CHECK (m.state->save (m.plugin, &os));
        return b.data;
    };
    auto load = [] (Instance& m, const std::vector<uint8_t>& data) {
        Bytes b { data };
        clap_istream is { &b, [] (const clap_istream* s, void* d, uint64_t n) -> int64_t {
            auto* r = static_cast<Bytes*> (s->ctx);
            const uint64_t take = std::min<uint64_t> (r->data.size() - r->at, n);
            std::memcpy (d, r->data.data() + r->at, take);
            r->at += take;
            return (int64_t) take;
        } };
        CHECK (m.state->load (m.plugin, &is));
    };
    const char* const data[] = { "Data A", "Data B", "Data C", "Data D", "Data E", "Data F", "Data G", "Data H", "Data I", "Data J" };
    Instance fm, bd;
    bd.setup ({ { "Machine", 1 } });
    auto same = [&] (Instance& m, Instance& as) {
        for (const char* d : data) CHECK (m.value (d) == as.value (d));
    };

    // A KIT preset, then FM TONE or BD: their own defaults, not the preset's zeros.
    for (const char* key : { "kit", "kit-explorer" })
    {
        Instance a, b;
        CHECK (a.loadPreset (key) && b.loadPreset (key));
        a.setup ({ { "Machine", 0 } });
        b.setup ({ { "Machine", 1 } });
        same (a, fm);
        same (b, bd);
    }

    // BD tuned, then KIT, saved: back to BD it is tuned still; to FM TONE, FM TONE's defaults.
    Instance tuned;
    tuned.setup ({ { "Machine", 1 } });
    tuned.setup ({ { "Data A", 0.9 } });
    tuned.setup ({ { "Machine", 8 } });
    const std::vector<uint8_t> saved = save (tuned);
    {
        Instance back;
        load (back, saved);
        back.setup ({ { "Machine", 1 } });
        CHECK (std::fabs (back.value ("Data A") - 0.9) < 1e-6);
    }
    {
        Instance other;
        load (other, saved);
        other.setup ({ { "Machine", 0 } });
        same (other, fm);
    }
    // Saved again after a load: the machine is kept through it.
    {
        Instance back;
        load (back, saved);
        const std::vector<uint8_t> again = save (back);
        CHECK (again == saved);
    }
    // A state from before the machine was kept (its last pair gone, the count one less): leaving KIT resets.
    {
        std::vector<uint8_t> old (saved.begin(), saved.end() - 8);
        uint32_t n = 0;
        std::memcpy (&n, old.data() + 8, 4);
        --n;
        std::memcpy (old.data() + 8, &n, 4);
        Instance back;
        load (back, old);
        CHECK (back.value ("Machine") == 8);
        back.setup ({ { "Machine", 1 } });
        same (back, bd);
    }
    // A preset loaded over a KIT entered from BD: the machine is forgotten.
    {
        Instance m;
        m.setup ({ { "Machine", 1 } });
        m.setup ({ { "Machine", 8 } });
        CHECK (m.loadPreset ("kit"));
        m.setup ({ { "Machine", 1 } });
        same (m, bd);
    }
}

// The engine review's findings, held: a resonant filter rings out before a voice ends; a hit sounds the same wherever
// it lands on the 32-frame grid; GENDY, TERRAIN and PHASE start without a thump or a click; the filter goes into and
// out of bypass without a step; PLUCK is in tune at low BRIGHT and high STIFF; MODAL's loudest corners stay near full
// scale, and its NOISE about as loud at any rate.
void engineReview()
{
    std::vector<float> l, r;
    auto knob = [] (Instance& m, const char* name, const char* text) {
        double v = 0.0;
        CHECK (m.params->text_to_value (m.plugin, m.id (name), text, &v));
        return v;
    };
    auto rms = [] (const std::vector<float>& x) {
        double s = 0.0;
        for (float f : x) s += (double) f * f;
        return std::sqrt (s / (double) x.size());
    };

    // The end of a hit through a filter at RESO 1: its last sample as quiet as an envelope's end, not a cut.
    for (int machine : { 1, 2, 6, 7, 17 }) // BD, SD, EFM DRUM, GND, KICK
        for (int type : { 0, 1 })           // LP, HP
        {
            Instance m;
            m.setup ({ { "Machine", machine } });
            m.setup ({ { "Filter Reso", 1.0 }, { "Filter Type", type }, { "Filter Freq", 0.15 } });
            m.render (4.0, { { 0, At::on, 60, 1.0 } }, l, r);
            size_t end = l.size();
            while (end > 0 && l[end - 1] == 0.0f) --end;
            CHECK (end > 0 && end < l.size()); // it sounded, and it ended
            if (end > 0 && std::fabs (l[end - 1]) > 1e-3f)
                std::printf ("FAIL machine %d filter type %d: its voice stops on %.4f\n", machine, type, l[end - 1]), ++failures;
        }

    // A hit's first millisecond with FILTER ENV up and ATTACK 0, wherever on the grid it lands.
    for (int machine : { 2, 4, 13 }) // SD, HH, PLUCK
    {
        double lo = 1e9, hi = 0.0;
        for (uint64_t offset : { 0, 8, 16, 31 })
        {
            Instance m;
            m.setup ({ { "Machine", machine } });
            m.setup ({ { "Filter Freq", 0.3 }, { "Filter Env", 1.0 }, { "Filter Decay", knob (m, "Filter Decay", "60ms") } });
            const uint64_t at = 3200 + offset;
            m.render (0.2, { { at, At::on, 60, 1.0 } }, l, r);
            double e = 0.0;
            for (size_t i = at; i < at + 48; ++i) e += (double) l[i] * l[i];
            lo = std::min (lo, e), hi = std::max (hi, e);
        }
        if (10.0 * std::log10 (hi / lo) > 1.5) std::printf ("FAIL machine %d: its onset differs %.1f dB across the grid\n", machine, 10.0 * std::log10 (hi / lo)), ++failures;
    }

    // GENDY at its default POINTS starts as a tone with no offset (no thump), its walks off.
    {
        Instance m;
        m.setup ({ { "Machine", 14 } });
        m.setup ({ { "Data B", 0.0 }, { "Data C", 0.0 } });
        m.render (0.1, { { 0, At::on, 48, 1.0 } }, l, r);
        double s = 0.0;
        for (size_t i = 0; i < 384; ++i) s += l[i];
        CHECK (std::fabs (s / 384.0) < 0.06);
    }
    // TERRAIN and PHASE start mid-wave: faded in, their first sample near silence.
    for (int machine : { 9, 10 })
    {
        Instance m;
        m.setup ({ { "Machine", machine } });
        m.render (0.1, { { 100, At::on, 60, 1.0 } }, l, r);
        CHECK (std::fabs (l[100]) < 0.05 * peak (l));
    }

    // A tone with the LFO taking FREQ across the bypass line and back, against the same never bypassed (RESO just
    // above it): their difference moves no more than the tone does between samples, no step.
    auto tone = [&] (double reso, std::vector<float>& out) {
        Instance m;
        m.setup ({ { "Machine", 0 } });
        m.setup ({ { "FM A Level", 0.0 }, { "FM B Level", 0.0 }, { "Data H", 0.0 }, { "Data E", 0.5 }, { "Data F", 0.0 }, { "Filter Freq", 1.0 },
                   { "Filter Reso", reso }, { "LFO Dest", 1 }, { "LFO Depth", 0.03 }, { "LFO Speed", 0.4 }, { "LFO Wave", 4 } });
        std::vector<float> rr;
        m.render (2.0, { { 0, At::on, 96, 1.0 } }, out, rr);
    };
    {
        std::vector<float> a, b;
        tone (0.0101, a);
        tone (0.0, b);
        double step = 0.0;
        for (size_t i = 4801; i < a.size(); ++i) step = std::max (step, (double) std::fabs ((b[i] - a[i]) - (b[i - 1] - a[i - 1])));
        if (step > 0.02) std::printf ("FAIL the filter's bypass steps %.4f\n", step), ++failures;
    }

    // PLUCK's pitch, by autocorrelation (its period, interpolated), against the note.
    auto cents = [&] (const std::vector<float>& x, size_t from, size_t len, double hz) {
        const double expect = 48000.0 / hz;
        const int lo = (int) (expect * 0.7), hi = (int) (expect * 1.3) + 2;
        std::vector<double> c ((size_t) hi + 2, 0.0);
        for (int lag = lo - 1; lag <= hi + 1; ++lag)
        {
            double s = 0.0, e1 = 0.0, e2 = 0.0;
            for (size_t i = from; i < from + len; ++i)
                s += (double) x[i] * x[i + (size_t) lag], e1 += (double) x[i] * x[i], e2 += (double) x[i + (size_t) lag] * x[i + (size_t) lag];
            c[(size_t) lag] = s / std::sqrt (e1 * e2 + 1e-30);
        }
        int best = lo;
        for (int lag = lo; lag <= hi; ++lag)
            if (c[(size_t) lag] > c[(size_t) best]) best = lag;
        const double a = c[(size_t) best - 1], b = c[(size_t) best], d = c[(size_t) best + 1];
        const double period = best + 0.5 * (a - d) / (a - 2.0 * b + d);
        return 1200.0 * std::log2 ((48000.0 / period) / hz);
    };
    struct Pluck { double bright, stiff; int key; bool late; double within; };
    for (const Pluck& c : { Pluck { 0.0, 0.0, 84, false, 20.0 }, Pluck { 0.2, 0.0, 96, false, 10.0 }, Pluck { 0.6, 1.0, 96, true, 8.0 },
                            Pluck { 0.6, 0.0, 96, true, 5.0 } })
    {
        Instance m;
        m.setup ({ { "Machine", 13 } });
        m.setup ({ { "Data B", 1.0 }, { "Data C", c.bright }, { "Data F", c.stiff }, { "Data G", 0.0 }, { "Data E", 0.0 } });
        m.render (0.4, { { 0, At::on, c.key, 1.0 } }, l, r);
        const double hz = 440.0 * std::pow (2.0, (c.key - 69) / 12.0);
        const double off = c.late ? cents (l, 7200, 4800, hz) : cents (l, 288, (size_t) std::max (4.0 * 48000.0 / hz, 480.0), hz);
        if (std::fabs (off) > c.within)
            std::printf ("FAIL PLUCK BRIGHT %.1f STIFF %.1f key %d: %+.1f cents\n", c.bright, c.stiff, c.key, off), ++failures;
    }

    // MODAL: the default as it was; its loudest corners (BRIGHT up, NOISE up, DAMP off) near full scale, not 30
    // times over; NOISE about as loud at 192 kHz as at 48.
    {
        Instance m;
        m.setup ({ { "Machine", 12 } });
        m.render (0.5, { { 0, At::on, 60, 1.0 } }, l, r);
        CHECK (peak (l) > 0.5 && peak (l) < 0.8);
    }
    for (double material : { 0.0, 0.33, 0.66, 1.0 })
        for (int key : { 36, 60 })
        {
            Instance m;
            m.setup ({ { "Machine", 12 } });
            m.setup ({ { "Data B", material }, { "Data C", 1.0 }, { "Data D", 0.0 }, { "Data E", 0.0 }, { "Data F", 0.0 }, { "Data G", 1.0 } });
            m.render (0.5, { { 0, At::on, key, 1.0 } }, l, r);
            if (peak (l) > 3.0) std::printf ("FAIL MODAL MATERIAL %.2f key %d at its loudest corner: peak %.2f\n", material, key, peak (l)), ++failures;
        }
    double noiseAt[2] {};
    for (int k = 0; k < 2; ++k)
    {
        Instance m (k == 0 ? 48000.0 : 192000.0);
        m.setup ({ { "Machine", 12 } });
        m.setup ({ { "Data G", 1.0 } });
        m.render (0.5, { { 0, At::on, 60, 1.0 } }, l, r);
        noiseAt[k] = rms (l);
    }
    if (noiseAt[1] > 2.5 * noiseAt[0]) std::printf ("FAIL MODAL NOISE: %.3f at 192 kHz against %.3f at 48\n", noiseAt[1], noiseAt[0]), ++failures;
}

// A key played again before its note-off. Melodee moves a pending off past the key's new on, so a held ORGAN or FM
// TONE (AMP DECAY INF) hears on, on, on, then one off. Each new on lets go of the note the key was playing, as its
// note-off would, so the key strikes alone and never stacks copies of itself (a comb, different each time); the one
// off then releases it. With note ids, an off or a choke for one note leaves the key's newer note sounding.
void repeatedKeys()
{
    std::vector<float> l, r, alone;
    for (int machine : { 16, 0 }) // ORGAN, FM TONE
    {
        const int key = 57;
        {
            Instance m;
            m.setup ({ { "Machine", (double) machine } });
            m.render (1.5, { { 0, At::on, key, 1.0 } }, alone, r);
        }
        Instance m;
        m.setup ({ { "Machine", (double) machine } });
        // Three ons, the last on the 32-frame grid where the lone note started (the middle one off it, out of phase).
        const size_t last = 48000;
        m.render (2.5, { { 0, At::on, key, 1.0 }, { 17150, At::on, key, 1.0 }, { last, At::on, key, 1.0 } }, l, r);
        // Once RELEASE (250 ms) has taken the older two away, the last one sounds alone: the lone note, sample for sample.
        double off = 0.0;
        for (size_t i = 19200; i < 72000; ++i) off = std::max (off, (double) std::fabs (l[last + i] - alone[i]));
        const double level = rms (l, last + 19200, last + 72000) / rms (alone, 19200, 72000);
        if (off > 1e-5 || std::fabs (level - 1.0) > 0.01)
            std::printf ("FAIL %s: a key played again stacks (%.4f from the lone note, %.3f times its level)\n", machines[machine], off, level), ++failures;
        ++checks;
        // The one off releases it.
        m.render (1.0, { { 0, At::off, key } }, l, r);
        CHECK (rms (l, 0, 2400) > 0.05 && peak (l, 48000 * 6 / 10) < 0.002);
    }
    // Note ids: the first note's off (or choke) lets go of it alone; the second's, or an off with no id, of the second.
    {
        Instance m;
        m.setup (pureSine);
        m.render (1.0, { { 0, At::on, 60, 1.0, "", {}, 1 }, { 4800, At::on, 60, 1.0, "", {}, 2 }, { 9600, At::off, 60, 0.0, "", {}, 1 } }, l, r);
        CHECK (rms (l, 38400, 48000) > 0.1); // the second still held
        m.render (1.0, { { 0, At::off, 60, 0.0, "", {}, 2 } }, l, r);
        CHECK (peak (l, 28800) < 0.002);
        m.render (0.5, { { 0, At::on, 62, 1.0, "", {}, 3 }, { 4800, At::on, 62, 1.0, "", {}, 4 }, { 9600, At::choke, 62, 0.0, "", {}, 3 } }, l, r);
        CHECK (rms (l, 19200, 24000) > 0.1);
        m.render (0.5, { { 0, At::choke, 62, 0.0, "", {}, 4 } }, l, r);
        CHECK (peak (l, 4800) < 1e-4);
        m.render (0.5, { { 0, At::on, 64, 1.0, "", {}, 5 }, { 4800, At::off, 64 } }, l, r);
        CHECK (peak (l, 4800 + 14400) < 0.002); // no id: the key's notes, whatever theirs
    }
}

// A Data value set at a note's own frame, just before it (a step's automation, which Melodee sends with the step's
// notes), plays in that note. What a machine takes as it strikes is the new value, not the last step's: KICK's
// ACCENT, PLUCK's PICK, MODAL's MALLET, GENDY's POINTS, and FM TONE's RATIO C up to the next 32-frame step.
void dataAtNote()
{
    struct Case { int machine; const char* data; double value; int key; double velocity; };
    for (const Case& c : { Case { 17, "Data J", 1.0, 36, 0.3 }, Case { 13, "Data D", 0.05, 57, 1.0 }, Case { 12, "Data F", 1.0, 57, 1.0 },
                           Case { 14, "Data A", 0.0, 57, 1.0 }, Case { 0, "Data B", 0.0, 57, 1.0 } })
    {
        // The value set long before the note, at the note's frame, or never.
        std::vector<float> early, atNote, never, r;
        auto play = [&] (std::vector<float>& out, int when) {
            Instance m;
            m.setup ({ { "Machine", (double) c.machine } });
            if (when == 0) m.setup ({ { c.data, c.value } });
            else m.setup ({}); // the same frames before the note
            std::vector<At> events;
            if (when == 1) events.push_back ({ 1000, At::param, 0, c.value, c.data });
            events.push_back ({ 1000, At::on, c.key, c.velocity });
            m.render (0.5, events, out, r);
        };
        play (early, 0);
        play (atNote, 1);
        play (never, 2);
        if (atNote != early) std::printf ("FAIL %s: %s at the note's frame plays as the value before it\n", machines[c.machine], c.data), ++failures;
        ++checks;
        CHECK (never != early); // (the value is one the note hears)
    }
    // The same at any block size: a key again at the frame of a Data change, then its off.
    for (int machine : { 16, 12, 13, 17 })
    {
        const std::vector<At> events { { 0, At::on, 57, 1.0 }, { 3001, At::param, 0, 0.2, "Data C" }, { 3001, At::on, 57, 0.8 },
                                       { 7000, At::on, 57, 0.6 }, { 20000, At::off, 57 } };
        uint64_t first = 0;
        for (uint32_t block : { 128u, 1u, 37u, 4096u })
        {
            Instance m;
            m.setup ({ { "Machine", (double) machine } });
            std::vector<float> l, r;
            m.render (0.6, events, l, r, block);
            const uint64_t h = hash (l, r);
            if (block == 128u) first = h;
            else if (h != first) std::printf ("FAIL %s: a key again at a Data change differs at block %u\n", machines[machine], block), ++failures;
            ++checks;
        }
    }
}

// The same output whatever the block size, with notes and changes inside blocks.
void blockSizes()
{
    std::vector<At> events { { 0, At::on, 60, 1.0 }, { 777, At::param, 0, 0.7, "Filter Freq" }, { 5001, At::on, 67, 0.6 },
                             { 9000, At::param, 0, 0.3, "LFO Depth" }, { 12345, At::off, 60 }, { 20000, At::param, 0, 1, "Machine" },
                             { 20000, At::on, 60, 1.0 }, { 30011, At::on, 62, 0.9 } };
    uint64_t first = 0;
    for (uint32_t block : { 128u, 1u, 37u, 4096u, 64u })
    {
        Instance m;
        m.setup ({ { "LFO Dest", 1 }, { "Filter Reso", 0.5 } });
        std::vector<float> l, r;
        m.render (1.0, events, l, r, block);
        const uint64_t h = hash (l, r);
        if (block == 128u) first = h;
        else CHECK (h == first);
    }
}

// The second barrage too: their slower changes (TERRAIN's spin and drift, MODAL's modes, PLUCK's line, GENDY's walks,
// PULSAR's pulses) on the same grid whatever the blocks.
void blockSizesBarrage()
{
    for (int machine = 9; machine < machineCount; ++machine)
    {
        std::vector<At> events { { 0, At::on, 50, 1.0 }, { 333, At::on, 57, 0.8 }, { 4000, At::param, 0, 0.7, "Data B" },
                                 { 7777, At::off, 50 }, { 9001, At::param, 0, 0.3, "Data F" }, { 15000, At::on, 62, 0.9 },
                                 { 20000, At::off, 57 }, { 26000, At::on, 45, 1.0 } };
        uint64_t first = 0;
        for (uint32_t block : { 128u, 1u, 37u, 4096u })
        {
            Instance m;
            m.setup ({ { "Machine", (double) machine } });
            m.setup ({ { "Data H", 0.6 }, { "Data G", 0.7 }, { "LFO Dest", 4 }, { "LFO Depth", 0.4 }, { "LFO Speed", 0.7 } });
            std::vector<float> l, r;
            m.render (0.8, events, l, r, block);
            const uint64_t h = hash (l, r);
            if (block == 128u) first = h;
            else if (h != first) std::printf ("FAIL %s differs at block %u\n", machines[machine], block), ++failures;
            ++checks;
        }
    }
}

// Random settings for every machine, and the extremes: finite and bounded.
void fuzz()
{
    std::mt19937 rng (20261121);
    std::uniform_real_distribution<double> u (0.0, 1.0);
    const double rates[5] = { 48000.0, 44100.0, 96000.0, 22050.0, 192000.0 };
    for (int round = 0; round < 180; ++round)
    {
        Instance m (rates[(round / machineCount) % 5]); // every machine at every rate
        std::vector<std::pair<std::string, double>> values { { "Machine", (double) (round % machineCount) } };
        m.setup (values);
        values.clear();
        for (const auto& [name, id] : m.ids)
        {
            if (name == "Machine") continue;
            clap_param_info info {};
            for (uint32_t i = 0; i < m.params->count (m.plugin); ++i)
                if (m.params->get_info (m.plugin, i, &info) && info.id == id) break;
            double x = u (rng);
            if (round % 5 == 0) x = x < 0.5 ? 0.0 : 1.0; // the corners
            values.push_back ({ name, info.min_value + x * (info.max_value - info.min_value) });
        }
        m.setup (values);
        std::vector<At> notes;
        for (int k = 0; k < 12; ++k) notes.push_back ({ (uint64_t) k * 2000, At::on, 30 + (k * 7) % 60, u (rng) });
        notes.push_back ({ 20000, At::off, 37 });
        std::vector<float> l, r;
        m.render (1.0, notes, l, r);
        if (! finite (l) || ! finite (r) || peak (l) > 8.0)
            std::printf ("FAIL fuzz round %d (%s at %.0f Hz): peak %.2f\n", round, machines[round % machineCount], m.rate, peak (l)), ++failures;
    }
}
// The verified review fixes: same-frame hits (Melodee gives every MIDI event of a block frame 0) play the louder
// once, KIT's closed hat wins over an open one struck on the same frame either way round, the sustain pedal holds
// notes, CC 121 resets the controllers, CC 84 slides a held note to the next with no new attack, a velocity-0
// note-on with a note id lets go of that note alone, a state from a newer version is refused, and a zero sample
// rate is refused at activate.
void reviewFixes()
{
    std::vector<float> l, r;
    const std::function<double (double, const std::vector<At>&)> level = [&] (double machine, const std::vector<At>& events) {
        Instance m;
        m.setup ({ { "Machine", machine } });
        m.render (0.3, events, l, r);
        return rms (l, 0, 4800);
    };
    for (double machine : { 1.0, 8.0 }) // BD on its own, then KIT's first slot (also BD): one voice, two hits, one frame
    {
        const double loud = level (machine, { { 0, At::on, 36, 1.0 } }), quiet = level (machine, { { 0, At::on, 36, 0.2 } });
        CHECK (level (machine, { { 0, At::on, 36, 1.0 }, { 0, At::on, 36, 0.2 } }) > 0.8 * loud);  // loud then quiet: the loud one
        CHECK (level (machine, { { 0, At::on, 36, 0.2 }, { 0, At::on, 36, 1.0 } }) > 0.8 * loud);  // quiet then loud: the loud one
        CHECK (level (machine, { { 0, At::on, 36, 0.6 }, { 0, At::on, 36, 0.6 } }) < 1.4 * level (machine, { { 0, At::on, 36, 0.6 } })); // a tie: once
        CHECK (quiet < 0.5 * loud);
        { // The tie again in the choke's own window (3 ms, 144 frames): one attack alone, not the first hit
          // fading under the second. Over 4,800 frames that overlap dilutes away, which is why the wide check
          // above also passed before the fix (1.52 times here, 1.00 now; the bits themselves are the
          // fingerprint's, since a second start reseeds the noise).
            std::vector<float> oneL, oneR, twoL, twoR;
            { Instance m; m.setup ({ { "Machine", machine } }); m.render (0.3, { { 0, At::on, 36, 0.6 } }, oneL, oneR); }
            { Instance m; m.setup ({ { "Machine", machine } }); m.render (0.3, { { 0, At::on, 36, 0.6 }, { 0, At::on, 36, 0.6 } }, twoL, twoR); }
            CHECK (rms (twoL, 0, 144) < 1.15 * rms (oneL, 0, 144));
            CHECK (rms (twoL, 0, 144) > 0.85 * rms (oneL, 0, 144));
        }
    }
    const std::function<double (const std::vector<At>&)> hatTail = [&] (const std::vector<At>& events) {
        Instance m;
        m.setup ({ { "Machine", 8 } });
        m.render (0.6, events, l, r);
        return rms (l, 6000, 14400);
    };
    // KIT slots 4 (key 39, closed) and 5 (key 40, open) on one frame: the closed one wins either way round.
    const double openAlone = hatTail ({ { 0, At::on, 40, 1.0 } });
    CHECK (openAlone > 0.002);
    CHECK (hatTail ({ { 0, At::on, 40, 1.0 }, { 0, At::on, 39, 1.0 } }) < 0.4 * openAlone);
    CHECK (hatTail ({ { 0, At::on, 39, 1.0 }, { 0, At::on, 40, 1.0 } }) < 0.4 * openAlone);
    // Struck in a block that rendered nothing after it (a 0-frame call here; stamped past a block's end is the
    // same), a hit is not the next block's same-frame hit: the next block's quiet hit chokes the loud one over
    // 3 ms and plays, where same-frame rules would have kept the loud one whole. Melodee never does this (its
    // notes land on frame 0 and the block renders after them); another host's tail must not drop a hit silently.
    for (double machine : { 1.0, 8.0 })
    {
        std::vector<float> quietL, quietR, crossedL, crossedR;
        { Instance m; m.setup ({ { "Machine", machine } }); m.render (0.3, { { 0, At::on, 36, 0.2 } }, quietL, quietR); }
        Instance m;
        m.setup ({ { "Machine", machine } });
        float dummyL[1] {}, dummyR[1] {};
        float* outs[2] = { dummyL, dummyR };
        clap_audio_buffer buffer {};
        buffer.data32 = outs;
        buffer.channel_count = 2;
        Events e;
        e.note (0, CLAP_EVENT_NOTE_ON, 36, 1.0);
        OutEvents ignored;
        clap_process p {};
        p.frames_count = 0;
        p.audio_outputs = &buffer;
        p.audio_outputs_count = 1;
        p.in_events = &e.in;
        p.out_events = &ignored.out;
        CHECK (m.plugin->process (m.plugin, &p) != CLAP_PROCESS_ERROR);
        m.render (0.3, { { 0, At::on, 36, 0.2 } }, crossedL, crossedR);
        CHECK (rms (quietL, 1000, 4800) > 0.01);
        CHECK (rms (crossedL, 1000, 4800) < 1.6 * rms (quietL, 1000, 4800)); // past the choke: the quiet hit alone
        CHECK (peak (crossedL, 0, 288) > 1.3 * peak (quietL, 0, 288));     // the loud hit fades under it, not gone
    }
    // The sustain pedal: the note sounds on after its off, and lets go when the pedal lifts.
    {
        Instance m;
        m.setup (pureSine);
        m.setup ({ { "Amp Release", 0.4 } });
        m.render (0.6, { { 0, At::on, 60, 1.0 }, { 2400, At::midi, 0, 0, "", { 0xb0, 64, 127 } }, { 4800, At::off, 60 },
                         { 14400, At::midi, 0, 0, "", { 0xb0, 64, 0 } } }, l, r);
        CHECK (rms (l, 9600, 14400) > 0.08);
        CHECK (rms (l, 21600, 28800) < 0.02);
    }
    // Struck again while the pedal holds the key's earlier note, that note lets go: the quiet restrike alone is
    // left sounding, not the loud note held under it.
    {
        std::vector<float> refL, refR;
        {
            Instance one;
            one.setup (pureSine);
            one.setup ({ { "Amp Release", 0.4 } });
            one.render (0.6, { { 0, At::on, 60, 0.2 } }, refL, refR);
        }
        const double quietAlone = rms (refL, 19200, 28800);
        Instance m;
        m.setup (pureSine);
        m.setup ({ { "Amp Release", 0.4 } });
        m.render (0.6, { { 0, At::on, 60, 1.0 }, { 2400, At::midi, 0, 0, "", { 0xb0, 64, 127 } }, { 4800, At::off, 60 },
                         { 9600, At::on, 60, 0.2 } }, l, r);
        CHECK (rms (l, 8400, 9600) > 0.08); // the pedal held the loud note until the restrike (silent without one)
        CHECK (quietAlone > 0.01);
        CHECK (rms (l, 19200, 28800) < 1.6 * quietAlone);
    }
    // CC 121: the bend back to the middle, and the pedal's notes let go.
    {
        Instance m;
        m.setup (pureSine);
        m.render (0.5, { { 0, At::on, 60, 1.0 }, { 2400, At::midi, 0, 0, "", { 0xe0, 0, 127 } }, { 9600, At::midi, 0, 0, "", { 0xb0, 121, 0 } } }, l, r);
        CHECK (std::fabs (frequency (l, 14400, 24000, 48000) - 261.63) < 3.0);
        Instance p;
        p.setup (pureSine);
        p.setup ({ { "Amp Release", 0.4 } });
        p.render (0.6, { { 0, At::on, 60, 1.0 }, { 2400, At::midi, 0, 0, "", { 0xb0, 64, 127 } }, { 4800, At::off, 60 },
                         { 9600, At::midi, 0, 0, "", { 0xb0, 121, 0 } } }, l, r);
        CHECK (rms (l, 8400, 9600) > 0.08); // the pedal held the note until the reset (silent without one)
        CHECK (rms (l, 16800, 24000) < 0.02);
    }
    // A slide: CC 84 names the held key, the next note goes to its pitch with no new attack. With a decaying amp
    // envelope the slid note dies on the first note's envelope where a plain restrike (the reference, no CC 84)
    // sounds on, and the first key's later off does not let it go early.
    {
        const std::function<double (bool, size_t, size_t, std::vector<float>&)> slideLevel = [&] (bool slide, size_t from, size_t to, std::vector<float>& out) {
            Instance m;
            m.setup (pureSine);
            m.setup ({ { "Amp Decay", 0.75 } }); // about 0.85 s to silence
            std::vector<At> events { { 0, At::on, 60, 1.0 }, { 12000, At::off, 60 } };
            if (slide) events.push_back ({ 9600, At::midi, 0, 0, "", { 0xb0, 84, 60 } }); // before the note, as the host queues it
            events.push_back ({ 9600, At::on, 64, 1.0 });
            std::vector<float> rr;
            m.render (1.2, events, out, rr);
            return rms (out, from, to);
        };
        std::vector<float> slid, restruck;
        CHECK (slideLevel (true, 10200, 12000, slid) > 0.004); // still sounding just after the slide
        CHECK (std::fabs (frequency (slid, 24000, 38400, 48000) - 329.63) < 4.0); // settled on the new pitch
        const double slidTail = slideLevel (true, 38400, 48000, slid), restruckTail = slideLevel (false, 38400, 48000, restruck);
        CHECK (slidTail < 0.5 * restruckTail); // no second attack: the first envelope's tail, not a new note's
    }
    // A velocity-0 note-on is an off for its own note id alone: the key's newer note sounds on.
    {
        Instance m;
        m.setup (pureSine);
        m.setup ({ { "Amp Release", 0.4 } });
        m.render (0.6, { { 0, At::on, 60, 1.0, "", {}, 7 }, { 4800, At::on, 60, 1.0, "", {}, 8 }, { 9600, At::on, 60, 0.0, "", {}, 7 } }, l, r);
        CHECK (rms (l, 19200, 28800) > 0.08);
    }
    // A state saved by a newer version is refused whole; nothing changes.
    {
        std::vector<uint8_t> saved;
        clap_ostream os { &saved, [] (const clap_ostream* s, const void* d, uint64_t n) -> int64_t {
            auto* v = static_cast<std::vector<uint8_t>*> (s->ctx);
            v->insert (v->end(), (const uint8_t*) d, (const uint8_t*) d + n);
            return (int64_t) n;
        } };
        {
            Instance m;
            m.setup ({ { "Data A", 1.0 } });
            CHECK (m.state->save (m.plugin, &os));
        }
        CHECK (saved.size() > 12);
        saved[4] = 2; // the version, after the tag
        struct Reader { const std::vector<uint8_t>* data; size_t at = 0; } reader { &saved };
        clap_istream is { &reader, [] (const clap_istream* s, void* d, uint64_t n) -> int64_t {
            auto* rd = static_cast<Reader*> (s->ctx);
            const uint64_t left = rd->data->size() - rd->at, take = std::min (left, n);
            std::memcpy (d, rd->data->data() + rd->at, take);
            rd->at += take;
            return (int64_t) take;
        } };
        Instance back;
        const double before = back.value ("Data A");
        CHECK (! back.state->load (back.plugin, &is));
        CHECK (back.value ("Data A") == before);
    }
    // No rate, no activation (the engine divides by it).
    {
        const auto* d = factory->get_plugin_descriptor (factory, 0);
        const clap_plugin* raw = factory->create_plugin (factory, &host, d->id);
        CHECK (raw != nullptr && raw->init (raw));
        const bool activated = raw->activate (raw, 0.0, 1, 4096);
        CHECK (! activated);
        if (activated) raw->deactivate (raw);
        raw->destroy (raw);
    }
    // A process call with nowhere to write is an error, not a crash.
    {
        Instance m;
        float* outs[2] = { nullptr, nullptr };
        clap_audio_buffer buffer {};
        buffer.data32 = outs;
        buffer.channel_count = 2;
        Events e;
        OutEvents ignored;
        clap_process p {};
        p.frames_count = 128;
        p.audio_outputs = &buffer;
        p.audio_outputs_count = 1;
        p.in_events = &e.in;
        p.out_events = &ignored.out;
        CHECK (m.plugin->process (m.plugin, &p) == CLAP_PROCESS_ERROR);
        float left[128] {};
        outs[0] = left; // a right channel claimed but not given is an error too
        CHECK (m.plugin->process (m.plugin, &p) == CLAP_PROCESS_ERROR);
    }
    // Eight held FM voices: how far past full scale the summed output goes (there is no limiter by design; the
    // README's levels note stands on this staying near the three-note figure).
    {
        Instance m;
        std::vector<At> notes;
        for (int k = 0; k < 8; ++k) notes.push_back ({ 0, At::on, 48 + 3 * k, 1.0 });
        m.render (1.0, notes, l, r);
        const double eight = std::max (peak (l), peak (r));
        std::printf ("  eight held FM voices peak %.3f (%.1f dBFS)\n", eight, 20.0 * std::log10 (eight));
        CHECK (eight > 0.5 && eight < 3.2);
    }
}

// The 0.1.7 review's same-frame findings (every MIDI event of a block arrives at frame 0 in Melodee):
// - the same key twice on one frame plays the louder hit once on the gated machines too (a tie to the later), as
//   on the drums: before, the later, quieter hit played;
// - a note on and off on one frame (Melodee sends a note shorter than its 128-frame block that way) plays to the
//   end of the 32-frame step it started in, then releases: before, the gated machines let it go before it had
//   played a sample, and it was silent;
// - a note choked on the frame it was struck (CC 120, a host's choke, a stolen voice) never sounds: before, it
//   played its 3 ms choke fade at full level (a BD hit with CC 120 peaked at 0.62).
void sameFrameReview()
{
    const int gated[] = { 0, 9, 10, 11, 12, 13, 14, 15, 16 }; // the machines whose amp hears note-off on their own
    std::vector<float> l, r;
    const std::function<void (int, const std::vector<At>&, std::vector<float>&)> play = [&] (int machine, const std::vector<At>& events,
                                                                                            std::vector<float>& out) {
        Instance m;
        m.setup ({ { "Machine", (double) machine } });
        std::vector<float> right;
        m.render (0.5, events, out, right);
    };
    for (int machine : gated)
    {
        // The louder of two hits of one key on one frame, either way round.
        std::vector<float> loud, quiet, loudQuiet, quietLoud, tie, once, released, releasedAlone;
        play (machine, { { 0, At::on, 57, 1.0 } }, loud);
        play (machine, { { 0, At::on, 57, 0.2 } }, quiet);
        play (machine, { { 0, At::on, 57, 1.0 }, { 0, At::on, 57, 0.2 } }, loudQuiet);
        play (machine, { { 0, At::on, 57, 0.2 }, { 0, At::on, 57, 1.0 } }, quietLoud);
        play (machine, { { 0, At::on, 57, 0.6 }, { 0, At::on, 57, 0.6 } }, tie);
        play (machine, { { 0, At::on, 57, 0.6 } }, once);
        const double loudLevel = rms (loud, 0, 4800);
        CHECK (rms (quiet, 0, 4800) < 0.5 * loudLevel); // (the velocities are heard)
        if (loudQuiet != loud) // the quieter later hit never sounds: the loud hit alone, to the bit
            std::printf ("FAIL %s: a quieter hit of the key on the same frame plays (%.3f of the loud hit)\n", machines[machine],
                         rms (loudQuiet, 0, 4800) / loudLevel), ++failures;
        ++checks;
        // The later hit starts with other noise than a lone one: MODAL's and PLUCK's strikes vary by about a fifth
        // with it, so these allow that (the quiet hit is a fifth of the loud one; a tie stacked, about twice one).
        const double quietLoudLevel = rms (quietLoud, 0, 4800) / loudLevel, tieLevel = rms (tie, 0, 4800) / rms (once, 0, 4800);
        if (quietLoudLevel < 0.6 || tieLevel > 1.4 || tieLevel < 0.6) // the loud one, and a tie once
            std::printf ("FAIL %s: quiet then loud on one frame at %.3f of the loud hit, a tie at %.3f of one\n", machines[machine], quietLoudLevel,
                         tieLevel), ++failures;
        ++checks;
        // On, on, off: the one off lets go of the louder hit that plays.
        play (machine, { { 0, At::on, 57, 1.0 }, { 0, At::on, 57, 0.2 }, { 9600, At::off, 57 } }, released);
        play (machine, { { 0, At::on, 57, 1.0 }, { 9600, At::off, 57 } }, releasedAlone);
        CHECK (released == releasedAlone);
        // On, off, on: the off between lets the second strike through (a sequenced note shorter than the block and a
        // pad on its key in that block). It plays held, as it would alone, whatever the first one's velocity; the
        // first, let go before it sounded, gives way to it.
        std::vector<float> restruck, heldAlone;
        play (machine, { { 0, At::on, 57, 1.0 }, { 0, At::off, 57 }, { 0, At::on, 57, 0.5 } }, restruck);
        play (machine, { { 0, At::on, 57, 0.5 } }, heldAlone);
        const double restruckLevel = rms (restruck, 4800, 12000) / rms (heldAlone, 4800, 12000);
        if (restruckLevel < 0.6 || restruckLevel > 1.4 || (machine == 0 && restruck != heldAlone))
            std::printf ("FAIL %s: on, off, on of one key on one frame holds the second at %.3f of it alone\n", machines[machine], restruckLevel), ++failures;
        ++checks;

        // A note on and off on one frame sounds: about as a note held for one 128-frame block does.
        std::vector<float> shortNote, oneBlock, pedal, allOff;
        play (machine, { { 0, At::on, 57, 1.0 }, { 0, At::off, 57 } }, shortNote);
        play (machine, { { 0, At::on, 57, 1.0 }, { 128, At::off, 57 } }, oneBlock);
        const double shortLevel = rms (shortNote, 0, 4800), blockLevel = rms (oneBlock, 0, 4800);
        if (! (blockLevel > 0.01 && shortLevel > 0.5 * blockLevel && shortLevel < 1.05 * blockLevel))
            std::printf ("FAIL %s: a note on and off on one frame at %.4f, one held a block at %.4f\n", machines[machine], shortLevel, blockLevel), ++failures;
        ++checks;
        CHECK (peak (shortNote, 24000) < 0.002); // and it ends
        // Let go by the pedal lifting on that frame, or by CC 123 there: the same.
        play (machine, { { 0, At::midi, 0, 0, "", { 0xb0, 64, 127 } }, { 0, At::on, 57, 1.0 }, { 0, At::off, 57 },
                         { 0, At::midi, 0, 0, "", { 0xb0, 64, 0 } } }, pedal);
        CHECK (pedal == shortNote);
        play (machine, { { 0, At::on, 57, 1.0 }, { 0, At::midi, 0, 0, "", { 0xb0, 123, 0 } } }, allOff);
        CHECK (allOff == shortNote);
        // The same at any block size, off the 32-frame grid too.
        uint64_t first = 0;
        for (uint32_t block : { 128u, 1u, 37u, 4096u })
        {
            Instance m;
            m.setup ({ { "Machine", (double) machine } });
            std::vector<float> bl, br;
            m.render (0.4, { { 1000, At::on, 57, 1.0 }, { 1000, At::off, 57 }, { 3001, At::on, 60, 0.7 }, { 3001, At::off, 60 } }, bl, br, block);
            const uint64_t h = hash (bl, br);
            if (block == 128u)
            {
                first = h;
                CHECK (rms (bl, 1000, 3000) > 0.01);
            }
            else if (h != first) std::printf ("FAIL %s: a note on and off on one frame differs at block %u\n", machines[machine], block), ++failures;
            ++checks;
        }
    }

    // On, off, on in KIT: a VOX or PULSAR slot (gated, as on their own) plays the second hit held, as it would alone.
    // On a one-shot slot (BD) the off means nothing, so the louder first hit still plays, to the bit as alone.
    for (double slotMachine : { 9.0, 10.0, 0.0 })
    {
        const std::function<void (const std::vector<At>&, std::vector<float>&)> kitPlay = [&] (const std::vector<At>& events, std::vector<float>& out) {
            Instance m;
            m.setup ({ { "Machine", 8 } });
            m.setup ({ { "Kit 1 Machine", slotMachine } });
            std::vector<float> right;
            m.render (0.5, events, out, right);
        };
        std::vector<float> restruck, alone;
        kitPlay ({ { 0, At::on, 36, 1.0 }, { 0, At::off, 36 }, { 0, At::on, 36, 0.5 } }, restruck);
        if (slotMachine == 0.0)
        {
            kitPlay ({ { 0, At::on, 36, 1.0 } }, alone);
            CHECK (restruck == alone);
            continue;
        }
        kitPlay ({ { 0, At::on, 36, 0.5 } }, alone);
        const double level = rms (restruck, 4800, 12000) / rms (alone, 4800, 12000);
        if (! (rms (alone, 4800, 12000) > 0.01 && level > 0.6 && level < 1.4))
            std::printf ("FAIL KIT %s slot: on, off, on on one frame holds the second at %.3f of it alone\n", slotMachine == 9.0 ? "VOX" : "PULSAR",
                         level), ++failures;
        ++checks;
    }

    // Choked on the frame it was struck, a hit never sounds: CC 120, a host's choke, and Melodee's silence (CC 120
    // and 123 on all 16 channels), on the drums, KIT, KICK and the gated machines.
    for (int machine : { 1, 8, 17, 0, 16, 12 })
    {
        const int key = machine == 8 || machine == 17 || machine == 1 ? 36 : 57;
        std::vector<float> cc120, choke, silence, later;
        play (machine, { { 0, At::on, key, 1.0 }, { 0, At::midi, 0, 0, "", { 0xb0, 120, 0 } } }, cc120);
        play (machine, { { 1000, At::on, key, 1.0 }, { 1000, At::choke, key } }, choke);
        std::vector<At> storm { { 0, At::on, key, 1.0 } };
        for (uint8_t c = 0; c < 16; ++c)
            storm.push_back ({ 0, At::midi, 0, 0, "", { (uint8_t) (0xb0 | c), 120, 0 } }), storm.push_back ({ 0, At::midi, 0, 0, "", { (uint8_t) (0xb0 | c), 123, 0 } });
        play (machine, storm, silence);
        if (peak (cc120) != 0.0 || peak (choke) != 0.0 || peak (silence) != 0.0)
            std::printf ("FAIL %s: a hit choked on its own frame sounds (CC 120 %.3f, choke %.3f, silence %.3f)\n", machines[machine], peak (cc120),
                         peak (choke), peak (silence)), ++failures;
        ++checks;
        // A frame later it has sounded: it fades over the 3 ms choke and is gone by the 32-frame step after it.
        play (machine, { { 0, At::on, key, 1.0 }, { 1, At::midi, 0, 0, "", { 0xb0, 120, 0 } } }, later);
        CHECK (peak (later, 0, 160) > 0.0 && peak (later, 192) == 0.0);
    }
    // Nine notes on one frame on FM TONE: the oldest is stolen before it sounds, so the other eight play alone.
    {
        std::vector<At> nine, eight;
        for (int k = 0; k < 9; ++k)
        {
            nine.push_back ({ 0, At::on, 48 + 3 * k, 0.5 });
            if (k > 0) eight.push_back ({ 0, At::on, 48 + 3 * k, 0.5 });
        }
        std::vector<float> withNine, withEight;
        play (0, nine, withNine);
        play (0, eight, withEight);
        double off = 0.0;
        for (size_t i = 0; i < withNine.size(); ++i) off = std::max (off, (double) std::fabs (withNine[i] - withEight[i]));
        if (off > 1e-5) std::printf ("FAIL FM TONE: a note stolen on the frame it was struck sounds (%.4f off the eight)\n", off), ++failures;
        ++checks;
    }
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: host Machine.clap OUT_FOLDER\n"); return 2; }
    void* lib = dlopen (argv[1], RTLD_NOW | RTLD_LOCAL);
    if (lib == nullptr) { std::printf ("cannot open %s: %s\n", argv[1], dlerror()); return 1; }
    entry = static_cast<const clap_plugin_entry*> (dlsym (lib, "clap_entry"));
    CHECK (entry && entry->init (argv[1]));
    factory = static_cast<const clap_plugin_factory*> (entry->get_factory (CLAP_PLUGIN_FACTORY_ID));
    CHECK (factory != nullptr);
    if (factory == nullptr) return 1;
    description();
    parameters();
    fmTone (argv[2]);
    drums (argv[2]);
    kit (argv[2]);
    barrage (argv[2]);
    struckAmpEnvelopes();
    screen();
    hostCopies();
    engineFindings();
    stateAndPresets();
    kitAndStates();
    engineReview();
    repeatedKeys();
    dataAtNote();
    reviewFixes();
    sameFrameReview();
    blockSizes();
    blockSizesBarrage();
    fuzz();
    entry->deinit();
    std::printf ("%s: %d checks, %d failed\n", failures ? "FAILED" : "ok", checks, failures);
    return failures ? 1 : 0;
}
