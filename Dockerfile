# syntax=docker/dockerfile:1
###############################################################################
# Zod dedicated-server deployment image.
#
# Bundles the match orchestrator + the headless `zod -d` engine + game data in
# one image. The orchestrator is the entrypoint; it spawns one `zod -d` per
# match (process-per-match) on ports 2300-2399 and hands clients a host:port.
#
# `zod -d` is genuinely headless: it links SDL3 but never opens a window or audio
# device (SDL_Init(VIDEO|AUDIO) lives only in the client), so no X server / xvfb
# is needed in the image.
#
#   docker build -t zod-server .
#   docker run --rm -p 8080:8080 -p 2300-2399:2300-2399 \
#     -e ADVERTISE_HOST=<address-clients-can-reach> zod-server
#
# Deploying to an x86_64 VPS from an arm64 machine? build with:
#   docker buildx build --platform linux/amd64 -t zod-server .
###############################################################################

# --- Stage 1: build the Go orchestrator (static, stdlib only) ----------------
FROM golang:1 AS gobuild
WORKDIR /src
COPY orchestrator/ ./
# stdlib only - no module downloads needed. Static binary so it runs on any base.
RUN CGO_ENABLED=0 go build -trimpath -ldflags="-s -w" -o /out/orchestrator .

# --- Stage 2: build the C++ engine (zod) against SDL3 ------------------------
FROM ubuntu:26.04 AS cppbuild
ARG JOBS=4
ARG ZOD_VERSION=docker
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential cmake pkg-config curl ca-certificates \
      libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev \
 && rm -rf /var/lib/apt/lists/*
# 26.04's libsdl3-mixer-dev (the new MIX_ API) hasn't reached the release pocket
# yet, so build SDL3_mixer 3.2.4 from source against the apt-provided SDL3 (same
# as CI). Decoders off - the engine only needs the MIX_ symbols to link.
# A second install into a DESTDIR stages the .so for an arch-agnostic runtime copy.
RUN curl -fsSL -o /tmp/mixer.tar.gz \
      https://github.com/libsdl-org/SDL_mixer/releases/download/release-3.2.4/SDL3_mixer-3.2.4.tar.gz \
 && mkdir -p /tmp/mixer && tar -xzf /tmp/mixer.tar.gz -C /tmp/mixer --strip-components=1 \
 && cmake -S /tmp/mixer -B /tmp/mixer/build \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
      -DBUILD_SHARED_LIBS=ON -DSDLMIXER_VENDORED=OFF -DSDLMIXER_SAMPLES=OFF \
      -DSDLMIXER_FLAC=OFF -DSDLMIXER_GME=OFF -DSDLMIXER_MOD=OFF \
      -DSDLMIXER_MP3=OFF -DSDLMIXER_MIDI=OFF -DSDLMIXER_OPUS=OFF \
      -DSDLMIXER_VORBIS=OFF -DSDLMIXER_WAVPACK=OFF \
 && cmake --build /tmp/mixer/build --parallel ${JOBS} \
 && cmake --install /tmp/mixer/build \
 && ldconfig
WORKDIR /src
COPY CMakeLists.txt ./
COPY src/ ./src/
# build only the engine target (skip tests / mapgen)
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DZOD_VERSION="${ZOD_VERSION}" \
 && cmake --build build --target zod --parallel ${JOBS}
# Stage exactly what the runtime needs into /rootfs, so the final image can be
# FROM scratch (no base OS at all). The headless server never inits video/audio,
# so the X11/Wayland/Mesa/audio backends SDL would dlopen are never needed - we
# ship only the binary's ldd closure (~3 MB), our own glibc + loader included.
#   - non-loader libs -> /rootfs/applibs (flat, arch-agnostic, found via LD_LIBRARY_PATH)
#   - the ELF interpreter -> its exact PT_INTERP path (e.g. /lib64/ld-linux-*),
#     which the kernel hard-codes, so it must stay where ldd reports it
#   - a writable /tmp (1777) for the orchestrator's status sockets + match logs
RUN set -eu; mkdir -p /rootfs/applibs /rootfs/tmp; chmod 1777 /rootfs/tmp; \
    ldd /src/build/zod | awk '/=> \//{print $3}' | sort -u | while IFS= read -r lib; do \
      [ -f "$lib" ] && cp -L "$lib" /rootfs/applibs/; \
    done; \
    interp=$(ldd /src/build/zod | awk '/ld-linux|ld\.so/{print $1; exit}'); \
    mkdir -p "/rootfs$(dirname "$interp")"; cp -L "$interp" "/rootfs$interp"

# --- Stage 3: runtime --------------------------------------------------------
# scratch: no base OS. Everything the two binaries need is copied in explicitly.
FROM scratch AS runtime
COPY --from=cppbuild /rootfs/ /
WORKDIR /app
COPY --from=gobuild  /out/orchestrator  /app/orchestrator
COPY --from=cppbuild /src/build/zod      /app/build/zod
# Game data the dedicated server actually reads (verified via strace): all of
# maps/ plus the planet tileinfo. The ~90 MB of client graphics under assets/
# are never opened by `zod -d`, so they're left out.
COPY maps/            /app/maps/
COPY assets/planets/  /app/assets/planets/
COPY map_list.txt default_settings.txt /app/

# Orchestrator config - override at run time. ADVERTISE_HOST MUST be an address
# clients can actually reach; 127.0.0.1 only works for same-host testing.
# LD_LIBRARY_PATH points the ELF loader at our flat lib dir (no ld.so.cache here).
ENV LD_LIBRARY_PATH=/applibs \
    ZOD_DIR=/app \
    ZOD_BIN=/app/build/zod \
    ORCH_ADDR=:8080 \
    ADVERTISE_HOST=127.0.0.1 \
    EMPTY_EXIT_SECS=300

EXPOSE 8080
EXPOSE 2300-2399/tcp
ENTRYPOINT ["/app/orchestrator"]
