#ifndef GAMES_H
#define GAMES_H

#include <pthread.h>

// Interface that everygame must implement
typedef struct {
    const char *name;
    int  (*run)(int fd);  // returns 1 on win, 0 on timeout
    void (*draw)(void);
} game_def_t;

extern game_def_t games[];
extern int        num_games;

// Shared between game and render thread, game writes, render reads
typedef struct {
    int  score;
    int  lives;
    int  fill_percent;
    int  time_left_ms;
    char message[128];
    char subtext[128];
} game_shared_t;

extern game_shared_t   game_shared;
extern pthread_mutex_t game_mutex;

#define GAME_SET_MSG(msg, sub) do {                         \
    pthread_mutex_lock(&game_mutex);                        \
    snprintf(game_shared.message, 128, "%s", (msg));        \
    snprintf(game_shared.subtext, 128, "%s", (sub));        \
    pthread_mutex_unlock(&game_mutex);                      \
} while(0)

#endif