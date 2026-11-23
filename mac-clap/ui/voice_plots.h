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
    auto sine = [](float cycles) { return std::sin(tau * cycles); };
    const float k = warp;
    switch (surface) {
    case 0:
        return sine(k * std::sqrt(x * x + y * y));
    case 1:
        return sine(.5f * k * (x * x - y * y));
    case 2:
        return sine(.5f * k * x) * sine(.5f * k * y + .25f);
    case 3:
        return sine(.5f * k * x + .3f * sine(.5f * k * y));
    case 4:
        return sine(.5f * k * x * y + .25f * x);
    default: {
        float r = x * x + y * y;
        return sine(.5f * k * r) / (1 + r);
    }
    }
}
void terrain(Raster &r, const float *v) {
    const int surface = position(v[0], 5);
    const float warp = .5f * std::pow(12.f, v[5]);
    auto project = [&](float x, float y) {
        return Vertex{int(std::lround(57 + x * 38 - y * 13)),
                      int(std::lround(56 + x * 3 + y * 19 - 12 * elevation(surface, x, y, warp)))};
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
    float d = .02f + .96f * v[0], peak = 6 * v[1];
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
        int y = 74 + int(std::lround(14 * std::cos(tau * phi)));
        if (x)
            r.line(6 + x, prev, 7 + x, y, cyan);
        prev = y;
    }
}
void vox(Raster &r, const float *v) {
    constexpr int first[] = {730, 530, 270, 570, 300}, second[] = {1090, 1840, 2290, 840, 870};
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
        int y = t < .78f ? 85 - int(36 * s * s * std::pow(1.f - .95f * v[4], std::floor(phase))) : 85;
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
        float t = x / 100.f;
        // Show one firing interval. FORMANT shortens the pulse and DENSITY spaces the firings.
        float q = t * std::exp2(4 * v[1] + 8 * v[2]);
        float phase = std::min(1.f, q), shape = 3 * v[3];
        const float waves[] = {std::sin(tau * phase), 1 - 2 * phase, phase < .5f ? 1.f : -1.f,
                               float((unsigned(x) * 1664525u + 1013904223u) >> 16 & 255) / 127.5f - 1};
        int i = std::min(2, int(shape));
        float carrier = waves[i] + (waves[i + 1] - waves[i]) * (shape - i);
        float falling = std::exp2(-7.2f * phase), bell = std::exp2(-36 * (phase - .5f) * (phase - .5f));
        float env = v[4] < .5f ? 1 + (falling - 1) * 2 * v[4] : falling + (bell - falling) * (2 * v[4] - 1);
        int y = q < 1 ? 52 - int(24 * carrier * env) : 52;
        if (x)
            r.line(x + 6, prev, x + 7, y, cyan);
        prev = y;
    }
    constexpr unsigned masks[] = {0xff, 0xaa, 0xdb, 0xee, 0xad, 0x00, 0x00, 0xa4};
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
void kick(Raster &r, const float *v) {
    // Upper trace follows the falling resonator pitch. Lower trace shows the amp contour.
    r.text(6, 19, "HZ", muted);
    r.text(6, 60, "AMP", muted);
    int base = 49 - position(v[0], 8), prevPitch = 0, prevAmp = 0;
    r.box(29, 50, 78, 1, rule);
    r.box(29, 85, 78, 1, rule);
    // Mark the initial excitation at the same time origin as the two response traces.
    int pulseHeight = position(v[6], 18), pulseWidth = 2 + position(v[5], 6);
    if (pulseHeight) {
        r.line(29, 50, 29, 50 - pulseHeight, lime);
        r.line(29, 50 - pulseHeight, 29 + pulseWidth, 50, lime);
    }
    for (int x = 0; x <= 77; ++x) {
        float t = x / 77.f;
        int py = base - int(23 * v[1] * std::exp(-t / (.01f + .36f * v[2])));
        float attack = .005f + .35f * v[4],
              env = t < attack ? t / attack : std::exp(-(t - attack) / (.025f + .5f * v[3]));
        int ay = 85 - int((14 + 10 * v[9]) * env);
        if (x) {
            r.line(28 + x, prevPitch, 29 + x, py, orchid);
            r.line(28 + x, prevAmp, 29 + x, ay, cyan);
        }
        prevPitch = py;
        prevAmp = ay;
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
    case Kind::Kick:
        r.text(6, 3, "RESONATOR", muted);
        kick(r, v);
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
