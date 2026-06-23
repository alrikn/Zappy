##
## zappy-gui — Makefile front-end for the CMake presets (CMakePresets.json)
##
## Default target builds the debug preset matching the host OS (linux-debug
## or windows-debug). Other platform/config combos and the docs target are
## available as explicit targets below.
##

PRESETS    := linux-debug linux-release windows-debug windows-release
JOBS       ?= $(shell nproc 2>/dev/null || echo 1)

# Path to the Godot editor binary, used by `bin` to export the runnable
# executable. Left as a plain override (`make bin GODOT=/path/to/godot`) —
# resolving/validating it automatically is a later step.
GODOT      ?= godot

ifeq ($(OS),Windows_NT)
    HOST_DEBUG_PRESET   := windows-debug
    HOST_BIN_TARGET      := bin-windows
else
    HOST_DEBUG_PRESET   := linux-debug
    HOST_BIN_TARGET      := bin-linux
endif

.PHONY: all bin bin-linux bin-windows $(PRESETS) docs clean fclean re help

all: $(HOST_DEBUG_PRESET)

$(PRESETS):
	cmake --preset $@
	cmake --build --preset $@ --parallel $(JOBS)

## Builds the release native library and has Godot export a runnable
## executable, using the matching preset in export_presets.cfg. `bin` targets
## the host OS; `bin-linux`/`bin-windows` cross-target explicitly (e.g. to
## build the Windows exe from WSL once mingw-w64 + Godot export templates for
## Windows are installed).
bin: $(HOST_BIN_TARGET)

bin-linux: linux-release
	$(GODOT) --headless --path . --export-release "linux"

bin-windows: windows-release
	$(GODOT) --headless --path . --export-release "windows"

## Doxygen HTML docs → docs/doxygen/html/index.html. Reuses the linux-debug
## build dir for configuration since the docs target isn't platform-specific.
docs:
	cmake --preset linux-debug
	cmake --build --preset linux-debug --target docs

clean:
	rm -rf build/linux-debug build/linux-release build/windows-debug build/windows-release

## Only removes the compiled libraries, never bin/zappy_gui.gdextension(.uid) —
## those are checked-in manifest files Godot needs, not build output.
fclean: clean
	rm -f bin/libzappy_gui.*
	rm -f zappy_gui zappy_gui.exe zappy_gui.pck zappy_gui.console.exe
	rm -rf docs/doxygen

re: fclean all

help:
	@echo "Targets:"
	@echo "  make [all]           build debug preset for host OS (default: $(HOST_DEBUG_PRESET))"
	@echo "  make linux-release   build release for Linux"
	@echo "  make windows-debug   cross-compile debug for Windows (mingw)"
	@echo "  make windows-release cross-compile release for Windows (mingw)"
	@echo "  make bin             build release lib + export runnable exe/bin for host OS"
	@echo "  make bin-linux       build release lib + export runnable bin for Linux"
	@echo "  make bin-windows     cross-build release lib + export runnable exe for Windows"
	@echo "  make docs            generate Doxygen HTML docs"
	@echo "  make clean           remove build/<preset> directories"
	@echo "  make fclean          clean + remove compiled libs and docs/doxygen"
	@echo "  make re              fclean + all"
