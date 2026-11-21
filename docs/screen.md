# Machine screen

Phase Fold pairs a pinned violet ribbon and native-size machine emblems with eight scrolling parameter rows. The second of three generated directions left the most room for values under Melodee’s soft keys. [Concept sheet](../assets/explore/directions.png) · [RGB565 contact sheet](../assets/previews/contact-sheet-2x.png)

Turn the wheel to move by row. Push to enter or leave edit mode. In edit mode the wheel changes a choice by one step or a continuous value by 1/128 per detent. Shift changes the continuous wheel increment to 1/1024. Minus and Plus jump to the first row of the adjacent group, or edit by a coarse increment. K1–K3 follow the cursor and its next two rows. A hollow marker on a card shows where to move a pot for takeover. Hold Shift for the control legend.

Run `make -C ui` to build the C++17 library. Run `tools/ui_preview/run` to regenerate the gallery, `tools/ui_preview/run --test` for interaction and pixel checks, and `tools/ui_preview/run --bench` for native timings. Gallery generation needs Pillow and the supplied chrome reference next to the repository. Set `MACHINE_CHROME` to an alternative copy of that reference. Run the font and art scripts in `tools/ui_preview/` from the repository root to rebuild the embedded arrays.

This take has been exercised on Mac and Linux arm64 fixtures. Panel appearance, SPI timing, physical control feel and operation alongside the audio engine still need a test on the MPC.
