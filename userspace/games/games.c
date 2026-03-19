#include "games.h"

game_shared_t game_shared = {0};
pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;


int  game_pipedream_run(int fd);
void game_pipedream_draw(void);

void game_rotbrain_run(int fd);
void game_rotbrain_draw(void);

int  game_killit_run(int fd);
void game_killit_draw(void);

int  game_memleak_run(int fd);
void game_memleak_draw(void);


game_def_t games[] = {
    {
        .name = "PIPE DREAM - Drain the FIFO before it fills",
        .input_mode = INPUT_MODE_BUTTONS,
        .run  = game_pipedream_run,
        .draw = game_pipedream_draw,
    }, 
    {
        .name = "ROT-BRAIN - Decode the scrambled word",
        .input_mode = INPUT_MODE_TEXT,
        .run  = game_rotbrain_run,
        .draw = game_rotbrain_draw,
    },
    {
        .name = "KILL IT - Type the PID to kill the process",
        .run  = game_killit_run,
        .draw = game_killit_draw,
    },
    {
        .name = "MEMORY LEAK - Alloc and free kernel memory",
        .run  = game_memleak_run,
        .draw = game_memleak_draw,
    },
};
int num_games = sizeof(games) / sizeof(games[0]);
