CC     = gcc
CFLAGS = -Wall -Wextra -pthread -I../shared
LIBS   = -lncurses

SRCS = main.c \
       input.c \
       games/games.c \
       games/game_pipedream.c \
       games/game_rotbrain.c \
       games/game_killit.c \
       games/game_memleak.c \
       games/game_typefaster.c \
       games/game_loadbalancer.c \
       games/game_hackthehost.c

all: game

game: $(SRCS)
	$(CC) $(CFLAGS) -o game $(SRCS) $(LIBS)

clean:
	rm -f game