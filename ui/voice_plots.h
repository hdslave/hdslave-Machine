// Copyright (C) 2026 hdslave
#pragma once
#include "raster.h"

namespace machine::ui::draw {
namespace voices {
constexpr float tau = 6.28318530718f;
struct Vertex {
    int x, y;
};
inline void join(Raster &r, Vertex a, Vertex b, Ink ink) {
    r.line(a.x, a.y, b.x, b.y, ink);
}

float elevation(int surface, float x, float y, float warp) {
    const float k = 2.f + 3.f * warp;
    switch (surface) {
    case 0:
        return std::cos(k * 2.f * std::sqrt(x * x + y * y));
    case 1:
        return x * x - y * y;
    case 2:
        return std::cos(k * x) * std::cos(k * y);
    case 3:
        return std::sin(k * (x + .4f * y * y));
    case 4:
        return .5f * (std::sin(k * x + 1.6f * y) + std::cos(k * y - 1.6f * x));
    default: {
        float radius = std::sqrt(x * x + y * y);
        return std::exp(-18.f * (radius - .55f) * (radius - .55f)) - std::exp(-5.f * radius * radius);
    }
    }
}
void terrain(Raster &r, const float *v) {
    const int surface = position(v[0], 5);
    auto project = [&](float x, float y) {
        return Vertex{int(std::lround(57 + x * 38 - y * 13)),
                      int(std::lround(56 + x * 3 + y * 19 - 12 * elevation(surface, x, y, v[5])))};
    };
    // A shallow axonometric surface leaves the path readable at panel resolution.
    for (int row = 0; row <= 8; ++row) {
        float axis = row * .25f - 1.f;
        auto across = project(-1, axis), down = project(axis, -1);
        for (int i = 1; i <= 32; ++i) {
            float t = i / 16.f - 1.f;
            auto a = project(t, axis), b = project(axis, t);
            join(r, down, b, rule);
            join(r, across, a, row % 2 ? muted : orchid);
            across = a;
            down = b;
        }
    }
    float rx = .1f + .56f * v[3], ry = .08f + .55f * v[4];
    float cx = (v[1] * 2 - 1) * (1 - rx), cy = (v[2] * 2 - 1) * (1 - ry);
    Vertex prev = project(cx + rx, cy), start = prev;
    for (int i = 1; i <= 48; ++i) {
        float angle = i * tau / 48;
        auto next = project(cx + rx * std::cos(angle), cy + ry * std::sin(angle));
        join(r, prev, next, cyan);
        prev = next;
    }
    r.box(start.x - 1, start.y - 1, 3, 3, lime);
}
void phase(Raster &r, const float *v) {
    float d = .01f + .98f * v[0], peak = 6 * v[1];
    int bend = 7 + position(d, 99);
    // Vertical range includes both the bend and the end of one phase cycle.
    float range = std::max(1.f, peak);
    int mid = 49 - int(std::lround(26 * peak / range));
    r.line(7, 49, bend, mid, orchid);
    r.line(bend, mid, 106, 49 - int(26 / range), orchid);
    r.box(bend - 1, mid - 1, 3, 3, paper);
    r.box(6, 74, 103, 1, rule);
    int prev = 0;
    for (int x = 0; x <= 100; ++x) {
        float t = x / 100.f, phi = t < d ? t / d * peak : peak + (1 - peak) * (t - d) / (1 - d);
        int y = 74 - int(std::lround(14 * std::cos(tau * phi)));
        if (x)
            r.line(6 + x, prev, 7 + x, y, cyan);
        prev = y;
    }
}
void vox(Raster &r, const float *v) {
    constexpr int first[] = {800, 500, 300, 500, 350}, second[] = {1150, 1800, 2250, 850, 650};
    float a = v[1] * 4;
    int i = int(a), j = std::min(4, i + 1);
    float shift = std::exp2(2 * v[2] - 1);
    for (int band = 0; band < 2; ++band) {
        auto *f = band ? second : first;
        int x = 8 + position((f[i] + (f[j] - f[i]) * (a - i)) * shift / 4800.f, 99);
        r.box(x, 24, 1, 13, band ? orchid : cyan);
        r.box(x - 1, 23, 3, 2, band ? orchid : cyan);
    }
    r.box(7, 38, 101, 1, rule);
    int pulses = 1 + position(v[3], 7), prev = 85;
    for (int x = 0; x <= 100; ++x) {
        float t = x / 100.f, phase = t / .78f * pulses;
        float s = std::sin(tau * .5f * phase);
        int y = t < .78f ? 85 - int(36 * s * s * std::exp(-(.8f + 3 * v[4]) * t)) : 85;
        if (x)
            r.line(x + 6, prev, x + 7, y, cyan);
        prev = y;
    }
}
void resonators(Raster &r, const float *v) {
    int strike = 8 + position(v[4], 97);
    r.box(8, 89, 99, 1, rule);
    r.box(strike - 1, 87, 3, 5, lime);
    for (int i = 0; i < 12; ++i) {
        float n = (i + 1.f) / 12, strikeGain = std::abs(std::sin((i + 1) * 3.141593f * (.02f + .96f * v[4])));
        float gain =
            (.16f + .84f * strikeGain) * std::exp(-n * (2.5f + 2 * v[3] - 2 * v[2])) * (1 - .25f * v[5] * n);
        int x = 9 + i * 8 + int(3 * v[1] * n * n), h = 3 + position(gain, 55);
        r.box(x, 78 - h, 3, h, rule);
        r.box(x, 78 - h, 3, 2, orchid);
        r.box(x, 78 - h + 2, 1, h - 2, cyan);
    }
}
void string(Raster &r, const float *v) {
    int pick = 9 + position(v[4], 94), rise = 4 + position(v[3], 23);
    r.box(7, 62, 99, 1, rule);
    r.line(7, 62, pick, 62 + rise / 2, rule);
    r.line(pick, 62 + rise / 2, 106, 62, rule);
    r.line(7, 62, pick, 62 - rise, cyan);
    r.line(pick, 62 - rise, 106, 62, cyan);
    r.box(6, 58, 2, 9, muted);
    r.box(106, 58, 2, 9, muted);
    r.box(pick - 1, 60 - rise, 3, 3, lime);
}
void breakpoints(Raster &r, const float *v, const Model &m) {
    int n = 3 + position(v[0], 13);
    r.box(7, 56, 101, 1, rule);
    Vertex prev{};
    for (int i = 0; i < n; ++i) {
        int sample = i * 127 / (n - 1);
        float normalized = (m.scope[sample] + 1) * .5f;
        Vertex p = {7 + i * 100 / (n - 1), 86 - position(normalized, 58)};
        if (i)
            join(r, prev, p, cyan);
        r.box(p.x - 1, p.y - 1, 3, 3, orchid);
        prev = p;
    }
}
void pulsaret(Raster &r, const float *v) {
    int prev = 52;
    for (int x = 0; x <= 100; ++x) {
        float t = x / 100.f, duration = .25f + .68f * v[2];
        float a = std::min(t / duration, 1.f);
        float s = std::sin(tau * a * std::exp2(4 * v[1]));
        float carrier = s + (s >= 0 ? 1 - s : -1 - s) * v[3];
        float envelope = std::pow(std::max(0.f, std::sin(3.141593f * a)), .25f + 2 * v[4]);
        int y = t < duration ? 52 - int(24 * carrier * envelope) : 52;
        if (x)
            r.line(x + 6, prev, x + 7, y, cyan);
        prev = y;
    }
    constexpr unsigned masks[] = {0xff, 0xaa, 0xdb, 0xee, 0xb6, 0x00, 0x00, 0x92};
    int mode = position(v[5], 7);
    for (int i = 0; i < 8; ++i) {
        int x = 10 + i * 12;
        r.frame(x, 83, 5, 5, muted);
        if (mode == 5 || mode == 6) { // Probability, shown with a partial fill.
            r.box(x + 1, mode == 5 ? 85 : 86, 3, mode == 5 ? 2 : 1, orchid);
        } else if (masks[mode] & (0x80 >> i))
            r.box(x + 1, 84, 3, 3, cyan);
    }
}
void drawbars(Raster &r, const float *v) {
    for (int i = 0; i < 8; ++i) {
        int x = 11 + i * 12, y = 26 + position(v[i], 8) * 7;
        r.box(x + 2, 24, 2, 61, rule);
        r.box(x + 2, 24, 2, y - 24, orchid);
        r.box(x - 1, y - 1, 8, 4, i < 3 ? paper : cyan);
    }
}
void panel(Raster &r, int id, const float *v, const Model &m) {
    switch (Kind(id)) {
    case Kind::Terrain:
        r.text(6, 3, "ORBIT MAP", muted);
        terrain(r, v);
        break;
    case Kind::Phase:
        r.text(6, 3, "PHASE BEND", muted);
        phase(r, v);
        break;
    case Kind::Vox:
        r.text(6, 3, "FORMANTS", muted);
        vox(r, v);
        break;
    case Kind::Modal:
        r.text(6, 3, "MODE BANK", muted);
        resonators(r, v);
        break;
    case Kind::Pluck:
        r.text(6, 3, "PICK POINT", muted);
        string(r, v);
        break;
    case Kind::Gendy:
        r.text(6, 3, "BREAKPOINTS", muted);
        breakpoints(r, v, m);
        break;
    case Kind::Pulsar:
        r.text(6, 3, "PULSARET", muted);
        pulsaret(r, v);
        break;
    case Kind::Organ:
        r.text(6, 3, "DRAWBARS", muted);
        drawbars(r, v);
        break;
    default:
        break;
    }
}
}
}
