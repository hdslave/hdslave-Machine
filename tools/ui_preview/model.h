// The silent demo and preview fixtures share a deterministic parameter source.
#pragma once
#include "../../ui/machine_ui.h"
#include "../../ui/kinds.h"
#include "extended_data.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace fixture {
using namespace machine::ui;
inline constexpr const char *dataNames[8][8] = {
    {"ALGO", "RATIO C", "RATIO A", "RATIO B", "HARM", "DETUNE", "FDBK", "MIX"},
    {"TUNE", "DECAY", "SWEEP", "SWP TIME", "CLICK", "DRIVE", "FM", "NOISE"},
    {"TUNE", "TONE DEC", "NOIS DEC", "SNAPPY", "BODY", "FILTER", "DRIVE"},
    {"BURSTS", "SPREAD", "DECAY", "TONE", "FILTER", "ROOM"},
    {"MODE", "TONE", "DECAY", "FILTER", "METAL", "NOISE"},
    {"MODE", "TONE", "DECAY", "RING", "FILTER"},
    {"PITCH", "RATIO", "INDEX", "P DECAY", "I DECAY", "A DECAY", "SWEEP", "NOISE"},
    {"SOURCE", "PITCH", "DECAY", "SWEEP"}};
// Public ids follow the engine. The selected kit slot is local screen navigation.
constexpr uint32_t slotSelector = 0xffffff00u;
constexpr int slotsBase = 100, slotStride = 16, slotFields = 11, baseCount = 33, storageSize = 228,
              paramCount = 121;
inline int idAt(int i) {
    return i < baseCount
               ? i
               : slotsBase + (i - baseCount) / slotFields * slotStride + (i - baseCount) % slotFields;
}
inline bool validId(uint32_t id) {
    return id < baseCount ||
           (id >= slotsBase && id < storageSize && (id - slotsBase) % slotStride < slotFields);
}
inline constexpr const char *envNames[] = {"A ATK", "A DEC", "A END", "A LEV",
                                           "B ATK", "B DEC", "B END", "B LEV"};
inline constexpr float envDefault[] = {.22f, .58f, .32f, .88f, .06f, .72f, .14f, .65f};
inline constexpr const char *shared[] = {"FREQ",   "RESO",  "TYPE",  "ENV",     "ATTACK", "DECAY",
                                         "ATTACK", "HOLD",  "DECAY", "RELEASE", "DRIVE",  "VOLUME",
                                         "SPEED",  "DEPTH", "WAVE",  "DEST"};
inline constexpr float commonDefault[] = {.62f, .42f, 0,    .5f, .06f, .35f, .08f, .23f,
                                          .49f, .42f, .12f, .8f, .28f, .84f, 0,    0};
