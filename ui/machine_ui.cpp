// Copyright (C) 2026 hdslave
#include "raster.h"
#include "art.h"
#include <cstdio>

namespace machine::ui {
using namespace draw;
namespace {
constexpr int listY = 52, rowH = 15, rowsVisible = 8, listW = 196, contextX = 202, contextW = 114;
constexpr int headW = contextX, headH = 35, cardY = 176;
constexpr int bannerY = placeBarRows + headH + 1, plotOffset = listY - bannerY, plotHeight = 97;
constexpr uint32_t none = 0xffffffffu;
constexpr uint64_t sweepMs = 150; // the heading's wipe from one machine to the next
int boundedCursor(int cursor, const Model &m) {
    return index(cursor, count(m) - 1);
}
bool rowDiff(const Row &a, const Row &b) {
    return a.group != b.group || a.role != b.role || a.data != b.data || a.value != b.value ||
           a.initial != b.initial || a.bipolar != b.bipolar || a.steps != b.steps ||
           std::strncmp(a.name, b.name, sizeof a.name) || std::strncmp(a.text, b.text, sizeof a.text);
}
uint64_t elapsed(uint64_t now, uint64_t from) {
    return now >= from ? now - from : 0;
}
void bar(Raster &r, int x, int y, int w, const Row &row, Ink ink) {
    if (w < 3)
        return;
    int v = position(row.value, w - 1), d = position(row.initial, w - 1),
        origin = row.bipolar ? (w - 1) / 2 : 0;
    r.box(x, y, w, 2, rule);
    if (row.steps > 1) {
        int n = std::min<int>(row.steps, w / 3 + 1);
        for (int k = 0; k < n; ++k)
            r.dot(x + k * (w - 1) / (n - 1), y - 1, muted);
    }
    r.box(x + std::min(origin, v), y, std::abs(v - origin) + 1, 2, ink);
    r.dot(x + d, y + 2, muted);
    if (row.bipolar)
        r.dot(x + origin, y - 1, paper);
}
struct VoiceLabel {
    const char *title;
    const char *description;
};
constexpr VoiceLabel labels[] = {{"FM TONE", "4-OPERATOR FM"},     {"BASS DRUM", "BD / PITCH SWEEP"},
                                 {"SNARE", "SD / BODY + NOISE"},   {"CLAP", "CP / NOISE BURSTS"},
                                 {"HI-HAT", "HH / METAL + NOISE"}, {"CYMBAL", "6 METAL OSCILLATORS"},
                                 {"FM DRUM", "EFM / PITCHED HIT"}, {"GROUND", "SINE, NOISE, IMPULSE"},
                                 {"KIT", "8 DRUM SLOTS"},          {"TERRAIN", "ORBITING A SURFACE"},
                                 {"PHASE", "VECTOR PHASESHAPER"},  {"VOX", "VOWEL SYNTH"},
                                 {"MODAL", "STRUCK RESONATOR"},    {"PLUCK", "PLUCKED STRING"},
                                 {"GENDY", "RANDOM-WALK SYNTH"},   {"PULSAR", "GATED PULSE TRAIN"},
                                 {"ORGAN", "8 DRAWBARS"},          {"KICK", "PULSE RESONATOR"}};
static_assert(std::size(labels) == machineCount && std::size(art::banners) == machineCount);

bool cowbell(const Model &m, int voice) {
    if (voice == int(Kind::CyCb))
        for (int i = 0; i < count(m); ++i)
            if (m.rows[i].role == Role::Data && m.rows[i].data == 0)
                return m.rows[i].value >= .5f;
    return false;
}

void heading(Raster &r, int machine, int slot, int drum, bool bell) {
    const bool kit = machine == int(Kind::Kit);
    const int voice = kit ? drum : machine;
    const VoiceLabel label = bell ? VoiceLabel{"COWBELL", "TWIN SQUARE WAVES"} : labels[voice];
    char slotLabel[24];
    if (kit)
        std::snprintf(slotLabel, sizeof slotLabel, "KIT %02d / %s", slot + 1, machineNames[voice]);
    r.box(0, 0, r.w, r.h, black);
    emblem(r, 5, 2, voice);
    r.text(42, 3, label.title, 0xff32616b, 24, true);
    r.text(44, 3, label.title, 0xff804485, 24, true);
    r.text(43, 3, label.title, paper, 24, true);
    r.text(43, 22, kit ? slotLabel : label.description, muted);
    r.box(4, r.h - 1, r.w - 4, 1, violet);
}

void meters(Raster &r, const Model &m, const Host &h) {
    r.box(0, 0, r.w, r.h, black);
    const bool kit = machineId(m) == int(Kind::Kit);
    r.text(3, 2, kit ? "SLOTS" : "VOICE", muted);
    if (h.playing)
        r.box(43, 5, 2, 2, cyan);
    for (int k = 0; k < 8; ++k) {
        int x = 51 + k * 8, level = position(m.voices[k], 4);
        for (int i = 0; i < 4; ++i)
            r.box(x, 9 - i * 2, 6, 1, i < level ? cyan : rule);
        if (kit && k == slotId(m))
            r.box(x, 11, 6, 1, paper);
    }
    r.box(0, r.h - 1, r.w, 1, violet);
}

void tabs(Raster &r, int machine, Group active, int originX) {
    r.box(0, 0, r.w, r.h, black);
    int x = 4 - originX;
    for (int g : {0, 5, 1, 2, 3, 4}) {
        if ((g == 1 && machine != 8) || (g == 5 && machine != 0))
            continue;
        int w = textWidth(groups[g], 8) + 12;
        const bool current = int(active) == g;
        for (int y = 0; y < 12; ++y) {
            int bevel = (11 - y) / 3;
            r.box(x + bevel, y, w - 3, 1, current ? 0xffbfa3d2 : card);
        }
        r.text(x + 6, 2, groups[g], current ? black : muted);
        x += w + 3;
    }
}
// Bounds of the lettering in plot coordinates. Ink reaches full strength nine
// pixels past a label horizontally or seven pixels past it vertically.
struct LabelBounds {
    int left = contextW, right = -1, top = art::bandHeight, bottom = -1;
    void include(int x, int y) {
        left = std::min(left, x);
        right = std::max(right, x);
        top = std::min(top, y);
        bottom = std::max(bottom, y);
    }
    float exposure(int x, int y) const {
        int dx = std::max({left - x, 0, x - right});
        int dy = std::max({top - y, 0, y - bottom});
        if (right < left || dx >= 9 || dy >= 7)
            return 1.f;
        float t = unit(std::sqrt(dx * dx / 81.f + dy * dy / 49.f));
        return t * t * (3.f - 2.f * t);
    }
};

void diagram(Raster &r, const Model &m, Group group) {
    // One tile owns both sides of the tab/plot boundary. Partial paints always
    // blend onto clean ink, so repeatedly moving a control cannot build up colour.
    Raster nav(r.buf, r.w, plotOffset);
    tabs(nav, machineId(m), group, contextX);
    Raster plot(r.buf + plotOffset * r.w, r.w, plotHeight);
    context(plot, m, group);
    LabelBounds caption, mode, sublabel;
    for (int y = 0; y < art::bandHeight; ++y) {
        for (int x = 0; x < r.w; ++x) {
            Ink ink = plot.buf[y * r.w + x];
            if (ink == muted || ink == valueInk)
                (y < 12 ? caption : sublabel).include(x, y);
            if (y < 12 && ink == orchid)
                mode.include(x, y);
        }
    }
    const auto *print = art::banners[machineId(m)];
    for (int y = 0; y < art::bandHeight; ++y) {
        Ink *line = r.buf + y * r.w;
        int start = r.w - art::bandWidth;
        if (y < plotOffset) {
            // Allow two clear pixels after the rightmost tab on this scanline.
            for (int x = r.w - 1; x >= start - 2; --x)
                if (line[x] != black) {
                    start = x + 3;
                    break;
                }
        }
        int plotY = y - plotOffset;
        // In the plot, keep three clear pixels after the caption and its mode text too, as after the tabs: the print
        // must not run under a heading's lettering (it covered the end of RESONATOR and PICK POINT).
        if (plotY >= 0 && plotY >= std::min(caption.top, mode.top) - 2 &&
            plotY <= std::max(caption.bottom, mode.bottom) + 2)
            start = std::max(start, std::max(caption.right, mode.right) + 4);
        for (int x = start; x < r.w; ++x) {
            int col = x - (r.w - art::bandWidth);
            auto index = print[y * art::bandWidth + col];
            if (!index)
                continue;
            Ink under = line[x];
            if (under != black && under != panel && under != halftoneInk && under != rule && under != violet)
                continue;
            float open =
                std::min({caption.exposure(x, plotY), mode.exposure(x, plotY), sublabel.exposure(x, plotY)});
            float feather = unit((col + 1) / 6.f);
            int weight = int(std::lround(feather * (32.f + 224.f * open)));
            line[x] = mix(under, art::inks[index], weight);
        }
    }
}

void outputScope(Raster &r, const Model &m) {
    r.box(0, 0, r.w, r.h, panel);
    r.text(5, 6, "OUT", muted);
    constexpr int left = 32, baseline = 11, samples = 78;
    r.box(left, baseline, samples, 1, rule);
    for (int x = 0; x < samples; x += 12)
        r.dot(left + x, baseline + 2, rule);
    if (!sounding(m))
        return;
    int last = baseline;
    for (int x = 0; x < samples; ++x) {
        // The host can report audio above full scale. Keep it inside the scope.
        float sample = m.scope[x * 127 / (samples - 1)];
        sample = std::isfinite(sample) ? std::clamp(sample, -1.f, 1.f) : 0.f;
        int y = baseline - int(std::lround(7 * sample));
        if (x)
            r.line(left + x - 1, last, left + x, y, cyan);
        last = y;
    }
}

void listRow(Raster &r, const Row *row, bool focus, bool edit, int rail) {
    r.box(0, 0, r.w, r.h, black);
    if (!row)
        return;
    r.box(5, 0, r.w - 5, 14, focus ? selected : panel);
    if (focus) {
        for (int k = 0; k < 4; ++k)
            r.box(5, k, 4 - k, 1, black);
        r.box(7, 13, 8, 1, orchid);
    }
    r.text(12, 3, row->name, focus ? paper : valueInk, sizeof row->name);
    int n = textWidth(row->text, sizeof row->text);
    r.text(147 - n, 3, row->text, focus ? lime : paper, sizeof row->text);
    bar(r, 154, 6, 39, *row, focus ? lime : muted);
    if (edit && focus) {
        r.box(6, 1, 4, 1, lime);
        r.box(6, 1, 1, 12, lime);
        r.box(6, 12, 4, 1, lime);
        r.box(r.w - 2, 1, 2, 1, lime);
        r.box(r.w - 2, 12, 2, 1, lime);
    }
    r.box(2, rail, 2, 10, lime);
}
void knobCard(Raster &r, const Row *row, int k, const Host &h) {
    r.box(0, 0, r.w, r.h, black);
    // Four dark violet inks keep the footer legible on the 565 panel.
    constexpr Ink bands[] = {0xff30203d, 0xff2b1b37, 0xff281930, 0xff221429};
    for (int y = 0; y < r.h; ++y)
        r.box(0, y, r.w - std::max(0, 4 - y), 1, bands[y * 4 / r.h]);
    Ink track = h.accent | 0xff000000;
    r.box(0, 0, r.w - 4, 1, mix(card, track, 130));
    r.box(0, 0, 12, 2, track);
    char key[] = "K1";
    key[1] += k;
    if (h.shift) {
        constexpr const char *lines[][2] = {
            {"WHEEL ROW", "PUSH EDIT"}, {"-/+ GROUP", "EDIT STEP"}, {"K1-K3", "TURN ROWS"}};
        r.text(6, 7, lines[k][0], paper);
        r.text(6, 22, lines[k][1], muted);
        return;
    }
    // Track ink identifies the physical knob even with very dark track colours.
    Ink lozenge = mix(track, paper, 145);
    for (int y = 0; y < 11; ++y)
        r.box(2 + (10 - y) / 4, 3 + y, 19, 1, lozenge);
    r.text(5, 4, key, black);
    if (!row) {
        r.text(25, 4, "-", muted);
        return;
    }
    r.text(25, 4, row->name, paper, sizeof row->name);
    r.text(5, 20, row->text, valueInk, sizeof row->text);
    int x = std::max(58, 9 + textWidth(row->text, sizeof row->text)), w = r.w - x - 7, y = 26;
    bar(r, x, y, w, *row, mix(track, paper, 80));
    if (w >= 3 && h.pot[k] >= 0) {
        int p = index(h.pot[k], 127) * (w - 1) / 127, target = position(row->value, w - 1);
        if (h.engaged[k])
            r.box(x + p - 1, y - 4, 3, 3, paper);
        else {
            r.frame(x + p - 2, y - 6, 5, 5, paper);
            if (p != target) {
                int dir = p < target ? 1 : -1;
                r.dot(x + p + 3 * dir, y - 4, paper);
                r.dot(x + p + 4 * dir, y - 4, paper);
            }
        }
    }
}
void foot(Raster &r) {
    r.box(0, 0, r.w, r.h, black);
    for (int x = -64; x < 390; x += 20)
        r.line(160 + (x - 160) * 9 / 10, 0, x, 5, violet);
    r.box(0, 3, r.w, 1, violet);
}
}

void Screen::knobs(const Model &m, uint32_t ids[3]) const {
    int pos = boundedCursor(cursor_, m);
    for (int k = 0; k < 3; ++k)
        ids[k] = pos + k < count(m) ? m.rows[pos + k].id : none;
}
void Screen::syncControls(const Model &m) {
    cursor_ = boundedCursor(cursor_, m);
    const int machine = machineId(m), slot = slotId(m), drum = slotKind(m.kitMachines[slot]);
    if (editing_ && count(m)) {
        const Row &row = m.rows[cursor_];
        const bool switched = machine != controlMachine_;
        const bool slotChanged = machine == int(Kind::Kit) && slot != controlSlot_;
        const bool drumChanged = machine == int(Kind::Kit) && !slotChanged && drum != controlDrum_;
        if (switched || drumChanged || (slotChanged && row.group == Group::Slot)) {
            Role selector = switched ? Role::Machine : slotChanged ? Role::KitSlot : Role::KitMachine;
            // The host echoes the selected step in its next model, before either input or paint.
            editing_ = row.role == selector && row.id == pendingSelector_ && row.steps > 1 &&
                       position(row.value, row.steps - 1) == pendingStep_;
            pendingSelector_ = none;
        }
    }
    controlMachine_ = machine;
    controlSlot_ = slot;
    controlDrum_ = drum;
    if (!count(m))
        editing_ = false;
}
bool Screen::input(const Model &m, Key key, int delta, bool shift, Edit *out, int &n) {
    n = 0;
    syncControls(m);
    if (!count(m))
        return false;
    if (key == Key::Push) {
        editing_ = !editing_;
        pendingSelector_ = none;
        return true;
    }
    if (key != Key::Wheel && key != Key::Nudge)
        return false;
    const Row &row = m.rows[cursor_];
    if (editing_) {
        if (!delta || !out)
            return true;
        float old = unit(row.value), next;
        if (row.steps > 1) {
            int last = row.steps - 1;
            auto step = std::clamp<int64_t>(int64_t(position(old, last)) + delta, 0, last);
            next = float(step) / last;
        } else if (row.steps == 1)
            next = 0;
        else
            next = unit(old + delta / ((shift && key == Key::Wheel) ? 1024.f : 128.f));
        if (next != old) {
            out[0] = {row.id, next};
            n = 1;
            if (row.role == Role::Machine || row.role == Role::KitMachine || row.role == Role::KitSlot) {
                pendingSelector_ = row.id;
                pendingStep_ = position(next, row.steps - 1);
            }
        }
    } else if (key == Key::Wheel) {
        cursor_ = int(std::clamp<int64_t>(int64_t(cursor_) + delta, 0, count(m) - 1));
    } else if (delta) {
        const int previousCursor = cursor_;
        int first = cursor_;
        while (first && m.rows[first - 1].group == row.group)
            --first;
        if (delta < 0 && first > 0) {
            --first;
            while (first && m.rows[first - 1].group == m.rows[first].group)
                --first;
            cursor_ = first;
        } else if (delta > 0) {
            int next = cursor_ + 1;
            while (next < count(m) && m.rows[next].group == row.group)
                ++next;
            if (next < count(m))
                cursor_ = next;
        }
        if (cursor_ != previousCursor)
            top_ = std::max(0, cursor_ - 1);
    }
    return true;
}
bool Screen::animating(const Model &m, const Host &) const {
    return sounding(m) || sweepFrame_ < 16 || glideY_ != glideTarget_;
}
bool Screen::paint(const Model &m, const Host &h, Ink *pixels, int stride, bool whole) {
    if (!pixels || stride < width)
        return false;
    const bool full = whole || !seen_;
    const int machine = machineId(m), slot = slotId(m), drum = slotKind(m.kitMachines[slot]);
    const bool switched = seen_ && machine != machineId(previous_);
    syncControls(m);
    const bool bell = cowbell(m, machine == int(Kind::Kit) ? drum : machine);
    // A switch wipes from the old heading to the new one. One that arrives while a wipe still runs (the machine knob
    // turned on through several machines) shows the new heading at once: restarting the wipe each time would keep
    // the old, half-wiped titles on screen until the knob stops.
    const bool snap = switched && sweepFrame_ < 16;
    if (switched) {
        fromMachine_ = machineId(previous_);
        fromSlot_ = slotId(previous_);
        fromKitMachine_ = slotKind(previous_.kitMachines[fromSlot_]);
        fromBell_ = lastBell_;
        sweepStart_ = h.nowMs;
    }
    int phase = snap ? 16
                : switched || sweepFrame_ < 16
                    ? int(std::min<uint64_t>(sweepMs, elapsed(h.nowMs, sweepStart_)) * 16 / sweepMs)
                    : 16;
    // A group jump can leave spare rows below the last group. Keep that view while moving within it.
    const int maxTop = std::max(count(m) - rowsVisible, std::min(top_, count(m) - 1));
    if (cursor_ <= top_)
        top_ = cursor_ - 1;
    if (cursor_ >= top_ + rowsVisible - 1)
        top_ = cursor_ - rowsVisible + 2;
    top_ = index(top_, maxTop);
    int target = (cursor_ - top_) * rowH + 2;
    if (!seen_)
        glideY_ = glideTarget_ = target;
    if (target != glideTarget_) {
        glideFrom_ = glideY_;
        glideStart_ = h.nowMs;
        glideTarget_ = target;
    }
    int rail = target;
    if (glideY_ != target) {
        int t = int(std::min<uint64_t>(96, elapsed(h.nowMs, glideStart_)));
        rail = target + (glideFrom_ - target) * (96 - t) * (96 - t) / (96 * 96);
    }
    // Repaint a bounded tile, then copy just differing pixels. No transient erasures reach the panel.
    Ink scratch[contextW * (plotOffset + plotHeight)];
    bool changed = full;
    if (full)
        for (int y = 0; y < height; ++y)
            std::fill_n(pixels + y * stride, width, black);
    auto tile = [&](int x, int y, int w, int ht, auto paintTile) {
        Raster r(scratch, w, ht);
        paintTile(r);
        for (int j = 0; j < ht; ++j) {
            Ink *dst = pixels + (y + j) * stride + x;
            const Ink *src = scratch + j * w;
            if (full)
                std::copy_n(src, w, dst);
            else if (std::memcmp(dst, src, w * sizeof(Ink))) {
                changed = true;
                for (int i = 0; i < w; ++i)
                    if (dst[i] != src[i])
                        dst[i] = src[i];
            }
        }
    };
    if (full || switched || phase != sweepFrame_ || bell != lastBell_ ||
        (machine == 8 && (slot != slotId(previous_) || drum != previous_.kitMachines[slot])))
        tile(0, placeBarRows, headW, headH, [&](Raster &r) {
            // The new heading at once; the orchid edge still sweeps across it, but never over half of the old title (a
            // split such as "BASS|ND" read as a glitch).
            heading(r, machine, slot, drum, bell);
            if (phase < 16)
                r.box(headW * phase / 16, 2, 1, r.h - 4, orchid);
        });
    if (full || switched)
        tile(contextX, placeBarRows, contextW, 20, [&](Raster &r) {
            r.box(0, 0, r.w, r.h, black);
            char number[8];
            std::snprintf(number, sizeof number, "%02d/%02d", machine + 1, machineCount);
            r.text(78, 4, number, paper);
        });
    bool meterDirty = full || switched || h.playing != previousHost_.playing || slot != slotId(previous_);
    for (int i = 0; i < 8; ++i)
        meterDirty |= position(m.voices[i], 4) != position(previous_.voices[i], 4);
    if (meterDirty)
        tile(contextX, placeBarRows + 20, contextW, headH - 20, [&](Raster &r) { meters(r, m, h); });
    auto group = count(m) ? m.rows[cursor_].group : Group::Machine;
    auto oldGroup = lastCursor_ >= 0 && lastCursor_ < count(previous_) ? previous_.rows[lastCursor_].group
                                                                       : Group::Machine;
    if (full || switched || group != oldGroup)
        tile(0, bannerY, contextX, 13, [&](Raster &r) { tabs(r, machine, group, 0); });
    for (int j = 0; j < rowsVisible; ++j) {
        int i = top_ + j, old = lastTop_ + j, y = j * rowH;
        const Row *row = i < count(m) ? m.rows + i : nullptr;
        bool focus = i == cursor_, wasFocus = old == lastCursor_;
        bool railDirty = rail != glideY_ &&
                         ((rail < y + rowH && rail + 10 > y) || (glideY_ < y + rowH && glideY_ + 10 > y));
        bool dirty = full || top_ != lastTop_ || (row != nullptr) != (old < count(previous_)) ||
                     (row && rowDiff(*row, previous_.rows[i])) || focus != wasFocus ||
                     (focus && editing_ != lastEditing_) || railDirty;
        if (dirty)
            tile(0, listY + y, listW, rowH, [&](Raster &r) { listRow(r, row, focus, editing_, rail - y); });
    }
    bool plotDirty = full || switched || group != oldGroup || count(m) != count(previous_) ||
                     (machine == 8 && (slot != slotId(previous_) || drum != previous_.kitMachines[slot]));
    for (int i = 0; !plotDirty && i < count(m); ++i)
        if (m.rows[i].role != previous_.rows[i].role || m.rows[i].data != previous_.rows[i].data ||
            m.rows[i].value != previous_.rows[i].value ||
            (m.rows[i].role == Role::AmpDecay &&
             std::strncmp(m.rows[i].text, previous_.rows[i].text, sizeof m.rows[i].text)))
            plotDirty = true;
    if ((group == Group::Machine || group == Group::Slot) && machine == int(Kind::Gendy) &&
        std::memcmp(m.scope, previous_.scope, sizeof m.scope))
        plotDirty = true;
    if (plotDirty)
        tile(contextX, bannerY, contextW, plotOffset + plotHeight, [&](Raster &r) { diagram(r, m, group); });
    bool live = sounding(m);
    if (full || live != sounding(previous_) ||
        (live && std::memcmp(m.scope, previous_.scope, sizeof m.scope)))
        tile(contextX, listY + plotHeight, contextW, 23, [&](Raster &r) { outputScope(r, m); });
    for (int k = 0; k < 3; ++k) {
        int i = cursor_ + k;
        const Row *row = i < count(m) ? m.rows + i : nullptr;
        if (full || lastCursor_ != cursor_ || h.shift != previousHost_.shift ||
            h.accent != previousHost_.accent || h.pot[k] != previousHost_.pot[k] ||
            h.engaged[k] != previousHost_.engaged[k] || (row != nullptr) != (i < count(previous_)) ||
            (row && rowDiff(*row, previous_.rows[i])))
            tile(4 + 106 * k, cardY, 100, 33, [&](Raster &r) { knobCard(r, row, k, h); });
    }
    if (full)
        tile(0, 210, width, 5, foot);
    lastBell_ = bell;
    previous_ = m;
    previousHost_ = h;
    lastCursor_ = cursor_;
    lastTop_ = top_;
    lastEditing_ = editing_;
    sweepFrame_ = phase;
    glideY_ = rail;
    seen_ = true;
    return changed;
}
}
