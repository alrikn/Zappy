##
## zappy_gui — CMake build system
##
## This file is the single source of truth for the entire build.
## It:
##   1. Pulls in all third-party libraries via FetchContent (pinned tags).
##   2. Defines the ImGui compiled static library (ImGui has no CMakeLists of its own).
##   3. Compiles all GLSL shaders to SPIR-V via glslc at build time.
##   4. Builds the zappy_gui executable from src/main.cpp and its includes.
##

cmake_minimum_required(VERSION 3.25)
project(zappy_gui LANGUAGES CXX VERSION 0.1.0)

# Guard: prevent "cmake ." from polluting the source tree with build artifacts.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not supported. "
        "Use: cmake --preset debug")
endif()

# ─── C++20 ────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)          # Use -std=c++20, not -std=gnu++20

# Export compile_commands.json so clangd/LSP can find includes and flags.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ─── System packages ──────────────────────────────────────────────────────────
# Vulkan::Vulkan    — the runtime loader (libvulkan.so)
# Vulkan::Headers   — the vulkan/vulkan.h header set
# Vulkan::glslc     — the GLSL→SPIR-V compiler executable
find_package(Vulkan REQUIRED COMPONENTS glslc)

# FreeType — used to rasterise font glyphs in ImGui.
find_package(Freetype REQUIRED)

# ─── FetchContent: download and pin third-party libraries ─────────────────────
# Every dependency is pinned to a specific release tag (or commit hash for stb
# which has no release tags). Never use a floating branch — it makes builds
# non-reproducible and can silently break after an upstream push.
include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# vk-bootstrap v1.3.295
# Wraps the boilerplate of VkInstance / VkPhysicalDevice / VkDevice / VkSwapchain
# selection. Without this, those ~300 lines of Vulkan setup are hand-written.
FetchContent_Declare(vk-bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG        v1.3.295
    GIT_SHALLOW    TRUE)

# VulkanMemoryAllocator v3.1.0
# Allocating GPU memory (VkDeviceMemory) by hand requires tracking alignment,
# memory type indices, and sub-allocation pools. VMA does all of this correctly.
FetchContent_Declare(VulkanMemoryAllocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.1.0
    GIT_SHALLOW    TRUE)

# GLFW 3.4 — prefer system installation, fall back to FetchContent.
# Avoids a git clone on machines that already have libglfw3-dev.
find_package(glfw3 3.3 QUIET)
if(NOT glfw3_FOUND)
    message(STATUS "System GLFW not found — fetching GLFW 3.4 via FetchContent")
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(glfw)
endif()

# GLM 1.0.1 — header-only math library (vectors, matrices, quaternions).
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE)

# Dear ImGui v1.91.6
# ImGui has NO CMakeLists.txt of its own. We use FetchContent_Populate (not
# MakeAvailable) to download the source tree, then compile it manually below.
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6
    GIT_SHALLOW    TRUE)

# tinyobjloader v2.0.0rc13 — loads Wavefront .obj mesh files.
set(TINYOBJLOADER_BUILD_TEST_LOADER OFF CACHE BOOL "" FORCE)
FetchContent_Declare(tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc13
    GIT_SHALLOW    TRUE)

# stb — single-header libraries (stb_image.h for texture loading).
# EXCEPTION to the pinned-tag rule: stb has never published a release tag
# in its 12-year history. Using 'master' is the only option. The risk is
# low because stb is header-only, changes rarely, and breaking changes are
# announced in the commit message. Revisit if a tagged release ever appears.
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE)

# spdlog v1.15.0 — structured logging with fmt-style formatting.
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.0
    GIT_SHALLOW    TRUE)

# Make all deps available except imgui (which needs manual population).
FetchContent_MakeAvailable(
    vk-bootstrap
    VulkanMemoryAllocator
    glm
    tinyobjloader
    stb
    spdlog)

# Populate imgui source without calling add_subdirectory (imgui has none).
# This sets imgui_SOURCE_DIR which we reference below.
FetchContent_Populate(imgui)

# ─── ImGui compiled static library ───────────────────────────────────────────
# ImGui is NOT header-only when using backends. We compile the core imgui files
# plus the Vulkan and GLFW backend implementations into a static library.
# Any translation unit that calls ImGui:: or ImGui_ImplVulkan_* just links imgui.
add_library(imgui STATIC
    # Core imgui — the widget system, draw list, tables, and demo window
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp

    # Vulkan backend — submits ImGui draw data as Vulkan render commands
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp

    # GLFW backend — polls keyboard/mouse state from the GLFW window
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends)

target_link_libraries(imgui PUBLIC
    Vulkan::Headers   # imgui_impl_vulkan.cpp needs <vulkan/vulkan.h>
    glfw)             # imgui_impl_glfw.cpp needs <GLFW/glfw3.h>

