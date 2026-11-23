#pragma once

#include "hdslave_tiles.h"

namespace machine::ui
{

struct TilesAccess
{
    static void setPinned (Screen& s, int top, int maxRow, int cursor)
    {
        s.pinnedTop_ = top;
        s.pinnedMax_ = maxRow;
        s.cursor_ = cursor;
    }
    static int& tileCursor (Screen& s, uint32_t tile) { return s.tileCursors_[tile % 6]; }
    static int getCursor (const Screen& s) { return s.cursor_; }
    static int getTop (const Screen& s) { return s.top_; }
};

} // namespace machine::ui

namespace machine
{
namespace
{

constexpr int kMachineTileCount = 6; // 3x2 grid

void machineTileRange (const ui::Model& m, uint32_t tile, int& outTop, int& outMax)
{
    const int total = (int) m.rowCount;
    int firstFilter = total, firstAmp = total, firstLfo = total, firstFm = -1;
    for (int i = 0; i < total; ++i)
    {
        if (m.rows[i].group == ui::Group::FmEnv && firstFm < 0) firstFm = i;
        if (m.rows[i].group == ui::Group::Filter && firstFilter == total) firstFilter = i;
        if (m.rows[i].group == ui::Group::Amp && firstAmp == total) firstAmp = i;
        if (m.rows[i].group == ui::Group::Lfo && firstLfo == total) firstLfo = i;
    }

    if (tile == 3) { outTop = firstFilter; outMax = firstAmp; return; }
    if (tile == 4) { outTop = firstAmp;    outMax = firstLfo; return; }
    if (tile == 5) { outTop = firstLfo;    outMax = total;    return; }

    if (firstFm >= 0)
    {
        // FM voice has Machine rows (0..firstFm-1) and FmEnv rows (firstFm..firstFilter-1)
        const int split = std::clamp ((firstFm + 1) / 2, 1, 8);
        if (tile == 0) { outTop = 0; outMax = std::min (split + 1, firstFm); return; }
        if (tile == 1) { outTop = std::max (0, split - 1); outMax = firstFm; return; }
        outTop = firstFm; outMax = firstFilter; return;
    }

    // Other machines or KIT (rows 0..firstFilter-1)
    const int s1 = std::clamp ((firstFilter + 2) / 3, 1, 8);
    const int s2 = std::clamp ((firstFilter * 2 + 2) / 3, s1, firstFilter);
    if (tile == 0) { outTop = 0; outMax = std::min (s1 + 1, firstFilter); return; }
    if (tile == 1) { outTop = std::max (0, s1 - 1); outMax = std::min (s2 + 1, firstFilter); return; }
    outTop = std::max (0, s2 - 1); outMax = firstFilter;
}

uint32_t CLAP_ABI machineTilesCount (const clap_plugin_t*, int* out_cols, int* out_rows)
{
    if (out_cols) *out_cols = 3;
    if (out_rows) *out_rows = 2;
    return kMachineTileCount;
}

const char* CLAP_ABI machineTilesTitle (const clap_plugin_t*, uint32_t tile)
{
    static constexpr const char* kTitles[kMachineTileCount] = {
        "01 // MACHINE VOICE (A)",
        "02 // MACHINE VOICE (B)",
        "03 // FM ENV / KIT SLOT",
        "04 // MULTI-MODE FILTER",
        "05 // AMP ENV & DRIVE",
        "06 // LFO MODULATOR"
    };
    return tile < kMachineTileCount ? kTitles[tile] : "";
}

bool CLAP_ABI machineTilesPaint (const clap_plugin_t* plugin, uint32_t tile, const melodee_screen_canvas_t* canvas, clap_id knobs_out[3])
{
    if (tile >= kMachineTileCount || !canvas || !canvas->pixels) return false;
    auto& p = of (plugin);
    fillModel (p);

    const int savedCursor = ui::TilesAccess::getCursor (p.screen);
    const int savedTop = ui::TilesAccess::getTop (p.screen);

    int top = 0, maxRow = (int) p.model.rowCount;
    machineTileRange (p.model, tile, top, maxRow);
    int cur = std::clamp (ui::TilesAccess::tileCursor (p.screen, tile), top, std::max (top, maxRow - 1));
    ui::TilesAccess::tileCursor (p.screen, tile) = cur;
    ui::TilesAccess::setPinned (p.screen, top, maxRow, cur);

    ui::Host h {};
    h.nowMs = canvas->now_ms;
    h.accent = canvas->accent;
    h.shift = canvas->shift;
    h.playing = canvas->playing;
    for (int k = 0; k < 3; ++k)
    {
        h.pot[k] = canvas->knobs[k].position;
        h.engaged[k] = canvas->knobs[k].engaged;
    }

    const bool drew = p.screen.paint (p.model, h, canvas->pixels, (int) canvas->stride, true);
    if (knobs_out)
    {
        for (int k = 0; k < 3; ++k)
        {
            const int rIdx = cur + k;
            if (rIdx >= top && rIdx < maxRow && p.model.rows[rIdx].id != slotRowId)
                knobs_out[k] = p.model.rows[rIdx].id;
            else
                knobs_out[k] = CLAP_INVALID_ID;
        }
    }

    ui::TilesAccess::setPinned (p.screen, -1, -1, savedCursor);
    return drew;
}

bool CLAP_ABI machineTilesHitTest (const clap_plugin_t* plugin, uint32_t tile, int x, int y, hdslave_tile_hotspot_t* out_spot)
{
    if (!out_spot || tile >= kMachineTileCount) return false;
    *out_spot = {};
    out_spot->param_id = CLAP_INVALID_ID;
    out_spot->param_id_y = CLAP_INVALID_ID;

    auto& p = of (plugin);
    fillModel (p);

    int top = 0, maxRow = (int) p.model.rowCount;
    machineTileRange (p.model, tile, top, maxRow);

    // 1. Top-left emblem / machine header (x: 0..198, y: 2..38) -> Machine selector
    if (x >= 0 && x < 198 && y >= 2 && y < 38)
    {
        out_spot->kind = HDSLAVE_HOTSPOT_PARAM;
        out_spot->param_id = ParamId::machine;
        out_spot->x = 2; out_spot->y = 2; out_spot->w = 194; out_spot->h = 35;
        return true;
    }

    // 2. Top-right 8-slot meter bar (x: 245..318, y: 20..38) -> Kit Slot selector 0..7!
    if (x >= 245 && x < 318 && y >= 20 && y < 38)
    {
        const int slot = std::clamp ((x - 251) / 8, 0, 7);
        out_spot->kind = HDSLAVE_HOTSPOT_ACTION;
        out_spot->custom_id = 10 + slot;
        out_spot->x = (int16_t) (251 + slot * 8);
        out_spot->y = 22;
        out_spot->w = 7;
        out_spot->h = 14;
        return true;
    }

    // 3. Left parameter list (x: 0..198, y: 55..167, rowH = 14)
    if (x >= 0 && x < 198 && y >= 55 && y < 55 + 8 * 14)
    {
        const int visIdx = (y - 55) / 14;
        const int rIdx = top + visIdx;
        if (rIdx >= top && rIdx < maxRow)
        {
            const uint32_t id = p.model.rows[rIdx].id;
            out_spot->x = 4;
            out_spot->y = (int16_t) (55 + visIdx * 14);
            out_spot->w = 192;
            out_spot->h = 14;
            if (id == slotRowId)
            {
                out_spot->kind = HDSLAVE_HOTSPOT_ACTION;
                out_spot->custom_id = 100; // Cycle kit slot
                return true;
            }
            out_spot->kind = HDSLAVE_HOTSPOT_PARAM;
            out_spot->param_id = id;
            return true;
        }
    }

    // 5. Right context plot (x: 200..318, y: 52..143) -> 2D interactive plot!
    if (x >= 200 && x < 318 && y >= 52 && y < 143)
    {
        out_spot->kind = HDSLAVE_HOTSPOT_PARAM;
        out_spot->x = 200; out_spot->y = 52; out_spot->w = 118; out_spot->h = 90;
        out_spot->invert_y = true;
        if (tile == 3)
        {
            out_spot->param_id = ParamId::filterFreq;
            out_spot->param_id_y = ParamId::filterReso;
            return true;
        }
        if (tile == 4)
        {
            out_spot->param_id = ParamId::ampDecay;
            out_spot->param_id_y = ParamId::ampDrive;
            return true;
        }
        if (tile == 5)
        {
            out_spot->param_id = ParamId::lfoSpeed;
            out_spot->param_id_y = ParamId::lfoDepth;
            return true;
        }
        // Voice tiles (0..2): drag first two continuous rows in tile
        for (int r = top; r < maxRow; ++r)
        {
            if (p.model.rows[r].id != slotRowId && p.model.rows[r].id != ParamId::machine)
            {
                if (out_spot->param_id == CLAP_INVALID_ID)
                    out_spot->param_id = p.model.rows[r].id;
                else if (out_spot->param_id_y == CLAP_INVALID_ID)
                {
                    out_spot->param_id_y = p.model.rows[r].id;
                    break;
                }
            }
        }
        if (out_spot->param_id != CLAP_INVALID_ID)
            return true;
    }

    // 6. Right output scope (x: 200..318, y: 144..167) -> Audition note pad
    if (x >= 200 && x < 318 && y >= 144 && y < 167)
    {
        out_spot->kind = HDSLAVE_HOTSPOT_NOTE_PAD;
        out_spot->custom_id = p.model.machine == kit ? (60 + p.slotPicked) : 60;
        out_spot->x = 200; out_spot->y = 144; out_spot->w = 118; out_spot->h = 23;
        return true;
    }

    return false;
}

void CLAP_ABI machineTilesFocus (const clap_plugin_t* plugin, uint32_t tile, int x, int y, bool)
{
    if (tile >= kMachineTileCount) return;
    auto& p = of (plugin);
    fillModel (p);
    int top = 0, maxRow = (int) p.model.rowCount;
    machineTileRange (p.model, tile, top, maxRow);
    if (x >= 0 && x < 198 && y >= 55 && y < 55 + 8 * 14)
    {
        const int rIdx = top + (y - 55) / 14;
        if (rIdx >= top && rIdx < maxRow)
            ui::TilesAccess::tileCursor (p.screen, tile) = rIdx;
    }
}

void CLAP_ABI machineTilesAction (const clap_plugin_t* plugin, uint32_t, int32_t custom_id, int32_t wheel_delta, bool shift)
{
    auto& p = of (plugin);
    if (custom_id >= 10 && custom_id < 18)
    {
        p.slotPicked = custom_id - 10;
        if (p.screenHost) p.screenHost->request_paint (p.host);
    }
    else if (custom_id == 100)
    {
        const int dir = wheel_delta != 0 ? (wheel_delta > 0 ? 1 : -1) : (shift ? -1 : 1);
        p.slotPicked = (p.slotPicked + dir + (int) kitSlots) % (int) kitSlots;
        if (p.screenHost) p.screenHost->request_paint (p.host);
    }
}

const clap_plugin_hdslave_tiles_t tilesExtension {
    machineTilesCount,
    machineTilesTitle,
    machineTilesPaint,
    machineTilesHitTest,
    machineTilesFocus,
    machineTilesAction,
    169,
};

} // namespace
} // namespace machine
