# Design Overview

Adds a third page (**Page 3**) to *Majora's Mask 3D*'s in-game Options menu for **New 3DS / New 3DS XL**, compatible with both the stock game and **Project Restoration** (without bundling Restoration).

## Display Modes

| Mode | Scene Resolution | Output Resolution | Notes |
|---|---|---|---|
| **Original** | Stock | Stock | Untouched game display/slider behavior |
| **Native 2D** | 400×240 | 400×240 | Single view, stereo disabled |
| **High-res 2D** | 800×480 | 800×240 | Wide 2D output, stereo disabled |
| **Supersample 2D** | 800×480 | 400×240 | Downsampled single view |
| **Supersample 3D** | 400×480 per eye | 400×240 per eye | Vertical supersampling with stereo |

## Menu, Controls & Persistence

- **Options Page 3:** Controls **Display**, **3D strength**, and **Look speed** (50%–200% post-normalization C-stick gain), using the game's native layout, font, and slide/fade animations.
- **In-game `START` access:** Pressing `START` during gameplay opens Options and pauses game time, while preserving non-gameplay `START`/`SELECT` behavior and Project Restoration's Notebook hook when applicable.
- **Saved settings:** Persisted independently of game saves in a versioned, CRC-checked two-file journal (`display-settings-a.bin` / `display-settings-b.bin`) under `/luma/titles/<TITLE_ID>/`.
