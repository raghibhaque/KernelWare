CC     = gcc
CFLAGS = -Wall -Wextra -pthread -I../shared
LIBS   = -lncurses

SRCS = main.c \
       input.c \
       games/games.c \
       games/game_pipedream.c \
       games/game_killit.c
       games/game_memleak.c

all: game

game: $(SRCS)
	$(CC) $(CFLAGS) -o game $(SRCS) $(LIBS)

clean:
	rm -f game