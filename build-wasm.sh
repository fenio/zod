#!/usr/bin/env bash
# Build Zod for the browser (WebAssembly) via Emscripten.
#
# STATUS: scaffold for phase 2 of docs/WASM.md. It will not produce a working
# build until (a) CMakeLists gains an `if(EMSCRIPTEN)` branch and (b) SDL3_image
# and SDL3_mixer are built from source for wasm (no emscripten port exists). It
# documents the exact invocation and fails loudly with a pointer until then.
#
# Prereqs: emsdk installed + activated (see docs/WASM.md).
#   export EMSDK=~/emsdk   # or wherever you cloned it
set -euo pipefail

EMSDK="${EMSDK:-$HOME/emsdk}"
if [ ! -f "$EMSDK/emsdk_env.sh" ]; then
  echo "emsdk not found at $EMSDK. Install it (see docs/WASM.md) or set EMSDK=..." >&2
  exit 1
fi
# shellcheck disable=SC1091
source "$EMSDK/emsdk_env.sh" >/dev/null 2>&1

if ! grep -q "EMSCRIPTEN" CMakeLists.txt 2>/dev/null; then
  echo "CMakeLists.txt has no EMSCRIPTEN branch yet (phase 2 of docs/WASM.md)." >&2
  echo "emcc is ready ($(emcc --version | head -1)); the build wiring is the next step." >&2
  exit 2
fi

BUILD_DIR="${BUILD_DIR:-build-wasm}"
emcmake cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
emmake cmake --build "$BUILD_DIR" --target zod --parallel 4
echo "done -> $BUILD_DIR (serve zod.html with COOP/COEP headers)"
