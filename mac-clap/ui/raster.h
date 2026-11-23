// Copyright (C) 2026 hdslave
#pragma once
#include "machine_ui.h"
#include "kinds.h"
#include "fonts.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>

namespace machine::ui::draw {
using Ink = uint32_t;
constexpr Ink halftoneInk = 0xff281b32;
constexpr Ink black = 0xff100c18, panel = 0xff171020, card = 0xff23152f, violet = 0xff382444,
              rule = 0xff51365f, muted = 0xffb4a1c4, paper = 0xfff4ebed, valueInk = 0xffd7c4ed,
              cyan = 0xff79dcd7, orchid = 0xffcd87e2, lime = 0xffcbef7b, selected = 0xff34283e;
inline float unit(float x) {
    return x >= 0.f ? std::min(x, 1.f) : 0.f;
}
inline int position(float x, int n) {
    return int(unit(x) * n + 0.5f);
}
inline int count(const Model &m) {
    return std::min<int>(m.rowCount, 40);
}
inline int index(int x, int last) {
    return std::clamp(x, 0, std::max(0, last));
}
inline int machineId(const Model &m) {
    return index(m.machine, machineCount - 1);
}
inline int slotId(const Model &m) {
    return index(m.kitSlot, 7);
}
inline const char *groups[] = {"MACHINE", "SLOT", "FILTER", "AMP", "LFO", "FM ENV"};
inline Ink mix(Ink a, Ink b, int weight) {
    Ink c = 0xff000000;
    for (int shift : {0, 8, 16}) {
        const auto av = (a >> shift) & 255, bv = (b >> shift) & 255;
        c |= ((av * (256 - weight) + bv * weight) >> 8) << shift;
    }
    return c;
}
// Draw in tile-local coordinates. The clip also supports a horizontal header reveal.
struct Raster {
    Ink *buf;
    int w, h, left = 0, right;
    Raster(Ink *p, int width, int height) : buf(p), w(width), h(height), right(width) {}
    void box(int x, int y, int width, int height, Ink c) {
        int x0 = std::max(x, left), x1 = std::min(x + width, right);
        if (x0 >= x1)
            return;
        for (int j = std::max(0, y); j < std::min(h, y + height); ++j)
            std::fill(buf + j * w + x0, buf + j * w + x1, c);
    }
    void dot(int x, int y, Ink c) {
        if (x >= left && x < right && y >= 0 && y < h)
            buf[y * w + x] = c;
    }
    void line(int x, int y, int u, int v, Ink c) {
        int dx = std::abs(u - x), dy = -std::abs(v - y), sx = x < u ? 1 : -1, sy = y < v ? 1 : -1,
            e = dx + dy;
        for (;;) {
            dot(x, y, c);
            if (x == u && y == v)
                break;
            int e2 = e * 2;
            if (e2 >= dy) {
                e += dy;
                x += sx;
            }
            if (e2 <= dx) {
                e += dx;
                y += sy;
            }
        }
    }
    void frame(int x, int y, int a, int b, Ink c) {
        box(x, y, a, 1, c);
        box(x, y + b - 1, a, 1, c);
        box(x, y, 1, b, c);
        box(x + a - 1, y, 1, b, c);
    }
    void text(int x, int y, const char *s, Ink ink, int limit = 24, bool big = false) {
        for (int i = 0; i < limit && s[i]; ++i) {
            unsigned ch = static_cast<unsigned char>(s[i]);
            int g = (ch >= 32 && ch <= 127 ? ch : '?') - 32;
            int advance = big ? font::titleWidth[g] : font::bodyWidth[g];
            const auto *glyph = big ? font::title[g] : font::body[g];
            for (int j = 0; j < (big ? 18 : 12); ++j) {
                uint32_t bits = glyph[j];
                for (int k = 0; bits; ++k, bits >>= 1)
                    if (bits & 1)
                        dot(x + k, y + j, ink);
            }
            x += advance;
        }
    }
};
inline int textWidth(const char *str, int limit, bool big = false) {
    int w = 0;
    for (int i = 0; i < limit && str[i]; ++i) {
        unsigned ch = static_cast<unsigned char>(str[i]);
        int g = (ch >= 32 && ch <= 127 ? ch : '?') - 32;
        w += big ? font::titleWidth[g] : font::bodyWidth[g];
    }
    return w;
}
inline bool sounding(const Model &m) {
    for (float v : m.voices)
        if (unit(v) > 0.f)
            return true;
    return false;
}
void context(Raster &, const Model &, Group);
void emblem(Raster &, int, int, int);
}
