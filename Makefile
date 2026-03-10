NAME = webserv
SRC = main.cpp \
	srcs/config/ConfigParser.cpp
HEADERS = includes/ConfigParser.hpp

OBJ = $(SRC:.cpp=.o)
CXX = c++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -Iincludes

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp $(HEADERS) Makefile
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re