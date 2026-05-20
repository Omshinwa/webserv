NAME = webserv

# a changer avant la correction
SRC =	src/main.cpp \
		src/utils/colorC.cpp \
		src/server/Connexion.cpp \
		src/server/Server.cpp

OBJ = $(SRC:.cpp=.o)

SRC_DIR = .
OBJ_DIR = build

SOURCE_FILES = $(addprefix $(SRC_DIR)/, $(SRC))
OBJECT_FILES = $(addprefix $(OBJ_DIR)/, $(OBJ))
DEPENDANCIES = $(OBJECT_FILES:.o=.d)

INCLUDES = -I src -I src/utils -I src/server

CXX = c++ -Wall -Wextra -Werror -std=c++98 -g3 -MMD -MP $(INCLUDES)

# Rules
all: $(NAME)

# linking
$(NAME): $(OBJECT_FILES) Makefile
	$(CXX) $(OBJECT_FILES) -o $(NAME)

# $(OBJECT_FILES): $(SOURCE_FILES)
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) -c $< -o $@

-include $(DEPENDANCIES)

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
