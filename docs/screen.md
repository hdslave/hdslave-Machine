# Machine screen

One emblem sits beside the voice name and its short description. The machine number and eight voice meters occupy the right end of the header. Each machine's 68 × 21 pixel print begins one clear pixel below the divider (canvas x=248, y=38). It fades beneath the section label and mode value. The side diagram shows the selected group, with the output trace at its base. In KIT, the emblem, title and diagram follow the selected slot. The header shows its slot number and voice code, the meters show slot activity, and the print identifies KIT. [All 18 headers and prints](../assets/previews/machine-prints-2x.png)

Of the three initial directions, Phase Fold left the most usable space around the values. Its original screen is preserved. The finished layout keeps its eight scrolling rows and gives the sound diagrams and three knob cards a six-pixel gutter. [Three directions](../assets/explore/directions.png) · [First take](../assets/explore/phase-fold-take-1.png) · [Header studies at native size](../assets/explore/header-print-proofs.png) · [Panel contact sheet](../assets/previews/contact-sheet-2x.png)

## Playing the page

Turn the wheel to move by row. Push to enter or leave edit mode. During an edit, the wheel moves one choice or 1/128 of a continuous range per detent. Keep turning on MACHINE or a kit slot’s MACH row to audition successive machines. An external machine change exits the edit. Shift and wheel use 1/1024 when the host forwards the event. Minus and Plus jump to the adjacent group with one preceding row for context, or edit by a coarse increment. K1–K3 follow the cursor and the next two rows. A hollow marker points from the physical pot toward its pickup value. Hold Shift for the legend.

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

The real-host run builds the silent arm64 CLAP demo in tools/ui_demo/out and captures Edit and Stage. The host build comes from the adjacent hdslaveOS checkout (../melodee-hdslave, branch hdslave); set MELODEE_SCREEN_TREE if it lives elsewhere. The preview fixture exports native, 3×, RGB565 and host-composited PNGs for every state. [Real host captures](../assets/previews/host/) · [Machine sweep](../assets/previews/sweep-strip.png) · [Cursor glide](../assets/previews/glide-strip.png) · [Meters and scope](../assets/previews/voices-scope-strip.png)

## Extended voices

The 17-machine edition appends TERRAIN, PHASE, VOX, MODAL, PLUCK, GENDY, PULSAR and ORGAN in engine order. Each has a matching 30-pixel emblem and a machine-specific panel. Turn X or Y to move the orbit over TERRAIN's mesh, D or V to change the phase bend, or POSITION to change the modal strike or string pick. VOX separates the vowel markers from the pulse train. GENDY samples the scope at its breakpoint count. PULSAR shows the pulse envelope above eight mask cells, and ORGAN draws each drawbar at its current detent. MODAL, PLUCK, VOX and PULSAR also work as KIT slots. Machine numbers and CLAP IDs already in use retain their meanings. [New voice pages](../assets/previews/new-machines.png)

KICK follows ORGAN as machine 17 (18/18 on the display). Its two pressure plates are the approved emblem. The chart shows the excitation pulse, falling pitch and amp contour. KICK has ten Data controls, can occupy a KIT slot and uses one voice on its own. Data I and J are Role::Data rows with data 8 and 9. TONE and ACCENT use IDs 33 and 34 on the main machine and fields 11 and 12 in a slot. The original four Screen entry points and the Model layout are unchanged. [KICK through the real host](../assets/previews/host/kick.png)

## Verification and cost

All 30 native fixture frames matched the approved layout pixel for pixel. The Mac and Linux arm64 fixtures passed full and incremental painting, stride, scrolling, controls and allocation checks. The slot tests build each of the twelve supported voices in each of the eight slots and compare the emblem, title and sound diagram with the standalone voice. They also switch CY/CB mode in place. AddressSanitizer and UndefinedBehaviorSanitizer passed on Mac.

Melodee loaded the ARM64 instrument. Screens from all 18 machine selections, all 96 slot assignments and Stage were checked against the renderer for machine identity, raised print placement and active slot indication. [Instrument screens from Melodee](../assets/previews/engine-controls.png)

Mean elapsed milliseconds per paint over 2,400 iterations per case.

| Paint | M4 Max | Linux arm64 VM, Cortex-A55 compiler target |
|---|---:|---:|
| Full FM page | 0.0273 | 0.0334 |
| Full TERRAIN page | 0.0313 | 0.0382 |
| Cursor | 0.00585 | 0.00965 |
| Meter | 0.00052 | 0.00065 |
| Idle | 0.00016 | 0.00013 |

The FM-to-BD switch replaced 5,734 pixels. Subsequent sweep frames changed at most 232 header pixels. The meter and scope together changed at most 197 pixels in their fixture. An idle paint touched zero pixels. The active art and font tables occupy 53,744 bytes, and the screen state occupies 2,432 bytes. Physical MPC LCD contrast, SPI timings and control feel require a hardware run.