inline constexpr float dataDefault[] = {0, .25f, .75f, .25f, .625f, .18f, .41f, .5f};
inline int notch(float v, int n) {
    return int(std::clamp(v, 0.f, 1.f) * (n - 1) + .5f);
}
inline void format(Row &r, int machine) {
    char *t = r.text;
    auto n = sizeof r.text;
    float v = r.value;
    constexpr const char *filters[] = {"LP", "HP", "BP"}, *waves[] = {"TRI", "SAW", "SQR", "S+H", "SINE"},
                         *sources[] = {"SINE", "NOISE", "IMPULSE"},
                         *dest[] = {"PITCH",  "FREQ",   "AMP",    "DATA A", "DATA B", "DATA C",
                                    "DATA D", "DATA E", "DATA F", "DATA G", "DATA H"};
    const char *literal = nullptr;
    switch (r.role) {
    case Role::Machine:
        literal = machineNames[notch(v, machineCount)];
        break;
    case Role::KitMachine:
        literal = machineNames[int(slotKinds[notch(v, slotKindCount)])];
        break;
    case Role::FilterType:
        literal = filters[notch(v, 3)];
        break;
    case Role::LfoWave:
        literal = waves[notch(v, 5)];
        break;
    case Role::LfoDest:
        literal = dest[notch(v, 11)];
        break;
    default:
        break;
    }
    if (r.role == Role::Data && machine >= int(Kind::Terrain)) {
        extendedText(extendedControls[machine - int(Kind::Terrain)][r.data], v, machine, t, n);
        return;
    }
    if (r.role == Role::Data && r.data == 0) {
        if (machine == 7)
            literal = sources[notch(v, 3)];
        if (machine == 4)
            literal = v < .5f ? "CLOSED" : "OPEN";
        if (machine == 5)
            literal = v < .5f ? "CY" : "CB";
    }
    if (r.role == Role::AmpDecay && v >= .995f)
        literal = "INF";
    if (literal)
        std::snprintf(t, n, "%s", literal);
    else if (r.steps > 1)
        std::snprintf(t, n, "%d", 1 + notch(v, r.steps));
    else if (r.bipolar)
        std::snprintf(t, n, "%+.2f", double(v * 2 - 1));
    else if (r.role == Role::Data && machine == 0 && r.data >= 1 && r.data <= 3)
        std::snprintf(t, n, "%.2f", double(.5f + v * 2));
    else if (r.role == Role::FilterFreq)
        std::snprintf(t, n, "%.1fk", double(v * 8));
    else if (r.role == Role::FmAttack || r.role == Role::FmDecay || r.role == Role::FilterAttack ||
             r.role == Role::FilterDecay || (r.role >= Role::AmpAttack && r.role <= Role::AmpRelease))
        std::snprintf(t, n, "%dms", int(v * 999));
    else
        std::snprintf(t, n, "%d", notch(v, 128));
}
inline float defaultValue(int id) {
    if (id == 0)
        return 0;
    if (id <= 8)
        return dataDefault[id - 1];
    if (id < 17)
        return envDefault[id - 9];
    if (id < baseCount)
        return commonDefault[id - 17];
    int k = (id - slotsBase) % slotStride, slot = (id - slotsBase) / slotStride;
    if (!k)
        return (slot == 2 ? 1 : slot % 7) / float(slotKindCount - 1);
    if (k <= 8)
        return dataDefault[k - 1];
    return k == 9 ? .8f : .5f;
}
template <class Reader> Model build(Reader read, int slot) {
    Model m{};
    m.machine = uint8_t(notch(read(0), machineCount));
    m.kitSlot = uint8_t(std::clamp(slot, 0, 7));
    for (int i = 0; i < 8; ++i)
        m.kitMachines[i] = uint8_t(slotKinds[notch(read(slotsBase + i * slotStride), slotKindCount)]);
    int machine = m.machine;
    bool kit = machine == 8;
    int drum = kit ? m.kitMachines[m.kitSlot] : machine;
    auto add = [&](uint32_t id, Group g, Role role, const char *name, int steps = 0, bool bipolar = false,
                   int data = 255) {
        auto &r = m.rows[m.rowCount++];
        r.id = id;
        r.group = g;
        r.role = role;
        r.data = uint8_t(data);
        std::snprintf(r.name, sizeof r.name, "%s", name);
        r.steps = uint16_t(steps);
        r.bipolar = bipolar;
        r.initial = id == slotSelector ? 0.f : defaultValue(id);
        r.value = id == slotSelector ? m.kitSlot / 7.f : std::clamp(read(id), 0.f, 1.f);
        if (steps > 1) {
            r.value = float(notch(r.value, steps)) / (steps - 1);
            r.initial = float(notch(r.initial, steps)) / (steps - 1);
        }
        format(r, drum);
    };
    add(0, Group::Machine, Role::Machine, "MACHINE", machineCount);
    int base = kit ? slotsBase + slotStride * m.kitSlot : 0;
    if (kit) {
        add(slotSelector, Group::Slot, Role::KitSlot, "SLOT", 8);
        add(base, Group::Slot, Role::KitMachine, "MACH", slotKindCount);
    }
    Group g = kit ? Group::Slot : Group::Machine;
    for (int a = 0; a < 8 && drum < 8 && dataNames[drum][a]; ++a) {
        int steps = 0;
        if (!a) {
            constexpr int counts[] = {8, 0, 0, 8, 2, 2, 0, 3};
            steps = counts[drum];
        }
        add(base + 1 + a, g, Role::Data, dataNames[drum][a], steps, drum == 0 && (a == 4 || a == 7), a);
    }
    if (drum >= int(Kind::Terrain)) {
        const auto &controls = extendedControls[drum - int(Kind::Terrain)];
        for (int a = 0; a < 10 && controls[a].name; ++a) {
            const auto &c = controls[a];
            add(base + 1 + a, g, Role::Data, c.name, c.steps,
                c.unit == Unit::bipolar || c.unit == Unit::semis, a);
            m.rows[m.rowCount - 1].initial = c.initial;
        }
    }
    if (kit) {
        add(base + 9, g, Role::Level, "LEVEL");
        add(base + 10, g, Role::Pan, "PAN", 0, true);
    }
    if (machine == 0)
        for (int i = 0; i < 8; ++i)
            add(9 + i, Group::FmEnv, Role(int(Role::FmAttack) + i % 4), envNames[i], 0, false, i / 4);
    for (int i = 0; i < 16; ++i) {
        Role role = Role(int(Role::FilterFreq) + i);
        add(17 + i,
            i < 6    ? Group::Filter
            : i < 12 ? Group::Amp
                     : Group::Lfo,
            role, shared[i],
            i == 2    ? 3
            : i == 14 ? 5
            : i == 15 ? 11
                      : 0,
            i == 3 || i == 13);
    }
    return m;
}
inline Model model(int machine = 0, int slot = 2) {
    return build(
        [&](int id) {
            if (id == 0)
                return machine / float(machineCount - 1);
            if (machine >= int(Kind::Terrain) && id >= 1 && id <= 8)
                return extendedControls[machine - int(Kind::Terrain)][id - 1].initial;
            return defaultValue(id);
        },
        slot);
}
inline Host host() {
    return {1000, 0xff9d8cff, {-1, -1, -1}, {true, true, true}, false, false};
}
inline void motion(Model &m, int frame, bool meters = true, bool scope = true) {
    for (int k = 0; k < 8 && meters; ++k)
        m.voices[k] = (m.machine == 0 || m.machine >= 8 || k == 0)
                          ? float(.50 + .48 * std::sin(frame * .62 + k * .92))
                          : 0.f;
    if (scope)
        for (int i = 0; i < 128; ++i)
            m.scope[i] = float(std::sin(i * .14 + frame * .55) * (.70 + .22 * std::cos(i * .26)));
}
}
