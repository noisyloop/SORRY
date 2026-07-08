cmake_minimum_required(VERSION 3.21)
project(sorry CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Homebrew on Apple Silicon keeps everything under /opt/homebrew.
if(APPLE)
  list(APPEND CMAKE_PREFIX_PATH /opt/homebrew)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_TTF REQUIRED IMPORTED_TARGET SDL2_ttf)
pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)
pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET SDL2_mixer)

add_executable(sorry
  src/main.cpp
  src/Game.cpp
  src/Renderer.cpp
  src/Input.cpp
  src/Room.cpp
  src/RoomLoader.cpp
  src/Player.cpp
  src/Entity.cpp
  src/NPC.cpp
  src/Dialogue.cpp
  src/Inventory.cpp
  src/SaveGame.cpp
  src/Audio.cpp
  src/ChiptuneSynth.cpp
  src/Assets.cpp
)

target_link_libraries(sorry PRIVATE
  PkgConfig::SDL2
  PkgConfig::SDL2_TTF
  PkgConfig::SDL2_IMAGE
  PkgConfig::SDL2_MIXER
)

target_compile_options(sorry PRIVATE -Wall -Wextra)
