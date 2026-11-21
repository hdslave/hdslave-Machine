// Copyright (C) 2026 hdslave
// hdslave Machine: its page on Melodee's screen (melodee_screen.h). Pure drawing and interaction: no threads, no
// heap use while running, no dependencies beyond the C++17 standard library. The plug-in glue calls it on the
// main thread.
#pragma once
#include <cstdint>

namespace machine::ui {

constexpr int width = 320, height = 218; // the canvas
constexpr int placeBarRows = 2;          // rows 0-1 lie under Melodee's place bar

enum class Group : uint8_t { Machine, Slot, Filter, Amp, Lfo, FmEnv };
enum class Role : uint8_t {
    Machine, Data, KitSlot, KitMachine, Level, Pan,
    FilterFreq, FilterReso, FilterType, FilterEnv, FilterAttack, FilterDecay,
    AmpAttack, AmpHold, AmpDecay, AmpRelease, AmpDrive, Volume,
    LfoSpeed, LfoDepth, LfoWave, LfoDest,
    FmAttack, FmDecay, FmEnd, FmLevel,
};

struct Row {           // one parameter, as the list shows it
    uint32_t id;       // its CLAP parameter id (what knobs() names, what an Edit sets)
    Group group;
    Role role;
    uint8_t data;      // Data A-H as 0-7, FM envelope A/B as 0/1, else 255
    char name[10];     // capitals as shown: "RATIO C", "SWP TIME"
    char text[12];     // its value as shown: "1.00", "LP", "+0.25", "CLOSED"
    float value;       // where it sits, 0..1
    float initial;     // its default, 0..1
    bool bipolar;      // drawn from the middle
    uint16_t steps;    // 0: continuous; else how many choices
};

struct Model {         // what the engine says, each paint
    uint8_t machine;   // stable machine number, in ui/kinds.h order
    uint8_t rowCount;
    Row rows[40];
    float voices[8];        // each voice's level now (FM: its 8 voices; a drum: [0]; KIT: each slot), 0..1
    uint8_t kitSlot;        // KIT: the slot shown, 0-7
    uint8_t kitMachines[8]; // KIT: a machine number from slotKinds in ui/kinds.h
    float scope[128];       // the latest output, -1..1, oldest first; all 0 when not given
};

struct Host {          // what Melodee's canvas says
    uint64_t nowMs;
    uint32_t accent;   // the track's colour, 0xFFRRGGBB
    int16_t pot[3];    // K1-K3 positions 0..127, -1 not known
    bool engaged[3];   // the pot drives its value (false: takeover pending)
    bool shift, playing;
};

enum class Key : uint8_t { Wheel, Push, Nudge }; // as MELODEE_SCREEN_WHEEL, _PUSH, _NUDGE

struct Edit { uint32_t id; float value; };       // a parameter the page set, 0..1

class Screen {
public:
    // An input on the page. The parameters it changed go in `out` (at most 4), `count` says how many. True if used.
    bool input(const Model& m, Key key, int value, bool shift, Edit* out, int& count);
    // What K1-K3 turn now: the cursor's row and the next two; 0xFFFFFFFF for none.
    void knobs(const Model& m, uint32_t ids[3]) const;
    // Draws what changed since the last call into `pixels` (0xAARRGGBB, `stride` pixels a row), which hold the last
    // picture unless `whole`. False when nothing changed (and nothing was touched).
    bool paint(const Model& m, const Host& h, uint32_t* pixels, int stride, bool whole);
    // Something is moving (the sweep, the cursor's glide, meters or scope while voices sound): ask for paints.
    bool animating(const Model& m, const Host& h) const;
private:
    int controlMachine_ = -1, glideTarget_ = -1;
    int cursor_ = 0, top_ = 0, lastCursor_ = -1, lastTop_ = -1;
    int fromMachine_ = 0, fromSlot_ = 0, fromKitMachine_ = 1;
    int sweepFrame_ = 16, glideY_ = -1, glideFrom_ = 0;
    uint64_t sweepStart_ = 0, glideStart_ = 0;
    bool seen_ = false, editing_ = false, lastEditing_ = false;
    Model previous_{};
    Host previousHost_{};
};

} // namespace machine::ui
