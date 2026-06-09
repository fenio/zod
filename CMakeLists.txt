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
pkg_check_modules(SDL2       REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)
pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET SDL2_mixer)
pkg_check_modules(SDL2_TTF   REQUIRED IMPORTED_TARGET SDL2_ttf)

# --- sources ---
# The game is every *.cpp except the separate map-editor / utility / Windows
# files (mirrors the source selection the hand-written makefile used).
file(GLOB ZOD_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(REMOVE_ITEM ZOD_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/map_editor.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/map_merger.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/tile_info_editor.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/win_tray.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/zgfile.cpp")

# Where assets are installed; baked into the binary as a fallback so it can
# find them after `make install` (see chdir_to_data_dir() in main.cpp).
set(ZOD_DATADIR "${CMAKE_INSTALL_PREFIX}/share/zod")

add_executable(zod ${ZOD_SRCS})

target_compile_definitions(zod PRIVATE
  DISABLE_OPENGL
  DISABLE_REGCHECK
  DISABLE_MYSQL
  ZOD_DATADIR="${ZOD_DATADIR}")

target_compile_options(zod PRIVATE -Wno-deprecated -Wno-write-strings)

target_link_libraries(zod PRIVATE
  PkgConfig::SDL2
  PkgConfig::SDL2_IMAGE
  PkgConfig::SDL2_MIXER
  PkgConfig::SDL2_TTF
  m)

# Windows: the engine's client/server code uses Winsock (WSAStartup, socket, …)
if(WIN32)
  target_link_libraries(zod PRIVATE ws2_32)
endif()

# --- install (binary -> bin/, game data -> share/zod/) ---
install(TARGETS zod RUNTIME DESTINATION bin)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION share/zod)
install(FILES     "${CMAKE_CURRENT_SOURCE_DIR}/map_list.txt" DESTINATION share/zod)
file(GLOB ZOD_MAPS "${CMAKE_CURRENT_SOURCE_DIR}/*.map")
install(FILES ${ZOD_MAPS} DESTINATION share/zod)
