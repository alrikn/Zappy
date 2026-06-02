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
all:
	$(MAKE) -C Server
	$(MAKE) -C Gui
	cp Server/zappy_server .
	cp Gui/zappy_gui .
	cp Ai/zappy_ai .


clean:
	$(MAKE) -C Server clean
	$(MAKE) -C Gui clean

fclean:	clean
	rm -f zappy_server zappy_gui zappy_ai

re:	fclean all
