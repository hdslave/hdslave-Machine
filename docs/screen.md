# Machine screen

A warped checkerboard and a dark violet sphere share a 68 × 21 pixel print beside the machine name. The name and the machine number each have their own space. The nine lavender emblems describe their sounds in silhouette. A larger version appears above the drum diagrams. The dots, grid and print stay still.

Of the three initial directions, Phase Fold left the most usable space around the values. Its original screen is preserved. The finished layout keeps its eight scrolling rows and gives the sound diagrams and three knob cards a six-pixel gutter. [Three directions](../assets/explore/directions.png) · [First take](../assets/explore/phase-fold-take-1.png) · [Header studies at native size](../assets/explore/header-print-proofs.png) · [Panel contact sheet](../assets/previews/contact-sheet-2x.png)

## Playing the page

Turn the wheel to move by row. Push to enter or leave edit mode. During an edit, the wheel moves one choice or 1/128 of a continuous range per detent. Shift and wheel use 1/1024 when the host forwards the event. Minus and Plus jump to the first row of the adjacent group or edit by a coarse increment. K1–K3 follow the cursor and the next two rows. A hollow marker points from the physical pot toward its pickup value. Hold Shift for the legend.

The interface accepts the engine's normalized rows and display strings. FM ENV adds Group::FmEnv and the FmAttack, FmDecay, FmEnd and FmLevel roles. The data field identifies modulator A (0) or B (1). The KIT SLOT row uses local ID 0xffffff00. The demo glue handles a wheel edit on that ID locally and drops it from the parameter mapping. An AMP DECAY value of INF draws a held level until release.

## Reproducing the checks

From the repository root, run the following commands. Gallery generation uses Python with Pillow and the supplied Melodee chrome. Set MACHINE_CHROME to the path to a different copy of the chrome reference if needed. The font and art build scripts live in tools/ui_preview and output native pixel arrays. The runtime draws them directly.

```sh
make -C ui
tools/ui_preview/run --test
tools/ui_preview/run --bench
tools/ui_preview/run --render
colima start melodee
tools/ui_preview/linux-check
tools/ui_demo/run
```

The real-host run builds the silent arm64 CLAP demo in tools/ui_demo/out and captures Edit and Stage. Set MELODEE_SCREEN_TREE if the host build lives elsewhere. The preview fixture exports native, 3×, RGB565 and host-composited PNGs for every state. [Real host captures](../assets/previews/host/) · [Machine sweep](../assets/previews/sweep-strip.png) · [Cursor glide](../assets/previews/glide-strip.png) · [Meters and scope](../assets/previews/voices-scope-strip.png)

## Nine-machine measurements

All 21 preview hashes matched on Mac and Linux arm64. Mean elapsed milliseconds per paint over 2,400 calls were 0.0215 full, 0.0064 cursor and 0.0017 meter on an M4 Max, and 0.0281 full, 0.0098 cursor and 0.0019 meter in Colima built with -O3 -mcpu=cortex-a55. Embedded fonts and art occupy 42,312 bytes. Screen state occupies 2,416 bytes.

The initial machine change replaced 4,798 pixels across the header and rows. Subsequent sweep frames changed at most 218 pixels in the header. Peak counts were 5,513 for a cursor move, 35 for meters, 213 for scope and 247 for meters with scope. Idle touched zero pixels. Tests compare partial paints to full redraws, check stride padding, check header and card clearances, and count allocations around controls and painting.

Actual panel contrast, SPI transfer time, physical control feel and drawing alongside the running audio engine still need a test on the MPC.

## Extended voices

The 17-machine edition appends TERRAIN, PHASE, VOX, MODAL, PLUCK, GENDY, PULSAR and ORGAN in engine order. Each has a matching 30-pixel emblem and a machine-specific panel. Turn X or Y to move the orbit over TERRAIN's mesh, D or V to change the phase bend, or POSITION to change the modal strike or string pick. VOX separates the vowel markers from the pulse train. GENDY samples the scope at its breakpoint count. PULSAR shows the pulse envelope above eight mask cells, and ORGAN draws each drawbar at its current detent. MODAL, PLUCK, VOX and PULSAR also work as KIT slots. Machine numbers and CLAP IDs already in use retain their meanings. [New voice pages](../assets/previews/new-machines.png)

All 29 reference states passed on Mac and Linux arm64. The terrain full redraw averaged 0.0250 ms on Mac and 0.0337 ms in the Cortex-A55-targeted VM build. The combined GENDY breakpoint, scope and meter update changed 660 pixels. Emblems, fonts, portraits and header use 49,512 bytes in total.
