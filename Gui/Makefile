##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## Makefile : TODO cmake
##


NAME = zappy_gui
SRC = main.cpp \



OBJ = $(SRC:.cpp=.o)
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -g -O2 -fno-omit-frame-pointer
CPPFLAGS = -I include

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