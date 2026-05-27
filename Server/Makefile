##
## EPITECH PROJECT, 2026
## Zappy
## File description:
## Makefile : TODO cmake
##


NAME = zappy_server
SRC = main.cpp \
	src/Server.cpp \
	src/server_helper.cpp \


OBJ = $(SRC:.cpp=.o)
CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -g -O2 -fno-omit-frame-pointer
CPPFLAGS = -I include

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(NAME) $(OBJ) -lconfig++

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(CONVERT_OBJ)

fclean: clean
	rm -f $(NAME) $(CONVERT_NAME)
	rm -f default.profraw default.profdata lcov.info
	rm -rf coverage_report
	rm -f tests/rt_unit_tests tests/rt_integration_tests tests/rt_functional_tests

re: fclean all

.PHONY: all clean fclean re