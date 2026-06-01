##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## Makefile : TODO cmake
##


NAME = zappy_server
SRC = main.cpp \
	src/Polling.cpp \
	src/server_helper.cpp \
	src/game_loop.cpp \


OBJ = $(SRC:.cpp=.o)
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -g -O2 -fno-omit-frame-pointer
CPPFLAGS = -I include -I include/Observer

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