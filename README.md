# HEARTH 64

A cozy low-poly 3D Nintendo 64 fireplace screensaver designed for a real **CRT TV** and **stock 4MB N64** (auto-detects NTSC 60Hz & PAL 50Hz via `libdragon`).

> **Just want to play it?** Grab the ready-to-run `hearth64.z64` ROM from the [Latest Release](https://github.com/njgreb/n64fireplace/releases/latest) and copy it onto your EverDrive-64, SummerCart64, or open it in `ares`!

## Features
- **Classic 90s N64 Low-Poly 3D Hearth**: Textured rustic stone chimney, soot-kissed angled firebrick firebox, heavy oak mantle with flickering beeswax candles, steaming mug, starry alpine night painting, festive mini pumpkins, wrought-iron grate & andirons, braided hearth rug, stacked firewood nook, and a starry cabin window.
- **Dynamic Flickering Firelight**: Real-time multi-frequency point lighting illuminates the stone, logs, rug, and cabin walls—and shifts hue with your chosen flame mood.
- **Layered 3D Flames, Coals & Embers**: Smooth IA8-blended 3D flame tongues, glowing ash/coal mound, swirling sparks, and interactive 3D log tossing.
- **Playful Hearth Visitors & Secret Easter Eggs**: Random appearances by a strutting **Thanksgiving Turkey**, a hopping **Frog**, and a cozy **Orange Tabby Cat** — each with its own sound effect and secret **A + B** Easter egg!
- **Cozy Audio Mixer**:
  - Seamless warm hearth rumble & fluttering flame roar
  - 6 randomized wood crackle/pop one-shots with organic pitch & stereo pan variation
  - Toggleable muffled **Cabin Rain** (with animated rain streaks outside the window!)
  - Toggleable dreamy 90s N64 **Ambient Synth** chord progression
- **CRT Burn-In Protection**: Slow Lissajous orbital drift + smooth auto-cycling across 4 camera angles so no high-contrast edges stay static on CRT phosphor, plus an optional Night Dimmer and auto-hiding HUD.

## N64 Controller Map

| Button | Action |
|---|---|
| **A** | Stoke the fire (flame flare, spark burst & whoosh) |
| **B** | Toss a fresh 3D log onto the fire |
| **A + B** *(while visitor is out)* | Secret Easter Eggs! (Turkey → Roast Dinner, Frog → Pumpkin Pie, Cat → Fireside Nap!) |
| **D-Pad Up / Down** | Adjust fire heat / intensity |
| **D-Pad Left / Right** | Cycle & summon hearth visitors (**Turkey**, **Frog**, or **Tabby Cat**) |
| **L / R** | Cycle flame color mood *(Classic Hearth, Driftwood Magic, Wildfire Emerald, Twilight Amethyst, Late-Night Amber)* |
| **C-Up** | Toggle cozy Cabin Rain (audio + window rain streaks) |
| **C-Down** | Toggle chill N64 Ambient Synth |
| **C-Left / C-Right** | Cycle camera angle *(Auto-Cycle, Classic Hearth, Fireside Rug, Parlor 3/4, Close-Up Coals)* |
| **Analog Stick** | Gently orbit the camera manually (auto-resumes CRT drift after idle) |
| **Z** | Cycle CRT Burn-In Drift *(Gentle / Wide / Off)* |
| **L + R** | Toggle CRT Night Dimmer |
| **START** | Toggle controls & status overlay *(auto-hides for CRT safety)* |

## Running on Real N64 Hardware
Copy `hearth64.z64` onto your **EverDrive-64**, **SummerCart64**, or other N64 flashcart SD card and boot it on any stock 4MB NTSC or PAL Nintendo 64!

## Building & Previewing Locally (macOS)
```bash
# Build hearth64.z64 using libdragon preview (via Colima + Docker)
./build.sh

# Build and launch immediately in ares emulator
./build.sh run

# Regenerate procedural textures & audio, then build
./build.sh regen
```