# IMGUI_IMPL_VULKAN_NO_PROTOTYPES: tell the Vulkan backend to load all Vulkan
# function pointers at runtime through vkGetInstanceProcAddr / vkGetDeviceProcAddr
# rather than expecting them to be statically linked. This is required when using
# vk-bootstrap, which owns the loader and device lifecycle. Without this flag the
# backend would try to call Vulkan functions as if they were ordinary library
# functions — which fails because libvulkan uses a dispatch table, not exports.
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES)

# ─── stb INTERFACE library ───────────────────────────────────────────────────
# stb is pure headers. Expose the download directory as an INTERFACE include path
# so any target that links stb_headers can do #include "stb_image.h".
add_library(stb_headers INTERFACE)
target_include_directories(stb_headers INTERFACE ${stb_SOURCE_DIR})

# ─── Shader compilation ───────────────────────────────────────────────────────
# GLSL source lives in src/shaders/. At build time we compile each .vert / .frag
# to a .spv (SPIR-V binary) via glslc. The runtime loads these .spv files using
# the SHADERS_DIR compile definition set below.
#
# CONFIGURE_DEPENDS: re-run cmake's glob if a shader file is added or removed,
# so CMake doesn't silently miss the new file.
file(GLOB SHADER_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.frag")

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_OUTPUT_DIR}")

set(SPIRV_OUTPUTS "")
foreach(SHADER_FILE ${SHADER_SOURCES})
    get_filename_component(SHADER_STEM "${SHADER_FILE}" NAME)
    set(SPIRV_OUT "${SHADER_OUTPUT_DIR}/${SHADER_STEM}.spv")

    add_custom_command(
        OUTPUT  "${SPIRV_OUT}"
        COMMAND Vulkan::glslc "${SHADER_FILE}" -o "${SPIRV_OUT}"
        DEPENDS "${SHADER_FILE}"
        COMMENT "glslc: ${SHADER_STEM} → ${SHADER_STEM}.spv"
        VERBATIM)

    list(APPEND SPIRV_OUTPUTS "${SPIRV_OUT}")
endforeach()

# A custom target that depends on all .spv outputs forces them to be built
# whenever they are out of date. zappy_gui depends on this target so shaders
# are always compiled before the executable is run.
add_custom_target(shaders ALL DEPENDS ${SPIRV_OUTPUTS})

# ─── zappy_gui executable ────────────────────────────────────────────────────
add_executable(zappy_gui
    src/main.cpp
    src/network/NetworkClient.cpp
    src/world/WorldState.cpp)

# shaders is a build-order dependency: the .spv files must exist before the
# game runs, but they don't affect the link step. add_dependencies (not
# target_link_libraries) expresses this correctly.
add_dependencies(zappy_gui shaders)

# ─── Include paths ────────────────────────────────────────────────────────────
# src/ is the root — sub-headers are included as "network/NetworkClient.hpp" etc.
target_include_directories(zappy_gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

# ─── Link libraries ──────────────────────────────────────────────────────────
target_link_libraries(zappy_gui PRIVATE
    Vulkan::Vulkan              # libvulkan.so — the Vulkan runtime loader
    Vulkan::Headers             # vulkan/vulkan.h header set
    vk-bootstrap::vk-bootstrap # Instance/device bootstrap
    GPUOpen::VulkanMemoryAllocator  # GPU memory allocator
    glfw                        # Window + input (imported target, system or fetched)
    glm::glm                    # Math header library
    imgui                       # Our compiled ImGui static lib
    tinyobjloader               # .obj mesh loader
    stb_headers                 # stb_image.h texture loader
    spdlog::spdlog              # Structured logging
    Freetype::Freetype)         # Font rasterisation

# ─── Compile definitions ──────────────────────────────────────────────────────
# VULKAN_VALIDATION expands to 1 in Debug builds and 0 in Release/RelWithDebInfo.
# The generator expression $<CONFIG:Debug> is evaluated at build time per config.
target_compile_definitions(zappy_gui PRIVATE
    VULKAN_VALIDATION=$<CONFIG:Debug>
    ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src/assets"
    SHADERS_DIR="${CMAKE_BINARY_DIR}/shaders")

# ─── Warning flags ────────────────────────────────────────────────────────────
# -Werror intentionally omitted: third-party headers (imgui, vk-bootstrap) often
# produce pedantic warnings we do not own and cannot fix.
target_compile_options(zappy_gui PRIVATE
    -Wall
    -Wextra
    -Wpedantic)


# ─── Documentation target ────────────────────────────────────────────────────
# Optional: only registered if Doxygen is installed on the host machine.
# Build with: cmake --build build/debug --target docs
# Output:     docs/doxygen/html/index.html  (build artifact — not committed)
find_package(Doxygen OPTIONAL_COMPONENTS dot)
if(DOXYGEN_FOUND)
    # doxygen-awesome-css — clean HTML theme for Doxygen output.
    # Fetched at configure time just like the other third-party libraries.
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
