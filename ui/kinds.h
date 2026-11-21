// Copyright (C) 2026 hdslave
#pragma once
#include <cstdint>
#include <iterator>

namespace machine::ui {
enum class Kind : uint8_t {
    FmTone,
    Bd,
    Sd,
    Cp,
    Hh,
    CyCb,
    Efm,
    Gnd,
    Kit,
    Terrain,
    Phase,
    Vox,
    Modal,
    Pluck,
    Gendy,
    Pulsar,
    Organ,
    Kick
};
inline constexpr const char *machineNames[] = {"FM TONE",  "BD",    "SD",    "CP",      "HH",    "CY/CB",
                                               "EFM DRUM", "GND",   "KIT",   "TERRAIN", "PHASE", "VOX",
                                               "MODAL",    "PLUCK", "GENDY", "PULSAR",  "ORGAN", "KICK"};
inline constexpr Kind slotKinds[] = {Kind::Bd,    Kind::Sd,  Kind::Cp,     Kind::Hh,
                                     Kind::CyCb,  Kind::Efm, Kind::Gnd,    Kind::Modal,
                                     Kind::Pluck, Kind::Vox, Kind::Pulsar, Kind::Kick};
inline constexpr int machineCount = int(std::size(machineNames)), slotKindCount = int(std::size(slotKinds));
inline int slotKind(uint8_t id) {
    for (Kind kind : slotKinds)
        if (uint8_t(kind) == id)
            return id;
    return int(Kind::Bd);
}
}
