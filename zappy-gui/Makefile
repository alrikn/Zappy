##
## zappy-gui — Makefile front-end for the CMake presets (CMakePresets.json)
##
## Default target builds linux-debug. Other platform/config combos and the
## docs target are available as explicit targets below.
##

PRESETS    := linux-debug linux-release windows-debug windows-release
JOBS       ?= $(shell nproc 2>/dev/null || echo 1)

.PHONY: all $(PRESETS) docs clean fclean re help

all: linux-debug

$(PRESETS):
	cmake --preset $@
	cmake --build --preset $@ --parallel $(JOBS)

## Doxygen HTML docs → docs/doxygen/html/index.html. Reuses the linux-debug
## build dir for configuration since the docs target isn't platform-specific.
docs:
	cmake --preset linux-debug
	cmake --build --preset linux-debug --target docs

clean:
	rm -rf build/linux-debug build/linux-release build/windows-debug build/windows-release

fclean: clean
	rm -rf bin docs/doxygen

re: fclean all

help:
	@echo "Targets:"
	@echo "  make [all]          build linux-debug (default)"
	@echo "  make linux-release   build release for Linux"
	@echo "  make windows-debug   cross-compile debug for Windows (mingw)"
	@echo "  make windows-release cross-compile release for Windows (mingw)"
	@echo "  make docs            generate Doxygen HTML docs"
	@echo "  make clean           remove build/<preset> directories"
	@echo "  make fclean          clean + remove bin/ and docs/doxygen"
	@echo "  make re              fclean + all"
