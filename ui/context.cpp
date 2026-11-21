// Copyright (C) 2026 hdslave
#include "raster.h"
#include "art.h"
#include "voice_plots.h"

namespace machine::ui::draw {
void emblem(Raster &r, int x, int y, int machine) {
    for (int j = 0; j < art::size; ++j)
        for (int i = 0; i < art::size; ++i) {
            auto ink = art::icons[machine][j * art::size + i];
            if (ink)
                r.dot(x + i, y + j, art::inks[ink]);
        }
}
namespace {
struct Patch {
    bool infiniteDecay = false;
    float data[8]{};
    float env[2][4]{};
    float controls[int(Role::LfoDest) + 1]{};
    explicit Patch(const Model &model) {
        for (int i = 0; i < count(model); ++i) {
            auto &p = model.rows[i];
            if (p.role == Role::AmpDecay)
                infiniteDecay = std::strncmp(p.text, "INF", sizeof p.text) == 0;
            if (p.role == Role::Data && p.data < 8)
                data[p.data] = unit(p.value);
            else if (p.role >= Role::FmAttack && p.role <= Role::FmLevel && p.data < 2)
                env[p.data][int(p.role) - int(Role::FmAttack)] = unit(p.value);
            else if (int(p.role) <= int(Role::LfoDest))
                controls[int(p.role)] = unit(p.value);
        }
    }
    float get(Role role) const {
        return controls[int(role)];
    }
};
struct Point {
    int x, y;
};
struct Route {
    Point nodes[4];
    unsigned inputs[4];
    unsigned carriers;
};
// Nodes in each diagram are C, A, B1, B2. Each destination has a source bit mask.
constexpr Route routes[] = {{{{55, 78}, {55, 59}, {55, 42}, {55, 24}}, {2, 4, 8, 0}, 1},
                            {{{55, 75}, {55, 50}, {30, 23}, {81, 23}}, {2, 12, 0, 0}, 1},
                            {{{55, 75}, {28, 48}, {82, 48}, {82, 22}}, {6, 0, 8, 0}, 1},
                            {{{28, 70}, {28, 31}, {81, 70}, {81, 31}}, {2, 0, 8, 0}, 5},
                            {{{20, 68}, {55, 68}, {90, 68}, {55, 27}}, {8, 8, 8, 0}, 7},
                            {{{20, 69}, {55, 69}, {90, 69}, {90, 29}}, {0, 0, 8, 0}, 7},
                            {{{55, 72}, {20, 26}, {55, 26}, {90, 26}}, {14, 0, 0, 0}, 1},
                            {{{18, 54}, {45, 54}, {72, 54}, {98, 54}}, {0, 0, 0, 0}, 15}};
void algorithm(Raster &r, int choice) {
    auto &route = routes[choice];
    r.text(6, 3, "ALGORITHM", muted);
    char num[] = "01";
    num[1] += choice;
    r.text(96, 3, num, orchid);
    constexpr const char *labels[] = {"C", "A", "B1", "B2"};
    for (int to = 0; to < 4; ++to) {
        auto b = route.nodes[to];
        for (int from = 0; from < 4; ++from)
            if (route.inputs[to] & (1u << from)) {
                auto a = route.nodes[from];
                r.line(a.x, a.y + 6, b.x, b.y - 9, orchid);
                r.line(b.x - 2, b.y - 10, b.x, b.y - 8, orchid);
                r.line(b.x + 2, b.y - 10, b.x, b.y - 8, orchid);
            }
        if (route.carriers & (1u << to)) {
            r.line(b.x, b.y + 6, b.x, 89, cyan);
            r.line(b.x, 89, 55, 89, cyan);
        }
    }
    r.line(55, 89, 55, 94, cyan);
    r.dot(54, 93, cyan);
    r.dot(56, 93, cyan);
    auto fb = route.nodes[3];
    r.line(fb.x + 10, fb.y + 3, fb.x + 15, fb.y + 3, orchid);
    r.line(fb.x + 15, fb.y + 3, fb.x + 15, fb.y - 7, orchid);
    r.line(fb.x + 15, fb.y - 7, fb.x + 6, fb.y - 7, orchid);
    r.dot(fb.x + 6, fb.y - 6, orchid);
    for (int i = 0; i < 4; ++i) {
        auto p = route.nodes[i];
        bool carrier = route.carriers & (1u << i);
        r.box(p.x - 10, p.y - 6, 21, 13, carrier ? 0xff294048 : card);
        r.frame(p.x - 10, p.y - 6, 21, 13, carrier ? cyan : rule);
        r.text(p.x - textWidth(labels[i], 2) / 2, p.y - 4, labels[i], carrier ? paper : valueInk);
    }
}
void axes(Raster &r) {
    for (int y = 28; y < 90; y += 16)
        for (int x = 9; x <= 108; x += 8)
            r.dot(x, y, rule);
    r.box(7, 87, 104, 1, rule);
    for (int x = 7; x < 111; x += 16)
        r.box(x, 86, 1, 3, muted);
}
template <class F> void curve(Raster &r, F sample, Ink color, int span = 61) {
    int prev = 0;
    for (int i = 0; i < 102; ++i) {
        int y = 87 - position(sample(i / 101.f), span);
        if (i)
            r.line(6 + i, prev, 7 + i, y, color);
        prev = y;
    }
}
float wave(int mode, float t) {
    float x = t - std::floor(t);
    switch (mode) {
    case 0:
        return 1.f - 4.f * std::abs(x - 0.5f);
    case 1:
        return 2 * x - 1;
    case 2:
        return x < 0.5f ? 1.f : -1.f;
    case 3: {
        constexpr float hold[] = {.72f, -.4f, .2f, .93f, -.86f, .42f, -.2f, .63f};
        return hold[int(t * 3) % 8];
    }
    default:
        return std::sin(t * 6.2831853f);
    }
}
}
void context(Raster &r, const Model &m, Group group) {
    r.box(0, 0, r.w, r.h, panel);
    for (int y = 19; y < r.h; y += 4)
        r.box(0, y, r.w, 1, 0xff191020);
    // A two-ink legend strip and a sparse halftone field frame the diagram.
    for (int y = 0; y < 13; ++y)
        r.box(0, y, r.w - y / 3, 1, 0xff453052);
    for (int y = 18; y < r.h - 2; y += 3)
        for (int x = 0; x < r.w; x += 4) {
            int edge = r.w - 13 - (y - 18) / 4;
            if (x >= edge)
                r.dot(x + ((y / 3) & 1), y, 0xff412445);
        }
    Patch p(m);
    if (group == Group::Filter) {
        int mode = position(p.get(Role::FilterType), 2);
        constexpr const char *types[] = {"LP", "HP", "BP"};
        r.text(6, 3, "FILTER", muted);
        r.text(95, 3, types[mode], orchid);
        axes(r);
        float cutoff = .05f + .90f * p.get(Role::FilterFreq), res = p.get(Role::FilterReso);
        curve(
            r,
            [&](float x) {
                float d = (x - cutoff) * 8, low = .5f - d / (2 * (1 + std::abs(d)));
                float peak = 1.f / (1.f + d * d * 6);
                return .08f + (mode == 2 ? (.52f + .32f * res) * peak
                                         : .70f * (mode == 0 ? low : 1 - low) + res * .34f * peak);
            },
            cyan);
    } else if (group == Group::FmEnv) {
        r.text(6, 3, "MOD ENV", muted);
        for (int op = 0; op < 2; ++op) {
            auto *v = p.env[op];
            int y = 48 + op * 39;
            Ink ink = op ? orchid : cyan;
            r.text(6, y - 20, op ? "B" : "A", ink);
            r.box(19, y, 91, 1, rule);
            int attack = 21 + position(v[0], 38), decay = attack + 2 + position(v[1], 38);
            int peak = y - position(v[3], 27), sustain = y - position(v[3] * v[2], 27);
            r.line(19, y, attack, peak, ink);
            r.line(attack, peak, decay, sustain, ink);
            r.line(decay, sustain, 110, sustain, ink);
            r.box(attack - 1, peak - 1, 3, 3, paper);
        }
    } else if (group == Group::Amp) {
        r.text(6, 3, "ENVELOPE", muted);
        axes(r);
        float a = 2 + 28 * p.get(Role::AmpAttack), h = 2 + 28 * p.get(Role::AmpHold),
              d = 2 + 28 * p.get(Role::AmpDecay), rel = 2 + 28 * p.get(Role::AmpRelease);
        float scale = std::min(1.f, 98 / (a + h + d + rel));
        int x = 8;
        Point nodes[] = {{x, 83},
                         {x += int(a * scale), 28},
                         {x += int(h * scale), 28},
                         {x += int(d * scale), p.infiniteDecay ? 28 : 64},
                         {x += int(rel * scale), 83}};
        for (int i = 1; i < 5; ++i)
            r.line(nodes[i - 1].x, nodes[i - 1].y, nodes[i].x, nodes[i].y, cyan);
        for (auto pt : nodes)
            r.box(pt.x - 1, pt.y - 1, 3, 3, orchid);
    } else if (group == Group::Lfo) {
        const int mode = position(p.get(Role::LfoWave), 4);
        constexpr const char *waves[] = {"TRI", "SAW", "SQR", "S+H", "SINE"};
        r.text(6, 3, "LFO", muted);
        r.text(111 - textWidth(waves[mode], 4), 3, waves[mode], orchid);
        axes(r);
        float speed = .5f + 4.5f * p.get(Role::LfoSpeed), depth = p.get(Role::LfoDepth) * 2 - 1;
        curve(r, [&](float x) { return .5f + .46f * depth * wave(mode, x * speed); }, cyan);
    } else {
        int id = machineId(m);
        if (id == 8)
            id = slotKind(m.kitMachines[slotId(m)]);
        if (id == 0) {
            algorithm(r, position(p.data[0], 7));
            return;
        }
        if (id >= int(Kind::Terrain)) {
            voices::panel(r, id, p.data, m);
            return;
        }
        // The sound portrait and its live parameter diagram have distinct plates.
        bool plotted = id == 1 || id == 3 || id == 4;
        int originY = plotted ? 18 : 42;
        for (int y = 0; y < art::portraitH; ++y)
            for (int x = 0; x < art::portraitW; ++x) {
                uint8_t ink = art::portraits[id - 1][y * art::portraitW + x];
                if (ink)
                    r.dot(x + (r.w - art::portraitW) / 2, y + originY, art::inks[ink]);
            }
        int cornerTop = plotted ? 17 : 32;
        r.box(5, cornerTop, 6, 1, rule);
        r.box(5, cornerTop, 1, 5, rule);
        r.box(107, plotted ? 62 : 94, 6, 1, rule);
        r.box(112, plotted ? 58 : 90, 1, 5, rule);
        if (plotted) {
            r.box(6, 88, 106, 1, rule);
            for (int x = 8; x < 111; x += 16)
                r.dot(x, 90, muted);
        }
        if (id == 1) {
            r.text(6, 3, "PITCH SWEEP", muted);
            curve(
                r,
                [&](float x) {
                    return (.2f + .7f * p.data[2]) * std::exp(-x * (3 + 9 * (1 - p.data[3]))) +
                           .08f * p.data[0];
                },
                orchid, 22);
            curve(r, [&](float x) { return .42f * std::exp(-x * (2 + 8 * (1 - p.data[1]))); }, cyan, 22);
        } else if (id == 3) {
            r.text(6, 3, "BURSTS", muted);
            const int bursts = 1 + position(p.data[0], 7);
            curve(
                r,
                [&](float x) {
                    float phase = x / (.06f + .065f * p.data[1]);
                    return phase < bursts
                               ? .9f * std::exp(-(phase - std::floor(phase)) * (3 + 7 * (1 - p.data[2])))
                               : 0.f;
                },
                cyan, 22);
        } else if (id == 4) {
            r.text(6, 3, "METAL BANK", muted);
            for (int i = 0; i < 6; ++i) {
                int x = 11 + i * 15 + position(p.data[1], 7);
                int amp = 5 + int((i % 3 + 1) * (2 + 3 * p.data[4])) + position(p.data[2], 4);
                r.line(x, 86, x + 2, 86 - amp, cyan);
                r.line(x + 2, 86 - amp, x + 6, 86, orchid);
            }
        } else {
            r.text(6, 3, machineNames[id], muted);
        }
    }
}
}
