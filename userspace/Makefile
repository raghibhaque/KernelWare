CC     = gcc
CFLAGS = -Wall -Wextra -pthread -lncurses -I../shared

SRCS = main.c \
       input.c \
       games/games.c \
       games/game_pipedream.c \
       games/game_rotbrain.c

all: game

game: $(SRCS)
	$(CC) $(CFLAGS) -o game $(SRCS)

clean:
	rm -f game