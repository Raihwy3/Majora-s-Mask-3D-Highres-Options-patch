# MM3D Display Options (v44 — EUR & USA)

An in-game **Page 3** addition to *The Legend of Zelda: Majora's Mask 3D* Options menu for Luma3DS (New 3DS / New 3DS XL), supporting both **EUR** and **USA** versions alongside **Project Restoration**.

## Features

- **In-game Options Page 3** (using native game layout, font, and slide/fade animations):
  - **Display mode**: `Original`, `Native 2D`, `High-res 2D` (800×240 wide), `Supersample 2D` (800×480 → 400×240), and `Supersample 3D` (400×480 per eye).
  - **3D strength**: Adjustable stereoscopic depth scale.
  - **Look speed**: `Very low (50%)`, `Low (75%)`, `Normal (100%)`, `Higher (125%)`, `High (150%)`, `Very high (175%)`, `Maximum (200%)` (applied post-normalization so positive/negative axes stay symmetric).
- **In-game `START` access**: Pressing `START` during gameplay opens Options and pauses game time.
- **Persistent SD settings**: Saved automatically in a versioned, CRC-checked two-file journal (`display-settings-a.bin` / `display-settings-b.bin`) under `/luma/titles/<TITLE_ID>/`.
- **Project Restoration compatible**: Preserves `code.bps` hooks when installed alongside Project Restoration (Restoration is not bundled).

## Supported Versions & Releases

Download the ZIP for your region from the [Releases](https://github.com/Raihwy3/Majora-s-Mask-3D-Highres-Options-patch/releases) page:

| Region | Title ID | Release Archive |
|---|---|---|
| **EUR** | `0004000000125600` | `mm3d-display-options-eur-v44.zip` |
| **USA** | `0004000000125500` | `mm3d-display-options-usa-v44.zip` |

> **Note:** Both EUR and USA builds require the **1.1 game update** installed, and work with both the stock game and **Project Restoration** (`code.bps`).

## Installation (Luma3DS)

1. Download the ZIP matching your region and game version from Releases.
2. Merge the `luma/` folder onto the root of your 3DS SD card, replacing:
   - `luma/titles/<TITLE_ID>/code.ips`
   - `luma/titles/<TITLE_ID>/exheader.bin`
   - `luma/titles/<TITLE_ID>/romfs/layout/Joker.Main.Play.Option.gar`
3. Keep your existing `code.bps` (if using Project Restoration) and any other RomFS files.
4. Ensure **Enable game patching** is turned on in the Luma3DS configuration menu.

## Controls (Page 3)

- **`R` / `L`** (or bottom tabs): Switch between Page 1, Page 2, and Page 3.
- **D-Pad / Circle Pad Up & Down** (or touch): Select row.
- **D-Pad / Circle Pad Left & Right** (or touch arrows): Change value (`3D strength` and `Look speed` apply immediately).
- **`A`** (or tap Display value): Apply `Display` mode (also retries a failed SD save).
- **`B`** (or Cancel button): Return to Page 2.
- **`START`** (during gameplay): Open Options and pause game time.

## Repository Structure

```text
.
├── eur/          # EUR (v44) C/ASM source, Makefile, build scripts, and tests
├── usa/          # USA (v44) C/ASM source, Makefile, build scripts, and tests
└── docs/         # Design notes (DESIGN.md) and native hook documentation (NATIVE_HOOKS.md)
```

## Building & Testing

### Requirements
- `devkitARM` (`/opt/devkitpro/devkitARM/bin/arm-none-eabi-gcc` and `objcopy`)
- Host C compiler (`cc`) with AddressSanitizer/UBSan support
- Python 3
- User-supplied decrypted game files (`*.dec.code`, `exheader.bin`, `Joker.Main.Play.Option.gar`) for full IPS/RomFS packaging (no copyrighted Nintendo game binaries are included in this repository).

### Run Host & ARM Verification Tests

```bash
# EUR v44
cd eur
make check arm native check-native check-text check-render check-menu check-style check-depth check-scanout check-resolve check-settings check-motion

# USA v44
cd usa
make check arm native check-native check-text check-render check-menu check-style check-depth check-scanout check-resolve check-settings check-motion
```

---

*Disclosure: AI tools were used during the development and organization of this project.*
