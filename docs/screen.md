# Machine screen

A warped checkerboard and a dark violet sphere share a 68 × 21 pixel print beside the machine name. The name and the machine number each have their own space. Each of the 18 machines has its own lavender sound emblem. The first seven drums also have a larger portrait in the diagram panel. The dots, grid and print stay still.

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

## Extended voices

The 17-machine edition appends TERRAIN, PHASE, VOX, MODAL, PLUCK, GENDY, PULSAR and ORGAN in engine order. Each has a matching 30-pixel emblem and a machine-specific panel. Turn X or Y to move the orbit over TERRAIN's mesh, D or V to change the phase bend, or POSITION to change the modal strike or string pick. VOX separates the vowel markers from the pulse train. GENDY samples the scope at its breakpoint count. PULSAR shows the pulse envelope above eight mask cells, and ORGAN draws each drawbar at its current detent. MODAL, PLUCK, VOX and PULSAR also work as KIT slots. Machine numbers and CLAP IDs already in use retain their meanings. [New voice pages](../assets/previews/new-machines.png)

KICK follows ORGAN as machine 17 (18/18 on the display). Its two pressure plates are the approved emblem. The chart shows the excitation pulse, falling pitch and amp contour. KICK has ten Data controls, can occupy a KIT slot and uses one voice on its own. Data I and J are Role::Data rows with data 8 and 9. TONE and ACCENT use IDs 33 and 34 on the main machine and fields 11 and 12 in a slot. The original four Screen entry points and the Model layout are unchanged. [KICK through the real host](../assets/previews/host/kick.png)

## Verification and cost

All 30 rendered states matched their reference hashes on Mac and Linux arm64. Control, stride, full and partial redraw, zero-allocation, gutter and idle-write tests passed. The Mac fixture also passed with address and undefined-behavior sanitizers enabled. The demo agreed with the engine's 18 machine names, 12 valid KIT choices, extended Data IDs and defaults, and 139-parameter count. The real Melodee host painted Edit and Stage. Its KICK capture returned 33, 34 and 17 for TONE, ACCENT and FREQ.

Mean elapsed milliseconds per paint, 2,400 iterations per case.

| Paint | M4 Max | Linux arm64 VM, Cortex-A55 compiler target |
|---|---:|---:|
| Full FM page | 0.0221 | 0.0299 |
| Full TERRAIN page | 0.0258 | 0.0363 |
| Cursor | 0.0065 | 0.0105 |
| Meter | 0.0017 | 0.0019 |
| Idle | 0.00017 | 0.00015 |

The initial FM-to-BD change replaced 4,792 pixels. Subsequent sweep frames changed at most 218 pixels in the header. Peak counts were 5,513 for the cursor glide, 35 for meters, 213 for scope and 247 for meters with scope. The GENDY fixture changed 660 pixels across its breakpoint diagram, meters and scope. At rest the renderer touched zero pixels. Embedded art and fonts occupy 50,412 bytes of the 256 KB budget. Screen state occupies 2,416 bytes.

The Linux figures measure the VM. Actual panel contrast, SPI transfer time, physical control feel and drawing alongside the running sound engine still need a test on the MPC.
