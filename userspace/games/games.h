#ifndef GAMES_H
#define GAMES_H

#include <pthread.h>

typedef enum {
    INPUT_MODE_BUTTONS,   
    INPUT_MODE_TEXT
} input_mode_t;


// Interface that everygame must implement
typedef struct {
    const char *name;
    input_mode_t input_mode;
    int  (*run)(int fd);  // returns 1 on win, 0 on timeout
    void (*draw)(void);
} game_def_t;

extern game_def_t games[];
extern int        num_games;

// Shared between game and render thread, game writes, render reads
typedef struct {
    int  score;
    int  lives;
    int  fill_percent;    // for Pipe Dream
    int  time_left_ms;
    char message[300];
    char subtext[128];
    char prompt[256];     // any game that shows a word
    char typed[64];       // text-input games
    int  typed_len;       // current length of typed buffer
    int solved;
} game_shared_t;

extern game_shared_t   game_shared;
extern pthread_mutex_t game_mutex;

#define GAME_SET_MSG(msg, sub) do {                         \
    pthread_mutex_lock(&game_mutex);                        \
    snprintf(game_shared.message, 300, "%s", (msg));        \
    snprintf(game_shared.subtext, 128, "%s", (sub));        \
    pthread_mutex_unlock(&game_mutex);                      \
} while(0)

#endif