NAME = webserv

SRC = srcs/config/Config.cpp \
	  srcs/config/ConfigParser.cpp \
      srcs/Server.cpp \
	  srcs/EpollLoop.cpp \
	  srcs/ProcessRequest.cpp \
	  srcs/ErrorResponseBuilder.cpp \
	  srcs/ConnectionManager.cpp \
	  srcs/StaticFileHandler.cpp \
      srcs/HttpRequest.cpp \
      srcs/HttpResponse.cpp \
	  srcs/CgiExecutor.cpp \
	  srcs/UploadHandler.cpp \
      main.cpp

HEADERS = includes/*

OBJ_DIR = obj
OBJ = $(addprefix $(OBJ_DIR)/,$(SRC:.cpp=.o))

CXX = c++
# CXXFLAGS = -std=c++17 -Wall -Wextra -Werror
CXXFLAGS = -std=c++17

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^


$(OBJ_DIR)/%.o: %.cpp $(HEADERS) Makefile
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re