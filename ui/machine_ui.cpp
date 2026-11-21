// Copyright (C) 2026 hdslave
#include "raster.h"
#include "art.h"
#include <cstdio>

namespace machine::ui {
using namespace draw;
namespace {
constexpr int listY = 52, rowH = 15, rowsVisible = 8, listW = 196, contextX = 202, contextW = 114;
constexpr int headW = listW, headH = 35, cardY = 176;
constexpr uint32_t none = 0xffffffffu;
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
void heading(Raster &r, int machine, int slot, int drum) {
    r.box(0, 0, r.w, r.h, black);
    char title[24]{};
    if (machine == 8)
        std::snprintf(title, sizeof title, "KIT %c %d %s", 127, slot + 1, machineNames[drum]);
    else
        std::snprintf(title, sizeof title, "%s", machineNames[machine]);
    bool stacked = machine == 8 && textWidth(title, 24, true) > headW - 46;
    const char *big = stacked ? machineNames[drum] : title;
    int ty = stacked ? 15 : 9;
    emblem(r, 4, 2, machine);
    if (stacked) {
        char slotName[] = "KIT / 1";
        slotName[6] = char('1' + slot);
        r.text(42, 1, slotName, muted);
    }
    r.text(41, ty, big, 0xff3b909e, 24, true);
    r.text(43, ty, big, 0xffa5529e, 24, true);
    r.text(42, ty, big, paper, 24, true);
    r.box(0, r.h - 1, r.w, 1, violet);
}
void meters(Raster &r, const Model &m, const Host &h) {
    r.box(0, 0, r.w, r.h, black);
    constexpr int markX = 2;
    for (int y = 0; y < art::bandHeight; ++y)
        for (int x = 0; x < art::bandWidth; ++x) {
            auto ink = art::band[y * art::bandWidth + x];
            if (ink)
                r.dot(markX + x, y, art::inks[ink]);
        }
    char number[8];
    std::snprintf(number, sizeof number, "%02u/%02u", unsigned(machineId(m) + 1), unsigned(machineCount));
    r.text(r.w - 36, 7, number, paper);
    r.text(2, 24, machineId(m) == 8 ? "SLOTS" : "VOICE", muted);
    if (h.playing)
        r.box(40, 26, 2, 2, cyan);
    for (int k = 0; k < 8; ++k) {
        int x = 48 + k * 8, level = position(m.voices[k], 4);
        for (int i = 0; i < 4; ++i)
            r.box(x, 30 - i * 2, 5, 1, i < level ? cyan : rule);
        if (machineId(m) == 8 && k == slotId(m))
            r.box(x, 32, 5, 1, paper);
    }
    r.box(0, r.h - 1, r.w, 1, violet);
}
void tabs(Raster &r, int machine, Group active) {
    r.box(0, 0, r.w, r.h, black);
    int x = 4;
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
bool Screen::input(const Model &m, Key key, int delta, bool shift, Edit *out, int &n) {
    n = 0;
    if (controlMachine_ != machineId(m)) {
        editing_ = false;
        controlMachine_ = machineId(m);
    }
    cursor_ = boundedCursor(cursor_, m);
    if (!count(m)) {
        editing_ = false;
        return false;
    }
    if (key == Key::Push) {
        editing_ = !editing_;
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
        }
    } else if (key == Key::Wheel) {
        cursor_ = int(std::clamp<int64_t>(int64_t(cursor_) + delta, 0, count(m) - 1));
    } else if (delta) {
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
    if (controlMachine_ != machine) {
        editing_ = false;
        controlMachine_ = machine;
    }
    if (count(m) == 0)
        editing_ = false;
    if (switched) {
        fromMachine_ = machineId(previous_);
        fromSlot_ = slotId(previous_);
        fromKitMachine_ = slotKind(previous_.kitMachines[fromSlot_]);
        sweepStart_ = h.nowMs;
    }
    int phase = switched || sweepFrame_ < 16
                    ? int(std::min<uint64_t>(250, elapsed(h.nowMs, sweepStart_)) * 16 / 250)
                    : 16;
    cursor_ = boundedCursor(cursor_, m);
    if (cursor_ <= top_)
        top_ = cursor_ - 1;
    if (cursor_ >= top_ + rowsVisible - 1)
        top_ = cursor_ - rowsVisible + 2;
    top_ = index(top_, count(m) - rowsVisible);
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
    Ink scratch[contextW * 97]; // The largest tile is the context plot.
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
    if (full || switched || phase != sweepFrame_ ||
        (machine == 8 && (slot != slotId(previous_) || drum != previous_.kitMachines[slot])))
        tile(0, 2, headW, headH, [&](Raster &r) {
            heading(r, machine, slot, drum);
            if (phase < 16) {
                int edge = headW * phase / 16;
                r.left = edge;
                heading(r, fromMachine_, fromSlot_, fromKitMachine_);
                r.box(edge, 2, 1, r.h - 4, orchid);
            }
        });
    bool meterDirty = full || switched || h.playing != previousHost_.playing || slot != slotId(previous_);
    for (int i = 0; i < 8; ++i)
        meterDirty |= position(m.voices[i], 4) != position(previous_.voices[i], 4);
    if (meterDirty)
        tile(contextX, 2, contextW, headH, [&](Raster &r) { meters(r, m, h); });
    auto group = count(m) ? m.rows[cursor_].group : Group::Machine;
    auto oldGroup = lastCursor_ >= 0 && lastCursor_ < count(previous_) ? previous_.rows[lastCursor_].group
                                                                       : Group::Machine;
    if (full || switched || group != oldGroup)
        tile(0, 38, width, 13, [&](Raster &r) { tabs(r, machine, group); });
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
        tile(contextX, listY, contextW, 97, [&](Raster &r) { context(r, m, group); });
    bool live = sounding(m);
    if (full || live != sounding(previous_) ||
        (live && std::memcmp(m.scope, previous_.scope, sizeof m.scope)))
        tile(contextX, listY + 97, contextW, 23, [&](Raster &r) {
            r.box(0, 0, r.w, r.h, panel);
            if (!live)
                return;
            r.box(5, 11, r.w - 10, 1, violet);
            int last = 11;
            for (int x = 0; x < r.w - 10; ++x) {
                float sample = m.scope[x * 127 / (r.w - 11)];
                int v = 11 - (position((sample + 1.f) * 0.5f, 18) - 9);
                if (x)
                    r.line(x + 4, last, x + 5, v, cyan);
                last = v;
            }
        });
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
