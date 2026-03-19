NAME = webserv

SRC = srcs/ServerConfig.cpp \
      srcs/Server.cpp \
	srcs/StaticFileHandler.cpp \
      srcs/HttpRequest.cpp \
      srcs/HttpResponse.cpp \
      main.cpp

HEADERS = includes/*

OBJ = $(SRC:.cpp=.o)

CXX = c++
# CXXFLAGS = -std=c++17 -Wall -Wextra -Werror
CXXFLAGS = -std=c++17

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