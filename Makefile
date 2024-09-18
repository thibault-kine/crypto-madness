# Nom du projet
NAME_SERVER = lpf_server
NAME_CLIENT = lpf

# Compilateur
CC = g++

# Options de compilation
CFLAGS = -g3 -Wall -Wextra -Werror 

# Fichiers source
SRC_SERVER = Server/main.cpp Packet/Packet.cpp Socket/Socket.cpp Utils/Utils.cpp Utils/Logger.cpp Utils/MD5.cpp
SRC_CLIENT = Client/main.cpp Packet/Packet.cpp Socket/Socket.cpp Utils/Utils.cpp Utils/Logger.cpp Utils/MD5.cpp

# Fichiers objets
OBJ_SERVER = $(SRC_SERVER:.cpp=.o)
OBJ_CLIENT = $(SRC_CLIENT:.cpp=.o)


# Règle par défaut
all: $(NAME_SERVER) $(NAME_CLIENT)

$(NAME_SERVER): $(OBJ_SERVER)
	$(CC) $(CFLAGS) -o ./$(NAME_SERVER) $(OBJ_SERVER)

$(NAME_CLIENT): $(OBJ_CLIENT)
	$(CC) $(CFLAGS) -o ./$(NAME_CLIENT) $(OBJ_CLIENT)

clean:
	rm -f $(OBJ_SERVER) $(OBJ_CLIENT)

fclean: clean
	rm -f $(NAME_SERVER) $(NAME_CLIENT)

re: fclean all

.PHONY: all clean fclean re