##
## zappy-gui — GDExtension build
##
## Builds src/ into a shared library that Godot loads via bin/zappy_gui.gdextension.
## godot-cpp is fetched and built from source, pinned to a tag matching the Godot
## editor version (see the GIT_TAG below — bump it when the project upgrades Godot).
##

cmake_minimum_required(VERSION 3.25)
project(zappy_gui_extension LANGUAGES CXX)

# Guard: prevent "cmake ." from polluting the source tree with build artifacts.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not supported. "
        "Use: cmake --preset <linux-debug|linux-release|windows-debug|windows-release>")
endif()

# ─── C++20 ────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json so clangd/LSP can find includes and flags.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# build/ lives inside the Godot project (res://). Without this, Godot's
# filesystem scanner tries to import the thousands of .obj intermediate files
# produced by the compiler as Wavefront OBJ meshes, spamming import errors.
# .gdignore tells the editor to skip this directory entirely. Recreated here
# because a clean rebuild (rm -rf build) would otherwise delete it.
file(WRITE "${CMAKE_BINARY_DIR}/.gdignore" "")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# ─── godot-cpp ────────────────────────────────────────────────────────────────
# Pinned to the latest tagged release at the time of writing. The Godot editor
# (4.6) is newer than this tag (4.5), but GDExtension is forward-compatible:
# an extension built against an older API works in a newer editor as long as
# compatibility_minimum in the .gdextension file is satisfied. Bump GIT_TAG to
# a 4.6 tag once godot-cpp publishes one.
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

FetchContent_Declare(godot-cpp
    GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
    GIT_TAG        godot-4.5-stable
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(godot-cpp)

# ─── zappy_gui GDExtension library ─────────────────────────────────────────────
add_library(zappy_gui SHARED
    src/register_types.cpp
    src/ZappyWorld.cpp
    src/network/protocol_parser.cpp
    src/network/zappy_connection.cpp
    src/world/world_state.cpp
    src/world/map_terrain.cpp
    src/world/tile_markers.cpp
    src/entities/player_entity.cpp
    src/entities/egg_entity.cpp
    src/entities/entity_manager.cpp)

target_include_directories(zappy_gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(zappy_gui PRIVATE godot-cpp)

target_compile_options(zappy_gui PRIVATE
    -Wall
    -Wextra
    -Wpedantic)

# ─── Output naming ──────────────────────────────────────────────────────────
# Produces bin/libzappy_gui.<platform>.<config>.x86_64.{so,dll}, matching the
# per-platform entries in bin/zappy_gui.gdextension. CMAKE_SYSTEM_NAME is "Linux"
# for a native build and "Windows" when configured with cmake/toolchain-mingw64.cmake.
string(TOLOWER "${CMAKE_SYSTEM_NAME}" ZAPPY_PLATFORM)
string(TOLOWER "${CMAKE_BUILD_TYPE}" ZAPPY_CONFIG)

set_target_properties(zappy_gui PROPERTIES
    PREFIX               "lib"
    OUTPUT_NAME          "zappy_gui.${ZAPPY_PLATFORM}.${ZAPPY_CONFIG}.x86_64"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# ─── Documentation target ────────────────────────────────────────────────────
# Optional: only registered if Doxygen is installed on the host machine.
# Build with: cmake --build <build-dir> --target docs
# Output:     docs/doxygen/html/index.html  (build artifact — not committed)
find_package(Doxygen OPTIONAL_COMPONENTS dot)
if(DOXYGEN_FOUND)
    # doxygen-awesome-css — clean HTML theme for Doxygen output.
    # Fetched at configure time just like godot-cpp above.
    FetchContent_Declare(doxygenawesome
        GIT_REPOSITORY https://github.com/jothepro/doxygen-awesome-css.git
        GIT_TAG        v2.3.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(doxygenawesome)

    # Substitute @placeholders@ in Doxyfile.in with the real paths resolved by CMake.
    set(DOXYGEN_AWESOME_CSS_DIR ${doxygenawesome_SOURCE_DIR})
    if(DOXYGEN_DOT_FOUND)
        set(DOXYGEN_HAVE_DOT YES)
    else()
        set(DOXYGEN_HAVE_DOT NO)
    endif()
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
        ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        @ONLY)

    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating HTML documentation → docs/doxygen/html/index.html"
        VERBATIM)
endif()
