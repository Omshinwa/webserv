NAME		=	webserv

INCLUDES	=	-I src -I src/utils -I src/server -I src/http -I src/event -I src/cgi

CXX			=	c++ -Wall -Wextra -Werror -std=c++98 -g3 -MMD -MP $(INCLUDES)

RM			=	rm -rf

SRC			=	src/main.cpp					\
				src/config/Config.cpp			\
				src/utils/Log.cpp				\
				src/utils/Utils.cpp				\
				src/utils/Utils_str.cpp			\
				src/server/Connection.cpp		\
				src/server/Server.cpp			\
				src/cgi/CgiHandler.cpp			\
				src/cgi/CgiProcess.cpp			\
				src/http/RequestParser.cpp		\
				src/http/ResponseBuilder.cpp	\
				src/http/ResponseBuilder_cgi.cpp\
				src/event/Reactor.cpp			\
				src/event/signal.cpp			\

OBJ			=	$(SRC:.cpp=.o)

OBJ_DIR		=	build/

OBJECT_FILES = $(addprefix $(OBJ_DIR), $(OBJ))


all: $(NAME)

$(NAME): $(OBJECT_FILES) Makefile
	$(CXX) $(OBJECT_FILES) -o $(NAME)

$(OBJ_DIR)%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) -c $< -o $@

-include $(OBJECT_FILES:.o=.d)

clean:
	$(RM) $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re
