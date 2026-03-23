NAME = webserv
SRC = main.cpp \
	srcs/config/ConfigParser.cpp \
	srcs/CgiExecutor.cpp
HEADERS = includes/config/Config.hpp includes/config/Parser.hpp includes/config/Tokenizer.hpp includes/CgiExecutor.hpp
TEST_CGI_NAME = cgi_executor_smoke
TEST_CGI_SRC = tests/cgi_executor_smoke.cpp srcs/CgiExecutor.cpp
TEST_CGI_OBJ = $(TEST_CGI_SRC:.cpp=.o)

OBJ = $(SRC:.cpp=.o)
CXX = c++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -Iincludes

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp $(HEADERS) Makefile
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TEST_CGI_OBJ)

fclean: clean
	rm -f $(NAME) $(TEST_CGI_NAME)

re: fclean all

.PHONY: all clean fclean re test-cgi-executor run-cgi-smoke test-integration