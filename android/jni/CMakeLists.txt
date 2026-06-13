# Top-level Android JNI build. The CI workflow lays SDL3 and its satellite
# libraries next to this file (SDL/, SDL_image/, SDL_ttf/, SDL_mixer/) and our
# engine sources under zod/. Each SDL lib is built from source (vendored deps)
# so the APK is self-contained.
cmake_minimum_required(VERSION 3.16)
project(zod_android CXX C)

# Build the satellite libraries' bundled dependencies (libpng, freetype,
# harfbuzz, decoders) rather than expecting them on the system.
set(SDLIMAGE_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_VENDORED   ON CACHE BOOL "" FORCE)
set(SDLMIXER_VENDORED ON CACHE BOOL "" FORCE)

# Trim the satellite libs to what the engine actually uses, to keep the build
# tractable: BMP/PNG images, TrueType fonts, and (for the wrapped audio) a
# couple of decoders.
set(SDLIMAGE_AVIF   OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JXL    OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TIF    OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_WEBP   OFF CACHE BOOL "" FORCE)

add_subdirectory(SDL)
add_subdirectory(SDL_image)
add_subdirectory(SDL_ttf)
add_subdirectory(SDL_mixer)
add_subdirectory(zodapp)
