##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## Makefile — thin wrapper; all build logic lives in CMakeLists.txt
##

NAME   := zappy_gui
PRESET := debug
BUILD  := build/debug

.PHONY: all $(NAME) clean fclean re

all: $(NAME)

$(NAME):
	cmake --preset $(PRESET)
	cmake --build $(BUILD) --target $(NAME)
	cp $(BUILD)/$(NAME) .

clean:
	rm -rf $(BUILD)/CMakeFiles

fclean:
	rm -rf build/
	rm -f $(NAME)

re: fclean all
