cmake_minimum_required(VERSION 3.16)
project(zod CXX)

# Pin to C++14: the code is pre-C++11, and on Windows `using namespace std`
# (C++17's std::byte) clashes with <windows.h>'s `byte` type ("reference to
# 'byte' is ambiguous"). C++14 has no std::byte, so the collision disappears.
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to a Release (optimized) build.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --- dependencies (portable across macOS Homebrew and Linux via pkg-config) ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2       REQUIRED IMPORTED_TARGET sdl3)
pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET sdl3-image)
pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET sdl3-mixer)
pkg_check_modules(SDL2_TTF   REQUIRED IMPORTED_TARGET sdl3-ttf)

# --- sources ---
# The game is every *.cpp except the separate map-editor / utility / Windows
# files (mirrors the source selection the hand-written makefile used).
file(GLOB ZOD_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(REMOVE_ITEM ZOD_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/map_editor.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/map_merger.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/map_gen_main.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/zod_tests.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/tile_info_editor.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/win_tray.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/zgfile.cpp")

# Where assets are installed; baked into the binary as a fallback so it can
# find them after `make install` (see chdir_to_data_dir() in main.cpp).
set(ZOD_DATADIR "${CMAKE_INSTALL_PREFIX}/share/zod")

# Release version baked into the binary (startup banner + window title), so
# bug reports unambiguously say what build they are about. From git tags when
# available; override with -DZOD_VERSION=... .
if(NOT ZOD_VERSION)
  execute_process(COMMAND git describe --tags --always --dirty
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE ZOD_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
endif()
if(NOT ZOD_VERSION)
  set(ZOD_VERSION "unknown")
endif()
message(STATUS "zod version: ${ZOD_VERSION}")

add_executable(zod ${ZOD_SRCS})

# Functional tests (pathfinding etc.) - links only the pathfinding sources +
# SDL, with stubbed debug-log symbols. Run with ctest or ./build/zod_tests.
add_executable(zod_tests src/zod_tests.cpp src/zpath_finding.cpp src/zpath_finding_astar.cpp)
target_link_libraries(zod_tests PRIVATE PkgConfig::SDL2 m)
enable_testing()
add_test(NAME zod_tests COMMAND zod_tests)

# Standalone random skirmish map generator — no engine code, no SDL; writes
# .map files the game loads like any hand-made map.
add_executable(zod_mapgen src/map_gen.cpp src/map_gen_main.cpp)

target_compile_definitions(zod PRIVATE
  DISABLE_OPENGL
  DISABLE_REGCHECK
  DISABLE_MYSQL
  ZOD_DATADIR="${ZOD_DATADIR}"
  ZOD_VERSION="${ZOD_VERSION}")

target_compile_options(zod PRIVATE -Wno-deprecated -Wno-write-strings)

target_link_libraries(zod PRIVATE
  PkgConfig::SDL2
  PkgConfig::SDL2_IMAGE
  PkgConfig::SDL2_MIXER
  PkgConfig::SDL2_TTF
  m)

# Windows: the engine's client/server code uses Winsock (WSAStartup, socket, …)
# and the IP Helper API (GetAdaptersInfo).
if(WIN32)
  target_link_libraries(zod PRIVATE ws2_32 iphlpapi)
  # Define WIN32_LEAN_AND_MEAN for EVERY translation unit (before any header,
  # incl. SDL's, pulls in <windows.h>). The socket headers now use <winsock2.h>
  # explicitly; without LEAN, a <windows.h> pulled in elsewhere first would drag
  # in the old <winsock.h> (1.1) and clash with winsock2 ("redefinition"). A
  # per-header #define is too late once a full <windows.h> is already in the TU.
  # (It also keeps <rpcndr.h>'s `byte` out, which future-proofs a move past
  # C++14, where std::byte would otherwise be ambiguous under `using namespace std`.)
  target_compile_definitions(zod PRIVATE WIN32_LEAN_AND_MEAN)
endif()

# --- install (binary -> bin/, game data -> share/zod/) ---
install(TARGETS zod RUNTIME DESTINATION bin)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION share/zod)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/maps"   DESTINATION share/zod)
install(FILES     "${CMAKE_CURRENT_SOURCE_DIR}/map_list.txt" DESTINATION share/zod)
