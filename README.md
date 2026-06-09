# Zod

A modernized build of the **Zod Engine** — the open-source remake of the 1996
Bitmap Brothers real-time strategy game *Z* — brought up to date to build and
play cleanly on current macOS (Apple Silicon) and Linux.

This is a fork of the Zod Engine by Michael Bok / Nighsoft
(<http://zod.sourceforge.net/>), originally released under the GPLv3. All the
original gameplay is intact; this fork focuses on getting it building and
running well on modern systems.

## What's new in this fork

- **Ported from SDL 1.2 to SDL 2** (via a small `sdl12_compat` shim).
- **Fixed a family of rendering regressions** the SDL2 port had introduced
  (sprites that silently failed to draw: zone flags, the resume banner, rock /
  grenade animations, vehicles, portrait overlays — all the same
  negative-source-rect clipping bug).
- **Modern scaled-framebuffer rendering**: the game renders to a logical
  framebuffer and is GPU-scaled crisp to a large, HiDPI/Retina-aware window
  (no more tiny, OS-blurred output). Smooth scaling by default; optional knobs
  via `ZOD_FILTER`, `ZOD_INTEGER`, `ZOD_SCANLINES`.
- **Two-finger trackpad panning** of the map.
- **Dropped the MySQL dependency** (it was only for the old online
  master-server); the binary now needs only SDL2 + extensions.
- **Installable**: finds its data when installed to a prefix, and launches
  straight into the single-player campaign with no arguments.
- **CMake build** + this README + CI.

## Install

### macOS — Homebrew (easiest)

```sh
brew install fenio/tap/zod
zod
```

This pulls a prebuilt binary (a bottle) when one is available for your arch,
otherwise it builds from source. `zod` with no arguments launches the
single-player campaign. (Equivalently: `brew tap fenio/tap && brew install zod`.)

### Windows — Scoop (experimental)

```powershell
scoop bucket add fenio https://github.com/fenio/scoop-bucket
scoop install zod
zod
```

Or grab the `zod-windows-x86_64.zip` straight from the
[Releases](https://github.com/fenio/zod/releases) page and run `zod.exe`.

> ⚠️ **The Windows build is experimental and not yet runtime-tested** — it
> compiles and links in CI, but the game hasn't been verified actually running
> on Windows. Reports welcome.

### From source (macOS / Linux)

See **Build & run** below.

## Build & run

Dependencies: a C++ compiler, CMake, pkg-config, and SDL2 with the image,
mixer, and ttf extensions.

**macOS (Homebrew):**
```sh
brew install cmake pkg-config sdl2 sdl2_image sdl2_mixer sdl2_ttf
```

**Debian / Ubuntu:**
```sh
sudo apt install build-essential cmake pkg-config \
  libsdl2-dev libsdl2-image-dev libsdl2-mixer-dev libsdl2-ttf-dev
```

**Fedora:**
```sh
sudo dnf install gcc-c++ cmake pkgconf-pkg-config \
  SDL2-devel SDL2_image-devel SDL2_mixer-devel SDL2_ttf-devel
```

Then:
```sh
cmake -S . -B build
cmake --build build -j
./build/zod            # run in place (assets are found next to the binary)
```

Install system-wide (optional):
```sh
sudo cmake --install build           # -> <prefix>/bin/zod, <prefix>/share/zod
zod                                  # plays the campaign
```

## Playing

Running with no arguments starts a local single-player campaign (you on red vs.
a blue bot, all 34 levels in order). Useful flags:

| Flag | Meaning |
|------|---------|
| `-l map_list.txt` | play the campaign map list in order (the default) |
| `-m FILE.map` | play a single map |
| `-t TEAM` / `-b TEAM` | your team / add a bot on a team (`red`, `blue`) |
| `-w` | windowed | 
| `-r WxH` | logical resolution (e.g. `800x600`; smaller = bigger sprites) |
| `-s` / `-u` | no sound / no music |

**Controls:** left-click selects, right-click moves/attacks; capture a zone by
walking a unit onto its flag; crew a neutral (grey) vehicle by sending a grunt
onto it; two-finger scroll (or arrow keys) pans the map. The game starts
**paused** — click the centre "Click to Start / Resume" banner to begin.

## License

GPLv3 — see [`LICENSE`](LICENSE). The Zod Engine is © its original authors
(Michael Bok / Nighsoft); this fork is distributed under the same license.
