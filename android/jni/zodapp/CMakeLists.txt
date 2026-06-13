# The engine itself, built as libmain.so (the name SDLActivity loads by default).
# The CI workflow copies the repo's src/ tree to ./zod next to this file.
cmake_minimum_required(VERSION 3.16)
project(zodapp CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC "${CMAKE_CURRENT_SOURCE_DIR}/zod")

file(GLOB ZOD_SRCS CONFIGURE_DEPENDS "${SRC}/*.cpp")
# mirror the desktop build's exclusions (editors / utilities / Windows-only)
list(REMOVE_ITEM ZOD_SRCS
  "${SRC}/map_editor.cpp"
  "${SRC}/map_merger.cpp"
  "${SRC}/map_gen_main.cpp"
  "${SRC}/zod_tests.cpp"
  "${SRC}/tile_info_editor.cpp"
  "${SRC}/win_tray.cpp"
  "${SRC}/zgfile.cpp")

# SDLActivity loads "libmain.so" by default, so the target must be named main.
add_library(main SHARED ${ZOD_SRCS})

target_include_directories(main PRIVATE ${SRC})

target_compile_definitions(main PRIVATE
  DISABLE_OPENGL
  DISABLE_REGCHECK
  DISABLE_MYSQL
  ZOD_VERSION="android-spike"
  # assets are extracted to the app's files dir at runtime (set via chdir),
  # so no compiled-in datadir
  ZOD_DATADIR="")

# The NDK enables -Werror=format-security by default (desktop builds don't);
# the vintage code has non-literal printf format strings that are benign here.
target_compile_options(main PRIVATE -Wno-deprecated -Wno-write-strings -Wno-error=format-security)

target_link_libraries(main PRIVATE
  SDL3::SDL3
  SDL3_image::SDL3_image
  SDL3_ttf::SDL3_ttf
  SDL3_mixer::SDL3_mixer)
