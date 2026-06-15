##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## Makefile : TODO cmake
##


NAME = zappy_server
SRC = main.cpp \
	src/Clients/Client.cpp \
	src/Clients/Player/Player.cpp \
	src/Clients/Player/Movement.cpp \
	src/Clients/Player/Actions.cpp \
	src/Clients/Player/Incantation.cpp \
	src/Clients/Gui/Gui.cpp \
	src/Clients/Gui/GuiCommands.cpp \
	src/Game/game_loop.cpp \
	src/Game/Tiles.cpp \
	src/Server/Polling.cpp \
	src/Server/server_helper.cpp \
	src/Server/client_helpers.cpp \


OBJ = $(SRC:.cpp=.o)
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -g -O2 -fno-omit-frame-pointer
CPPFLAGS = -I include -I include/Observer -I include/Client -I include/Map -I include/Team

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(NAME) $(OBJ)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(CONVERT_OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re