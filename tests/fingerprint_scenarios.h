// hdslave Machine's fingerprint scenarios (tests/fingerprint.cpp): each preset played, each machine and algorithm,
// KIT's grooves, the LFO on every destination, other block sizes and rates, and random patches. Add new scenarios at
// the end; never change an old one (its hash is what the sound was).
#pragma once

namespace
{
std::vector<Event> chordEvents (uint64_t at, std::initializer_list<int> keys, uint64_t hold)
{
    std::vector<Event> list;
    for (int k : keys) list.push_back ({ at, Event::noteOn, "", (double) k }), list.push_back ({ at + hold, Event::noteOff, "", (double) k });
    return list;
}
std::vector<Event> beats (int key, uint64_t every, int count)
{
    std::vector<Event> list;
    for (int b = 0; b < count; ++b) list.push_back ({ (uint64_t) b * every, Event::noteOn, "", (double) key });
    return list;
}

std::vector<Scenario> scenarios()
{
    std::vector<Scenario> list;
    const char* const keys[] = { "default", "fm-keys", "fm-bell", "fm-bass", "fm-pad", "acid-fm", "kick", "long-kick", "snare", "clap",
                                 "closed-hat", "open-hat", "cymbal", "cowbell", "fm-tom", "ground-sine", "kit" };
    for (const char* key : keys)
    {
        Scenario s;
        s.name = std::string ("preset-") + key;
        s.preset = key;
        s.seconds = 3.0;
        if (std::string (key) == "kit")
            for (int k = 0; k < 16; ++k) s.events.push_back ({ (uint64_t) k * 6000, Event::noteOn, "", (double) (36 + (k * 5) % 8) });
        else
        {
            s.events = chordEvents (0, { 48, 55, 60 }, 40000);
            for (auto e : chordEvents (60000, { 62 }, 30000)) s.events.push_back (e);
        }
        list.push_back (s);
    }
    for (int a = 0; a < 8; ++a)
    {
        Scenario s;
        s.name = "algorithm-" + std::to_string (a + 1);
        s.values = { { "Data A", a / 7.0 }, { "FM A Level", 0.6 }, { "FM B Level", 0.55 }, { "FM A End", 0.4 }, { "FM B End", 0.3 },
                     { "Data E", 0.7 }, { "Data G", 0.35 }, { "Data H", 0.4 } };
        s.events = chordEvents (0, { 45, 57, 64 }, 50000);
        s.seconds = 2.0;
        list.push_back (s);
    }
    for (int m = 1; m <= 7; ++m)
    {
        Scenario s;
        s.name = "machine-" + std::to_string (m);
        s.values = { { "Machine", (double) m } };
        s.events = beats (60, 9000, 8);
        s.seconds = 2.5;
        list.push_back (s);
    }
    for (int d = 0; d < 11; ++d)
    {
        Scenario s;
        s.name = "lfo-dest-" + std::to_string (d);
        s.values = { { "LFO Dest", (double) d }, { "LFO Depth", 0.6 }, { "LFO Speed", 0.6 }, { "LFO Wave", (double) (d % 5) },
                     { "Filter Freq", 0.7 }, { "Filter Reso", 0.4 } };
        s.events = chordEvents (0, { 52, 59 }, 60000);
        s.seconds = 1.5;
        list.push_back (s);
    }
    {
        Scenario s;
        s.name = "kit-groove";
        s.values = { { "Machine", 8 }, { "Kit 1 Pan", -0.4 }, { "Kit 4 Pan", 0.5 }, { "Filter Type", 1 }, { "Filter Freq", 0.2 } };
        const int pattern[16] = { 36, 39, 40, 39, 37, 39, 40, 39, 36, 36, 41, 39, 37, 42, 43, 38 };
        for (int b = 0; b < 32; ++b) s.events.push_back ({ (uint64_t) b * 6000, Event::noteOn, "", (double) pattern[b % 16] });
        s.seconds = 4.5;
        list.push_back (s);
    }
    for (uint32_t block : { 1u, 37u, 4096u })
    {
        Scenario s;
        s.name = "block-" + std::to_string (block);
        s.preset = "acid-fm";
        s.block = block;
        s.events = chordEvents (0, { 40 }, 10000);
        for (auto e : chordEvents (12001, { 47 }, 7000)) s.events.push_back (e);
        s.events.push_back ({ 15000, Event::param, "Filter Freq", 0.6 });
        s.seconds = 1.0;
        list.push_back (s);
    }
    for (double rate : { 44100.0, 96000.0 })
    {
        Scenario s;
        s.name = "rate-" + std::to_string ((int) rate);
        s.preset = "kit";
        s.rate = rate;
        for (int k = 0; k < 8; ++k) s.events.push_back ({ (uint64_t) (k * rate / 8), Event::noteOn, "", (double) (36 + k) });
        s.seconds = 2.0;
        list.push_back (s);
    }
    for (uint32_t r = 0; r < 24; ++r)
    {
        Scenario s;
        s.name = "random-" + std::to_string (r);
        s.values = randomValues (5000 + r, r % 9);
        s.events = chordEvents (0, { 36 + (int) r % 8, 48, 60 }, 20000);
        for (auto e : chordEvents (24000, { 38, 43, 67 }, 9000)) s.events.push_back (e);
        s.events.push_back ({ 30000, Event::param, "Machine", (double) ((r + 4) % 9) });
        s.events.push_back ({ 30001, Event::noteOn, "", 40.0 });
        s.seconds = 1.5;
        list.push_back (s);
    }
    // The second barrage (0.1.0 too): each new preset played, each new machine's phrase, the new kit.
    for (const char* key : { "terrain-drone", "terrain-bass", "phase-lead", "phase-acid", "vox-choir", "vox-knock", "marimba", "gong",
                             "pluck-harp", "pluck-bass", "gendy-insects", "gendy-buzz", "pulsar-rhythm", "pulsar-formant", "drawbar-organ",
                             "full-organ", "resonator-kick", "boom-kick" })
    {
        Scenario s;
        s.name = std::string ("preset-") + key;
        s.preset = key;
        s.seconds = 2.5;
        s.events = chordEvents (0, { 45, 52, 57 }, 40000);
        for (auto e : chordEvents (60000, { 64 }, 30000)) s.events.push_back (e);
        list.push_back (s);
    }
    for (int m = 9; m <= 17; ++m)
    {
        Scenario s;
        s.name = "machine-" + std::to_string (m);
        s.values = { { "Machine", (double) m }, { "LFO Dest", 4 }, { "LFO Depth", 0.3 }, { "LFO Speed", 0.65 } };
        const int notes[6] = { 45, 57, 52, 64, 48, 60 };
        for (int b = 0; b < 6; ++b)
            for (auto e : chordEvents ((uint64_t) b * 14000, { m == 17 ? 36 + b : notes[b] }, 11000)) s.events.push_back (e);
        s.events.push_back ({ 30000, Event::param, "Data B", 0.8 });
        s.seconds = 2.0;
        list.push_back (s);
    }
    {
        Scenario s;
        s.name = "kit-explorer";
        s.preset = "kit-explorer";
        const int pattern[16] = { 36, 42, 40, 39, 37, 43, 41, 39, 36, 36, 42, 39, 37, 40, 43, 38 };
        for (int b = 0; b < 32; ++b)
            for (auto e : chordEvents ((uint64_t) b * 6000, { pattern[b % 16] }, 3000)) s.events.push_back (e);
        s.seconds = 4.5;
        list.push_back (s);
    }
    return list;
}
} // namespace
