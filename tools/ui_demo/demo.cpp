// Copyright (C) 2026 hdslave
// Silent CLAP instrument for trying the UI on a plug-in track.
#include "melodee_screen.h"
#include "../ui_preview/model.h"
#include <atomic>
#include <new>
namespace {
using namespace fixture;
constexpr const char *features[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER, nullptr};
const clap_plugin_descriptor_t descriptor = {
    CLAP_VERSION, "com.hdslave.machine.ui-demo", "Machine UI Demo", "hdslave", "", "", "",
    "1",          "Silent screen demonstration", features};
struct Demo {
    clap_plugin_t plugin{};
    const clap_host_t *host;
    const clap_host_melodee_screen_t *display = nullptr;
    const clap_host_params_t *params = nullptr;
    Screen screen;
    std::atomic<float> value[storageSize]{}, pending[storageSize]{};
    std::atomic<int> slot{0};
    std::atomic<bool> animates{false}, outgoing{false};
    std::atomic<unsigned> strikes[8]{};
    unsigned seen[8]{};
    uint64_t stamp[8]{};
    explicit Demo(const clap_host_t *h) : host(h) {
        for (int i = 0; i < paramCount; ++i) {
            int id = idAt(i);
            value[id].store(defaultValue(id));
            pending[id].store(-1.f);
        }
    }
    Model snapshot() const {
        return build([&](int id) { return value[id].load(std::memory_order_relaxed); }, slot.load());
    }
    void repaint() const {
        if (display)
            display->request_paint(host);
    }
};
Demo &demo(const clap_plugin_t *p) {
    return *static_cast<Demo *>(p->plugin_data);
}
int steps(clap_id id) {
    if (id == 0)
        return 9;
    if (id == 19)
        return 3;
    if (id == 31)
        return 5;
    if (id == 32)
        return 11;
    if (id >= slotsBase && (id - slotsBase) % slotStride == 0)
        return 7;
    return 0;
}
bool bipolar(clap_id id) {
    return id == 20 || id == 30 || (id >= slotsBase && (id - slotsBase) % slotStride == 10);
}
double plain(clap_id id, float v) {
    if (int n = steps(id))
        return notch(v, n);
    return bipolar(id) ? 2. * v - 1 : v;
}
float norm(clap_id id, double v) {
    if (!std::isfinite(v))
        return 0;
    if (int n = steps(id))
        return float(std::round(std::clamp(v, 0., double(n - 1)))) / (n - 1);
    return float(std::clamp(bipolar(id) ? (v + 1) * .5 : v, 0., 1.));
}
bool info(const clap_plugin_t *, uint32_t i, clap_param_info_t *out) {
    if (i >= paramCount)
        return false;
    int id = idAt(i);
    *out = {};
    out->id = id;
    out->flags = CLAP_PARAM_IS_AUTOMATABLE;
    out->min_value = bipolar(id) ? -1 : 0;
    out->max_value = 1;
    if (int n = steps(id)) {
        out->flags |= CLAP_PARAM_IS_STEPPED;
        out->max_value = n - 1;
    }
    out->default_value = plain(id, defaultValue(id));
    if (!id)
        std::strcpy(out->name, "Machine");
    else if (id <= 8)
        std::snprintf(out->name, sizeof out->name, "Data %c", 'A' + id - 1);
    else if (id < 17) {
        std::strcpy(out->module, "FM Env");
        std::strcpy(out->name, envNames[id - 9]);
    } else if (id < baseCount) {
        std::strcpy(out->module, id < 23 ? "Filter" : id < 29 ? "Amp" : "LFO");
        std::strcpy(out->name, shared[id - 17]);
    } else {
        int field = (id - slotsBase) % slotStride;
        std::snprintf(out->module, sizeof out->module, "Slot %d", 1 + (id - slotsBase) / slotStride);
        if (field > 0 && field < 9)
            std::snprintf(out->name, sizeof out->name, "Data %c", 'A' + field - 1);
        else
            std::strcpy(out->name, field == 0 ? "Machine" : field == 9 ? "Level" : "Pan");
    }
    return true;
}
bool valueText(const clap_plugin_t *p, clap_id id, double v, char *out, uint32_t size) {
    if (!validId(id) || !size)
        return false;
    auto &d = demo(p);
    bool slotField = id >= slotsBase;
    int slot = slotField ? (id - slotsBase) / slotStride : d.slot.load();
    auto model = build(
        [&](uint32_t key) {
            if (key == id)
                return norm(id, v);
            if (slotField && key == 0)
                return 1.f;
            return d.value[key].load();
        },
        slot);
    for (int i = 0; i < model.rowCount; ++i)
        if (model.rows[i].id == id) {
            std::snprintf(out, size, "%s", model.rows[i].text);
            return true;
        }
    std::snprintf(out, size, "%.3f", v);
    return true;
}
void strike(Demo &d, int key) {
    int machine = notch(d.value[0].load(), 9), voice = 0;
    if (machine == 8) {
        if (key < 36 || key > 43)
            return;
        voice = key - 36;
        d.slot.store(voice);
    } else if (machine == 0)
        voice = key & 7;
    d.strikes[voice].fetch_add(1);
    d.repaint();
}
void flush(const clap_plugin_t *p, const clap_input_events_t *in, const clap_output_events_t *out) {
    auto &d = demo(p);
    bool dirty = false;
    for (uint32_t i = 0; in && i < in->size(in); ++i) {
        const auto *e = in->get(in, i);
        if (e->space_id != CLAP_CORE_EVENT_SPACE_ID)
            continue;
        if (e->type == CLAP_EVENT_PARAM_VALUE && e->size >= sizeof(clap_event_param_value_t)) {
            const auto *v = reinterpret_cast<const clap_event_param_value_t *>(e);
            if (validId(v->param_id)) {
                d.value[v->param_id].store(norm(v->param_id, v->value));
                dirty = true;
            }
        } else if (e->type == CLAP_EVENT_NOTE_ON && e->size >= sizeof(clap_event_note_t)) {
            strike(d, reinterpret_cast<const clap_event_note_t *>(e)->key);
        } else if (e->type == CLAP_EVENT_MIDI && e->size >= sizeof(clap_event_midi_t)) {
            const auto *msg = reinterpret_cast<const clap_event_midi_t *>(e)->data;
            if ((msg[0] & 0xf0) == 0x90 && msg[2])
                strike(d, msg[1]);
        }
    }
    if (out && d.outgoing.exchange(false)) {
        for (int i = 0; i < paramCount; ++i) {
            int id = idAt(i);
            float v = d.pending[id].exchange(-1.f);
            if (v < 0)
                continue;
            clap_event_param_value_t e{};
            e.header = {sizeof(e), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, CLAP_EVENT_IS_LIVE};
            e.param_id = id;
            e.note_id = e.port_index = e.channel = e.key = -1;
            e.value = plain(id, v);
            if (!out->try_push(out, &e.header)) {
                float empty = -1;
                d.pending[id].compare_exchange_strong(empty, v);
                d.outgoing.store(true);
            }
        }
    }
    if (dirty)
        d.repaint();
}
const clap_plugin_params_t parameters = {
    [](const clap_plugin_t *) -> uint32_t { return paramCount; },
    info,
    [](const clap_plugin_t *p, clap_id id, double *v) {
        if (!validId(id))
            return false;
        *v = plain(id, demo(p).value[id].load());
        return true;
    },
    valueText,
    [](const clap_plugin_t *, clap_id, const char *, double *) { return false; },
    flush};
const clap_plugin_audio_ports_t audioPorts = {
    [](const clap_plugin_t *, bool in) -> uint32_t { return in ? 0 : 1; },
    [](const clap_plugin_t *, uint32_t i, bool in, clap_audio_port_info_t *v) {
        if (in || i)
            return false;
        *v = {};
        v->id = 0;
        std::strcpy(v->name, "Stereo out");
        v->flags = CLAP_AUDIO_PORT_IS_MAIN;
        v->channel_count = 2;
        v->port_type = CLAP_PORT_STEREO;
        v->in_place_pair = CLAP_INVALID_ID;
        return true;
    }};
const clap_plugin_note_ports_t notePorts = {
    [](const clap_plugin_t *, bool in) -> uint32_t { return in ? 1 : 0; },
    [](const clap_plugin_t *, uint32_t i, bool in, clap_note_port_info_t *v) {
        if (!in || i)
            return false;
        *v = {};
        v->id = 0;
        std::strcpy(v->name, "Pads");
        v->supported_dialects = CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_CLAP;
        v->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
        return true;
    }};
bool paint(const clap_plugin_t *p, const melodee_screen_canvas_t *c) {
    if (c->width != width || c->height != height || c->stride < width || c->stride > INT32_MAX)
        return false;
    auto &d = demo(p);
    auto m = d.snapshot();
    Host h{};
    h.nowMs = c->now_ms;
    h.accent = c->accent;
    h.shift = c->shift;
    h.playing = c->playing;
    for (int k = 0; k < 3; ++k) {
        h.pot[k] = c->knobs[k].position;
        h.engaged[k] = c->knobs[k].engaged;
    }
    for (int k = 0; k < 8; ++k) {
        unsigned seq = d.strikes[k].load();
        if (seq != d.seen[k]) {
            d.seen[k] = seq;
            d.stamp[k] = h.nowMs;
        }
        if (seq && h.nowMs >= d.stamp[k] && h.nowMs - d.stamp[k] < 220)
            m.voices[k] = 1 - float(h.nowMs - d.stamp[k]) / 220;
    }
    bool changed = d.screen.paint(m, h, c->pixels, int(c->stride), c->whole);
    d.animates.store(d.screen.animating(m, h));
    return changed;
}
bool input(const clap_plugin_t *p, const melodee_screen_input_t *e) {
    if (e->kind > MELODEE_SCREEN_NUDGE)
        return false;
    auto &d = demo(p);
    auto m = d.snapshot();
    Edit edits[4];
    int n = 0;
    bool used = d.screen.input(m, Key(e->kind), e->value, e->shift, edits, n);
    for (int i = 0; i < n; ++i) {
        auto edit = edits[i];
        if (edit.id == slotSelector)
            d.slot.store(notch(edit.value, 8));
        else if (validId(edit.id)) {
            d.value[edit.id].store(edit.value);
            d.pending[edit.id].store(edit.value);
            d.outgoing.store(true);
            if (d.params)
                d.params->request_flush(d.host);
        }
    }
    if (used)
        d.repaint();
    return used;
}
const clap_plugin_melodee_screen_t display = {paint, input, [](const clap_plugin_t *p, clap_id ids[3]) {
                                                  auto &d = demo(p);
                                                  d.screen.knobs(d.snapshot(), ids);
                                                  for (int k = 0; k < 3; ++k)
                                                      if (ids[k] == slotSelector)
                                                          ids[k] = CLAP_INVALID_ID;
                                              }};
clap_process_status process(const clap_plugin_t *p, const clap_process_t *block) {
    flush(p, block->in_events, block->out_events);
    for (uint32_t i = 0; i < block->audio_outputs_count; ++i) {
        auto &port = block->audio_outputs[i];
        port.constant_mask = UINT64_MAX;
        for (uint32_t ch = 0; ch < port.channel_count; ++ch) {
            if (port.data32)
                std::fill_n(port.data32[ch], block->frames_count, 0.f);
            if (port.data64)
                std::fill_n(port.data64[ch], block->frames_count, 0.);
        }
    }
    auto &d = demo(p);
    if (d.animates.load())
        d.repaint();
    return CLAP_PROCESS_CONTINUE;
}
const clap_plugin_t *create(const clap_plugin_factory_t *, const clap_host_t *host, const char *id) {
    if (!host || std::strcmp(id, descriptor.id) || !clap_version_is_compatible(host->clap_version))
        return nullptr;
    auto *d = new (std::nothrow) Demo(host);
    if (!d)
        return nullptr;
    d->plugin = {&descriptor,
                 d,
                 [](const clap_plugin_t *p) {
                     auto &d = demo(p);
                     d.display = static_cast<const clap_host_melodee_screen_t *>(
                         d.host->get_extension(d.host, CLAP_EXT_MELODEE_SCREEN_HOST));
                     d.params = static_cast<const clap_host_params_t *>(
                         d.host->get_extension(d.host, CLAP_EXT_PARAMS));
                     return true;
                 },
                 [](const clap_plugin_t *p) { delete &demo(p); },
                 [](const clap_plugin_t *, double, uint32_t, uint32_t) { return true; },
                 [](const clap_plugin_t *) {},
                 [](const clap_plugin_t *) { return true; },
                 [](const clap_plugin_t *) {},
                 [](const clap_plugin_t *) {},
                 process,
                 [](const clap_plugin_t *, const char *id) -> const void * {
                     if (!std::strcmp(id, CLAP_EXT_MELODEE_SCREEN))
                         return &display;
                     if (!std::strcmp(id, CLAP_EXT_PARAMS))
                         return &parameters;
                     if (!std::strcmp(id, CLAP_EXT_AUDIO_PORTS))
                         return &audioPorts;
                     if (!std::strcmp(id, CLAP_EXT_NOTE_PORTS))
                         return &notePorts;
                     return nullptr;
                 },
                 [](const clap_plugin_t *) {}};
    return &d->plugin;
}
const clap_plugin_factory_t factory = {
    [](const clap_plugin_factory_t *) -> uint32_t { return 1; },
    [](const clap_plugin_factory_t *, uint32_t i) { return i ? nullptr : &descriptor; }, create};
}
extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION, [](const char *) { return true; }, []() {},
    [](const char *id) -> const void * {
        return !std::strcmp(id, CLAP_PLUGIN_FACTORY_ID) ? &factory : nullptr;
    }};
