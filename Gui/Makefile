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

## Builds the native library and has Godot export a runnable executable,
## using the matching preset in export_presets.cfg. `bin` targets the host
## OS; `bin-linux`/`bin-windows` cross-target explicitly (e.g. to build the
## Windows exe from WSL once mingw-w64 + Godot export templates for Windows
## are installed).
##
## Both the debug and release presets are built first: Godot's export step
## does an internal debug-context project scan before packaging, and without
## bin/libzappy_gui.*.debug.*.so present it can't resolve the GDExtension
## classes during that scan, silently baking placeholder nodes (empty
## NodePath exports) into the exported scenes — which crashes at runtime
## (e.g. on pressing Connect) instead of failing the build.
##
## A plain `godot --headless --path . --import` priming pass runs first.
## On a project with no .godot/ yet (e.g. right after cloning — .godot/ is
## machine-local and gitignored, so it never comes from git), Godot 4.6.3
## hits an engine bug while registering GDExtension classes for the very
## first time: it tries to register ConnectDialog twice, the second attempt
## fails, every method/property bind for it then fails too ("for unexisting
## class"), and the corrupted state segfaults Godot on shutdown. The bug
## doesn't reproduce once .godot/'s class-name/uid caches already exist, so
## priming an --import pass first absorbs it harmlessly (--import never
## exports, so it never poisons .godot/exported/ with a bad bake) and the
## real export below then always runs against an already-warm cache.
##
## .godot/exported/ (cached scene-to-.pck bakes) is cleared before exporting
## regardless, since it was separately observed reusing a stale bake after a
## .tscn source change instead of re-converting it.
bin: $(HOST_BIN_TARGET)

bin-linux: linux-debug linux-release
	$(GODOT) --headless --editor --import --path . > /dev/null 2>&1 || true
	rm -rf .godot/exported
	$(GODOT) --headless --path . --export-release "linux"

bin-windows: windows-debug windows-release
	$(GODOT) --headless --editor --import --path . > /dev/null 2>&1 || true
	rm -rf .godot/exported
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
