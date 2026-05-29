NAME		=	webserv

CXX			=	c++ -Wall -Wextra -Werror -std=c++98 -g3 -MMD -MP $(INCLUDES)

INCLUDES	=	-I src -I src/util -I src/server -I src/http
RM			=	rm -rf

SRC			=	src/main.cpp					\
				src/util/Log.cpp				\
				src/server/Connexion.cpp		\
				src/server/Server.cpp			\
				src/http/RequestParser.cpp		\
				src/http/ResponseBuilder.cpp

OBJ			=	$(SRC:.cpp=.o)

OBJ_DIR		=	build/

OBJECT_FILES = $(addprefix $(OBJ_DIR), $(OBJ))
DEPENDANCIES = $(OBJECT_FILES:.o=.d)


all: $(NAME)

$(NAME): $(OBJECT_FILES) Makefile
	$(CXX) $(OBJECT_FILES) -o $(NAME)

$(OBJ_DIR)%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) -c $< -o $@

-include $(DEPENDANCIES)

clean:
	$(RM) $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re
