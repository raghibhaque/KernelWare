#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <sys/ioctl.h>
#include "../../shared/kw_ioctl.h"
#include "games.h"

extern volatile int currentScreen;


void game_rotbrain_run(int fd)
{
    struct kw_config cfg = { .lives = 3, .difficulty = 1.0, .timeout_ms = 30000 };
    ioctl(fd, KW_IOCTL_SET_CONFIG, &cfg);
    ioctl(fd, KW_IOCTL_START, 2);

    // Fetch the scrambled prompt
    struct kw_state state;
    memset(&state, 0, sizeof(state));
    ioctl(fd, KW_IOCTL_GET_STATE, &state);

    pthread_mutex_lock(&game_mutex);
    snprintf(game_shared.message, 128, "DECODE: %s", state.prompt);
    snprintf(game_shared.subtext, 128, "Type the original word and press Enter");
    game_shared.score = 0;
    game_shared.typed[0] = '\0';
    game_shared.typed_len = 0;
    pthread_mutex_unlock(&game_mutex);

    unsigned char event;
    while (1) {
        ssize_t n = read(fd, &event, 1);
        if (n <= 0) break;

        if (event == KW_EVENT_CORRECT) {
            pthread_mutex_lock(&game_mutex);
            snprintf(game_shared.message, 300, "CORRECT! Score: 100");
            pthread_mutex_unlock(&game_mutex);
            break;
        } else if (event == KW_EVENT_TIMEOUT) {
            pthread_mutex_lock(&game_mutex);
            snprintf(game_shared.message, 300, "TIME'S UP!");
            pthread_mutex_unlock(&game_mutex);
            break;
        }
    }

    ioctl(fd, KW_IOCTL_STOP);
    sleep(2);
    currentScreen = 0;
}


void game_rotbrain_draw(void)
{
    pthread_mutex_lock(&game_mutex);
    char msg[300], sub[128], typed[64];
    strncpy(msg, game_shared.message, 300);
    strncpy(sub, game_shared.subtext,  128);
    strncpy(typed, game_shared.typed, 64);
    int score = game_shared.score;
    pthread_mutex_unlock(&game_mutex);

    mvprintw(4, 10, "=== ROT-BRAIN ===");
    mvprintw(6, 10, "%s", msg);
    mvprintw(8, 10, "Your answer: %s_", typed);
    mvprintw(10, 10, "%s", sub);
    mvprintw(12, 10, "Score: %d", score);
    mvprintw(14, 10, "[A-Z] type | [Enter] submit | [Backspace] delete");
}