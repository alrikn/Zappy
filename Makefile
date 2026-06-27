##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## Makefile
## TODO: CHANGE ALL THIS TO BE CMAKE
##


# this calles the makefiles in the Ai directory, Gui directory and Server directory and copies the binaries to the root

#also we'll need to find a way to compile python code to make a binary
# answer: using a shell script wrpaper its zappy_ai.py and compiles it using pyinstaller,
# then moves the binary to the root

# Gui's `bin` target exports a runnable exe/bin (zappy_gui.exe on Windows,
# zappy_gui on Linux).
ifeq ($(OS),Windows_NT)
    GUI_BIN := zappy_gui.exe
    GUI_LIB := libzappy_gui.windows.release.x86_64.dll
else
    GUI_BIN := zappy_gui
    GUI_LIB := libzappy_gui.linux.release.x86_64.so
endif

all:
	$(MAKE) -C Server
	$(MAKE) -C Gui bin
	cp Server/zappy_server .
	cp Gui/$(GUI_BIN) .
	cp Gui/zappy_gui.pck .
	mkdir -p bin
	cp Gui/$(GUI_LIB) bin/
	cp Ai/zappy_ai .


clean:
	$(MAKE) -C Server clean
	$(MAKE) -C Gui clean

fclean:	clean
	$(MAKE) -C Server fclean
	$(MAKE) -C Gui fclean
	rm -f zappy_server zappy_gui zappy_gui.exe zappy_gui.pck zappy_ai
	rm -f libzappy_gui.linux.release.x86_64.so libzappy_gui.windows.release.x86_64.dll

re:	fclean all
