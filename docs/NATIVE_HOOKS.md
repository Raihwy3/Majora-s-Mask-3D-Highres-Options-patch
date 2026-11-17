# Native Hooks & Integration Notes

Offsets below are **base executable file offsets** (matching the 1.1-compatible base `code.bin` used by both EUR and USA v44).

## Key Game Offsets

| Offset | Role |
|---|---|
| `0x4a1d80` | Options initialization |
| `0x4a2028` | Options cursor navigation |
| `0x4a214c` | Options setting change dispatch |
| `0x4a2584` / `0x4a26e0` | Options touch-input dispatch and widget hit loop |
| `0x4a2ac8` | Options state/input processing |
| `0x4a2f80` | Options page transition helper |
| `0x49ba10` / `0x49ba14` | Native Notebook trigger (patched by Project Restoration with a non-linking branch) |

## Project Restoration Compatibility

- Project Restoration's `code.bps` appends its own code and replaces the Notebook trigger with a non-linking branch (`B`).
- The mod's `START` hook preserves registers/continuation and chains to the installed Restoration target when not opening Options.
- All other patched offsets match stock bytes in both stock and Restoration executables.

## Native UI & Animation Notes

- **Three-tab layout:** `Joker.Main.Play.Option.gar` is extended for 3 tabs while keeping original widget, pane, and animation indices intact.
- **Native animations:** The native animation engine writes evaluated Y (`widget+0x90`) and alpha (`widget+0xd0`) separately from base pose (`+0x40` / `+0x80`). Page 3 detaches the outgoing page's animation player during transitions and restores it on return.
- **Text rendering:** Uses a private `MenuOption` / `MessageWindow` text stream rather than sharing the game's global stream buffer.
