# Running Zod in the browser (WebAssembly / Emscripten)

Status: **recon + toolchain set up.** Nothing in the engine or the native build
has changed yet — this is the groundwork and the evidence-based plan for a web
port. The native (SDL3 desktop) build is completely unaffected.

## Why it's feasible
- The engine is plain C++ + SDL3 + CMake; SDL3 compiles to wasm via Emscripten.
- Rendering is already **SDL_Renderer + SDL_Texture** (`zvideo.cpp`), with the
  OpenGL path `#ifndef DISABLE_OPENGL`'d out (we build with `DISABLE_OPENGL`).
  SDL3's renderer targets **WebGL** under Emscripten automatically — this ports
  for free.

## Toolchain (verified)
- Install the SDK once: `git clone https://github.com/emscripten-core/emsdk`,
  then `./emsdk install latest && ./emsdk activate latest`.
- `source ~/emsdk/emsdk_env.sh` puts `emcc` / `emcmake` on PATH.
- Verified working: **emcc 6.0.1**; a trivial `SDL3/SDL.h` program compiled to
  wasm cleanly (the `sdl3` port auto-fetched SDL 3.4.2 and built `libSDL3.a`).

## SDL dependency status under Emscripten (verified via `emcc --show-ports`)
| lib | emscripten port? | plan |
|---|---|---|
| SDL3 | ✅ `--use-port=sdl3` (experimental) | use the port |
| SDL3_ttf | ✅ `--use-port=sdl3_ttf` | use the port |
| **SDL3_image** | ❌ no port (only sdl2_image) | **build from source for wasm** |
| **SDL3_mixer** | ❌ no port (only sdl2_mixer) | **build from source for wasm** (or stub audio first) |

`SDL3_image` / `SDL3_mixer` are the first concrete blocker: the engine includes
their headers (`zvideo.cpp`, `zsound_engine.cpp`, `zsdl*.h`, …), so even
*compiling* needs them present. Build each from source against the port's SDL3
(`emcmake cmake` on the upstream release, decoders trimmed to what we use — PNG
for image; for mixer, audio can be stubbed for a first pass since the dedicated
server proves the game logic doesn't need it).

## The real obstacles (in order of effort)
1. **SDL3_image / SDL3_mixer for wasm** — build from source (above). Required to compile/link.
2. **Networking (the big one).** The engine is client/server over **TCP sockets
   even in singleplayer** (`main.cpp` spawns a server thread + client over
   `127.0.0.1`). Browsers have no raw TCP; Emscripten emulates BSD sockets over
   **WebSockets** via a relay — there is no in-process loopback.
   - Singleplayer in-browser needs an **in-process transport shim** that passes
     packets between the embedded server and client without real sockets.
   - Multiplayer in-browser needs the dedicated server reachable over
     **WebSocket** (a `ws→tcp` gateway in front of `zod -d`, or the server
     speaking WS). Dovetails with the dockerized orchestrator.
3. **Blocking main loop.** `ZPlayer::Run()` is a `while(allow_run)` loop; the
   browser needs to yield each frame — `emscripten_set_main_loop`, or compile
   with `-sASYNCIFY` (quick first pass, some size/speed cost).
4. **Threads.** The server runs on an `SDL_CreateThread`; under Emscripten that
   maps to pthreads (Web Workers + SharedArrayBuffer, needs COOP/COEP headers).
   The in-process shim could let singleplayer run single-threaded and sidestep this.
5. **Assets.** ~90 MB under `assets/`. `--preload-file` packs a `.data` blob; for
   the web client that download wants trimming/compression/lazy-loading (the
   server-side trim we did for the Docker image doesn't apply — the client needs
   the graphics).

## Phased plan (each phase independently shippable, native build untouched)
1. **Foundation (this doc).** Toolchain installed; deps + blockers mapped.
2. **It compiles.** Add a guarded `if(EMSCRIPTEN)` CMake branch (sdl3 + sdl3_ttf
   ports; sdl3_image/mixer from source) + `build-wasm.sh`. Goal: the engine links
   to wasm (networking stubbed/non-functional).
3. **It renders.** `-sASYNCIFY`, preload a minimal asset set, get the main
   menu / a static scene drawing in a browser. Proves the pipeline end-to-end.
4. **Singleplayer.** In-process transport shim → play vs bots in-browser.
5. **Multiplayer.** `ws→tcp` gateway in front of the dedicated server.

## Build entry point
`build-wasm.sh` (added alongside this doc) is the scaffold for phase 2 — it
sources emsdk and runs `emcmake`. It will only succeed once phase 2's CMake
branch + the from-source SDL3_image/mixer land; until then it documents the
exact invocation.
