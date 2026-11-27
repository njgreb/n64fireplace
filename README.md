# HEARTH 64

A cozy low-poly 3D Nintendo 64 fireplace screensaver running at a **locked 60 FPS** via **Tiny3D (`libdragon` + `t3d`)**, built for a real **CRT TV**, **stock 4 MB N64** (`240p 16-bit`), and **8 MB Expansion Pak** consoles (`640×480i` Hi-Res default + runtime-toggleable `240p 32-bit` true color; auto-detects NTSC 60Hz & PAL 50Hz).

> **Just want to play it?** Grab the ready-to-run `hearth64.z64` ROM from the [Latest Release](https://github.com/njgreb/n64fireplace/releases/latest) and copy it onto your EverDrive-64, SummerCart64, or open it in `ares`!

## Features
- **Four-Season RTC Calendar (`Spring Blossom`, `Summer Cabin`, `Thanksgiving`, `Cozy Christmas`)**:
  - **RTC Auto-Detection**: Reads the flashcart / emulator Real-Time Clock on boot (Mar–May Spring Blossom, Jun–Aug Summer Cabin, Sep–Nov Thanksgiving, Dec–Feb Cozy Christmas).
  - **3D Mantle Snowglobe Transition**: Switching seasons smoothly glides the camera to the mantle snowglobe, shakes up a season-themed flurry with sleigh bells & chimes, and reveals the transformed cabin.
  - **Full Seasonal Cabin Makeovers**: Dedicated `64×32` mantle paintings, 3D mantle & hearth decor (cherry blossoms & flower vase in Spring; wildflower bunting, lanterns, citronella mug & s'mores tray in Summer; autumn garland & pumpkin in Thanksgiving; fairy-light garland, knit stockings, lit Christmas tree, presents & cocoa in Christmas), plus matching seasonal music & ambient vibes.
- **60 FPS Tiny3D Hardware Graphics & Alive Cabin Window**:
  - Native Tiny3D RSP/RDP rendering with a 3-light rig (warm fire key, cool cabin-window moonlight, warm hearth under-bounce), animated hearth & rug firelight pools, layered 3D flames, curling wall-bouncing embers, rising chimney flue smoke, and interactive 3D log tossing.
  - Alive cabin window featuring a glowing moon & halo, drifting clouds, shooting stars, distant storm lightning, parallax mountains & pines, falling rain/snow, Spring cherry-blossom petals, Summer fireflies, and Christmas corner frost.
  - **Expansion Pak Video Toggle**: Switch seamlessly at runtime in the START menu between `480i Hi-Res` (`640×480` 16-bit AA) and `240p 32-bit` (`320×240` true color) on 8 MB consoles; stock 4 MB consoles run `240p 16-bit` automatically.
- **Interactive Fireside Toybox & Seasonal Hearth Visitors**:
  - **3D Marshmallow Roasting (`Z + A`)**: Roast a marshmallow on a skewer right in the flames (raw → golden perfection ding → charred/on-fire; press `A` to blow it out!).
  - **Playful Hearth Visitors & Seasonal `A + B` Specials**:
    - **Spring Blossom**: Strutting **Turkey** (`A + B` → Spring Victory Strut!), hopping **Frog Wizard** (`A + B` → Fresh Berry Tart!), and **Blossom-Ear Tabby Cat** (`A + B` → Spring Catnap!).
    - **Summer Cabin**: Strutting **Turkey** (`A + B` → Summer Hoedown Strut!), **Frog Wizard** (`A + B` → S'mores & Watermelon Platter!), and **Sunglasses Tabby Cat** (`A + B` → Cool Summer Nap!).
    - **Thanksgiving**: Strutting **Turkey** (`A + B` → Roast Dinner!), **Frog Wizard** (`A + B` → Pumpkin Pie!), and **Orange Tabby Cat** (`A + B` → Fireside Nap!).
    - **Cozy Christmas**: SM64-style waddling **Penguin** (`A + B` → Belly Slide & Mini Snowman!), **Santa-Hat Frog** (`A + B` → Santa's Milk & Cookies!), and **Santa-Hat Tabby Cat** (`A + B` → Cozy Christmas Nap!).
- **Cozy Audio Mixer**:
  - Seamless warm hearth rumble & fluttering flame roar
  - 6 randomized wood crackle/pop one-shots with organic pitch & stereo pan variation
  - Toggleable seasonal window weather (**Cabin Rain** or **Winter Snowfall**)
  - Toggleable dreamy 90s N64 **Ambient Synth** or **Winter Chimes** chord progression
- **CRT Burn-In Protection**: Slow Lissajous orbital drift + smooth auto-cycling across 5 camera angles (including **Snowglobe Macro**) so no high-contrast edges stay static on CRT phosphor, plus an optional Night Dimmer and auto-hiding HUD.

## N64 Controller Map

| Button | Action |
|---|---|
| **START** | Toggle interactive **Settings & Controls Menu** (`D-Up/Down` pick, `D-Left/Right` or `A` edit, `B` exit — includes `480i Hi-Res` / `240p 32-bit` Video toggle on Expansion Pak) |
| **Z + L / R** | Quick-switch **Season** anytime (triggers the 3D Mantle Snowglobe transition!) |
| **Z + A** | Toggle **3D Marshmallow Roasting** skewer over the flames (`A` blows out a flaming mallow) |
| **A** | Stoke the fire (flame flare, spark burst & whoosh — or blow out flaming marshmallow) |
| **B** | Toss a fresh 3D log onto the fire |
| **A + B** *(while visitor is out)* | Seasonal Easter Eggs! (Turkey → Strut Dance / Roast Dinner in Thanksgiving, Penguin → Belly Slide & Mini Snowman, Frog → Seasonal Treat, Cat → Seasonal Fireside Nap!) |
| **D-Pad Up / Down** | Adjust fire heat / intensity |
| **D-Pad Left / Right** | Cycle & summon seasonal hearth visitors (**Turkey / Penguin**, **Frog**, **Tabby Cat**) |
| **L / R** | Cycle flame color mood *(Classic Hearth, Driftwood Magic, Wildfire Emerald, Twilight Amethyst, Late-Night Amber)* |
| **C-Up** | Toggle seasonal window weather (**Cabin Rain** / **Winter Snowfall**) |
| **C-Down** | Toggle cozy 90s N64 music (**Ambient Synth** / **Winter Chimes**) |
| **C-Left / C-Right** | Cycle camera angle *(Auto-Cycle, Hearth & Mantle, Parlor 3/4 Wide, Fireside Rug, Close-Up Coals, Snowglobe Macro)* |
| **Analog Stick** | Gently orbit the camera manually (auto-resumes CRT drift after idle) |
| **Z** | Cycle CRT Burn-In Drift *(Gentle / Wide / Off)* |
| **L + R** | Toggle CRT Night Dimmer |

## Running on Real N64 Hardware
Copy `hearth64.z64` onto your **EverDrive-64**, **SummerCart64**, or other N64 flashcart SD card and boot it on any NTSC or PAL Nintendo 64! Runs on stock **4 MB** consoles (`240p 16-bit`) and automatically unlocks **8 MB Expansion Pak** modes (`640×480i` Hi-Res and `240p 32-bit`).

## Building & Previewing Locally (macOS)
```bash
# Build hearth64.z64 using libdragon + Tiny3D (via Colima + Docker)
./build.sh

# Build and launch immediately in ares emulator
./build.sh run

# Regenerate procedural textures & audio, then build
./build.sh regen
```
