// Parameter descriptions for constructing synthetic preview and demo rows.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace fixture {
enum class Unit {
    choice,
    bipolar,
    unit,
    ratioX,
    rateHz,
    number,
    seconds,
    semis,
    vowel,
    decayInf,
    material,
    count,
    hz,
    octaves
};
struct Control {
    const char *name;
    Unit unit;
    float lo, hi, initial;
    int steps;
};
inline constexpr Control extendedControls[][10] = {
    {
        // TERRAIN
        {"SURFACE", Unit::choice, 0, 5, 0.4f, 6},
        {"X", Unit::bipolar, 0, 1, 0.62f, 0},
        {"Y", Unit::bipolar, 0, 1, 0.5f, 0},
        {"WIDTH", Unit::unit, 0, 1, 0.6f, 0},
        {"HEIGHT", Unit::unit, 0, 1, 0.4f, 0},
        {"WARP", Unit::ratioX, 0.5f, 6.0f, 0.442f, 0},
        {"SPIN", Unit::rateHz, 0, 8, 0.15f, 0},
        {"DRIFT", Unit::unit, 0, 1, 0.2f, 0},
    },
    {
        // PHASE
        {"D", Unit::unit, 0, 1, 0.5f, 0},
        {"V", Unit::number, 0, 6, 0.25f, 0},
        {"V ENV", Unit::bipolar, 0, 1, 0.62f, 0},
        {"ENV DEC", Unit::seconds, 0, 0, 0.5f, 0},
        {"TWIN", Unit::unit, 0, 1, 0.25f, 0},
        {"SUB", Unit::unit, 0, 1, 0.3f, 0},
        {"FOLD", Unit::unit, 0, 1, 0.0f, 0},
        {"GRIT", Unit::unit, 0, 1, 0.0f, 0},
    },
    {
        // VOX
        {"TUNE", Unit::semis, -24, 24, 0.5f, 49},
        {"VOWEL", Unit::vowel, 0, 1, 0.0f, 0},
        {"SHIFT", Unit::ratioX, 0.5f, 2.0f, 0.5f, 0},
        {"PULSES", Unit::count, 1, 8, 2.f / 7, 8},
        {"P DECAY", Unit::unit, 0, 1, 0.5f, 0},
        {"F ENV", Unit::bipolar, 0, 1, 0.5f, 0},
        {"F DEC", Unit::seconds, 0, 0, 0.5f, 0},
        {"LENGTH", Unit::decayInf, 0, 0, 1.0f, 0},
    },
    {
        // MODAL
        {"TUNE", Unit::semis, -24, 24, 0.5f, 49},
        {"MATERIAL", Unit::material, 0, 1, 0.33f, 0},
        {"BRIGHT", Unit::unit, 0, 1, 0.5f, 0},
        {"DAMP", Unit::unit, 0, 1, 0.45f, 0},
        {"POSITION", Unit::unit, 0, 1, 0.3f, 0},
        {"MALLET", Unit::unit, 0, 1, 0.5f, 0},
        {"NOISE", Unit::unit, 0, 1, 0.1f, 0},
        {"SCATTER", Unit::unit, 0, 1, 0.0f, 0},
    },
    {
        // PLUCK
        {"TUNE", Unit::semis, -24, 24, 0.5f, 49},
        {"DECAY", Unit::seconds, 0, 0, 0.81f, 0},
        {"BRIGHT", Unit::unit, 0, 1, 0.6f, 0},
        {"PICK", Unit::unit, 0, 1, 0.6f, 0},
        {"POSITION", Unit::unit, 0, 1, 0.2f, 0},
        {"STIFF", Unit::unit, 0, 1, 0.0f, 0},
        {"BODY", Unit::unit, 0, 1, 0.3f, 0},
        {"TENSION", Unit::bipolar, 0, 1, 0.5f, 0},
    },
    {
        // GENDY
        {"POINTS", Unit::count, 3, 16, 5.f / 13, 14},
        {"AMP WALK", Unit::unit, 0, 1, 0.15f, 0},
        {"TIMEWALK", Unit::unit, 0, 1, 0.1f, 0},
        {"WILD", Unit::unit, 0, 1, 0.2f, 0},
        {"LOCK", Unit::unit, 0, 1, 1.0f, 0},
        {"SMOOTH", Unit::unit, 0, 1, 0.5f, 0},
        {"BOUNDS", Unit::unit, 0, 1, 0.8f, 0},
        {"RATE", Unit::unit, 0, 1, 0.6f, 0},
    },
    {
        // PULSAR
        {"TUNE", Unit::semis, -24, 24, 0.5f, 49},
        {"FORMANT", Unit::ratioX, 1.0f, 16.0f, 0.396f, 0},
        {"DENSITY", Unit::unit, 0, 1, 0.0f, 0},
        {"SHAPE", Unit::unit, 0, 1, 0.0f, 0},
        {"ENVELOPE", Unit::unit, 0, 1, 0.5f, 0},
        {"MASK", Unit::choice, 0, 7, 0.0f, 8},
        {"JITTER", Unit::unit, 0, 1, 0.0f, 0},
        {"LENGTH", Unit::decayInf, 0, 0, 1.0f, 0},
    },
    {
        // ORGAN
        {"16'", Unit::count, 0, 8, 1.0f, 9},
        {"5 1/3'", Unit::count, 0, 8, 1.0f, 9},
        {"8'", Unit::count, 0, 8, 1.0f, 9},
        {"4'", Unit::count, 0, 8, 0.0f, 9},
        {"2 2/3'", Unit::count, 0, 8, 0.0f, 9},
        {"2'", Unit::count, 0, 8, 0.0f, 9},
        {"1 3/5'", Unit::count, 0, 8, 0.0f, 9},
        {"1 1/3'", Unit::count, 0, 8, 0.0f, 9},
    },
    {
        // KICK
        {"PITCH", Unit::hz, 30, 250, .35f, 0},
        {"BEND", Unit::octaves, 0, 4, .45f, 0},
        {"TIME", Unit::seconds, 0, 0, .42f, 0},
        {"DECAY", Unit::seconds, 0, 0, .66f, 0},
        {"ATTACK", Unit::seconds, 0, 0, 0, 0},
        {"COLOUR", Unit::unit, 0, 1, .3f, 0},
        {"PULSE", Unit::unit, 0, 1, .5f, 0},
        {"DRIVE", Unit::unit, 0, 1, .25f, 0},
        {"TONE", Unit::hz, 150, 18000, .75f, 0},
        {"ACCENT", Unit::unit, 0, 1, .5f, 0},
    },
};
inline void extendedText(const Control &c, float v, int machine, char *text, size_t size) {
    const float linear = c.lo + (c.hi - c.lo) * v;
    switch (c.unit) {
    case Unit::choice: {
        constexpr const char *surfaces[] = {"RIPPLE", "SADDLE", "LATTICE", "DUNE", "KNOT", "CRATER"};
        constexpr const char *masks[] = {"ALL", "1/2", "2/3", "3/4", "5/8", "RND 50", "RND 25", "EUCLID"};
        std::snprintf(text, size, "%s", (machine == 9 ? surfaces : masks)[int(std::lround(linear))]);
        break;
    }
    case Unit::vowel: {
        const char *letters = "AEIOU";
        int i = int(v * 4);
        float mix = v * 4 - i;
        if (i == 4 || mix < .15f || mix > .85f)
            std::snprintf(text, size, "%c", letters[i + (mix > .85f)]);
        else
            std::snprintf(text, size, "%c>%c", letters[i], letters[i + 1]);
        break;
    }
    case Unit::material: {
        constexpr const char *names[] = {"STRING", "BAR", "DRUM", "BELL"};
        int i = int(std::lround(v * 3));
        std::snprintf(text, size, "%s%s", std::abs(v * 3 - i) < .08f ? "" : "~", names[i]);
        break;
    }
    case Unit::ratioX:
        std::snprintf(text, size, "%.2fx", c.lo * std::pow(c.hi / c.lo, v));
        break;
    case Unit::rateHz:
        std::snprintf(text, size, "%.2fHz", c.hi * v * v);
        break;
    case Unit::semis:
        std::snprintf(text, size, linear == 0 ? "%.0fst" : "%+.0fst", linear);
        break;
    case Unit::bipolar:
        std::snprintf(text, size, "%+.0f", (v * 2 - 1) * 100);
        break;
    case Unit::count:
        std::snprintf(text, size, "%.0f", linear);
        break;
    case Unit::number:
        std::snprintf(text, size, "%.2f", linear);
        break;
    case Unit::decayInf:
    case Unit::seconds: {
        if (c.unit == Unit::decayInf && v >= .995f) {
            std::snprintf(text, size, "INF");
            break;
        }
        float seconds = v <= 0 ? 0 : .0005f * (std::pow(20000.f, v) - 1);
        if (seconds < 1)
            std::snprintf(text, size, "%.0fms", seconds * 1000);
        else
            std::snprintf(text, size, "%.2fs", seconds);
        break;
    }
    case Unit::hz: {
        float f = c.lo * std::pow(c.hi / c.lo, v);
        if (f >= 1000)
            std::snprintf(text, size, "%.1fk", f / 1000);
        else if (f < 100)
            std::snprintf(text, size, "%.1fHz", f);
        else
            std::snprintf(text, size, "%.0fHz", f);
        break;
    }
    case Unit::octaves:
        std::snprintf(text, size, "%.1foc", linear);
        break;
    case Unit::unit:
        std::snprintf(text, size, "%.0f", v * 100);
        break;
    }
}
}
