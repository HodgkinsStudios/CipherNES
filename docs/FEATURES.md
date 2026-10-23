# CipherNES 1.0 controls and transport

**Maintained by HodgkinsStudios**

| Feature | Default | Details |
| --- | --- | --- |
| Rewind | Hold **R** | Frame-by-frame rewind with captured PCM audio played in reverse. |
| Fast-forward | Hold **T** | Ramps smoothly to **150%** with continuous-phase audio resampling. |
| Display mode | **Escape** | Windowed → aspect-correct fullscreen → stretched fullscreen → windowed. Not remappable. |
| Controller mapping | Configure → Controller Input | Keyboard, gamepad buttons, hats, axes and triggers; R/T can be remapped. |
| Record inputs | TAS menu | Inputs from discarded branches are removed after rewind or loading an earlier state. |
| Save/load state | Save slots | New states retain their exact TAS input prefix and video image. |
| Open ROM/recording | Menus / drag and drop | Native OS file chooser where available. |

Rewind history is bounded by **1,800 NTSC frames** and **256 MiB**. Each frame has a complete machine state, compressed framebuffer delta, captured PCM and input cursor. Reverse audio uses the actual captured samples with short fades at direction changes. Fast-forward never skips emulated CPU/PPU/APU frames and is **disabled during active TAS recording**. Forward-only AVI capture disables rewind and fast-forward.

The `.ci` TAS input format remains version 1. New `.nss` files have versioned `CNSSM02!` host metadata for TAS input prefixes and captured video; the **core state version remains 6**. Old save files can load outside active TAS capture. To avoid desynchronized movies, older states without TAS input history cannot load while recording.

Ubuntu uses `zenity` or `kdialog` for the system picker; macOS uses the native chooser panel, and Windows uses standard OS dialogs. Flatpak's sandbox native-file-picker integration is not yet fully certified; see `packaging/flatpak/README.md`.
