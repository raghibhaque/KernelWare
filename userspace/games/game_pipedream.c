#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ncurses.h>
#include <sys/ioctl.h>
#include "../../shared/kw_ioctl.h"
#include "games.h"

extern volatile int currentScreen;

void game_pipedream_run(int fd)
{
    ioctl(fd, KW_IOCTL_START, 1);

    pthread_mutex_lock(&game_mutex);
    game_shared.score        = 0;
    game_shared.fill_percent = 0;
    snprintf(game_shared.message, 128, "DRAIN THE PIPE!");
    snprintf(game_shared.subtext, 128, "Press A / S / D / F to drain");
    pthread_mutex_unlock(&game_mutex);

    unsigned char event;
    while (1) {
        
        ssize_t n = read(fd, &event, 1);
        if (n <= 0) break;

        if (event == KW_EVENT_TIMEOUT) {
            pthread_mutex_lock(&game_mutex);
            snprintf(game_shared.message, 128, "PIPE OVERFLOWED! GAME OVER");
            snprintf(game_shared.subtext, 128, "Final score: %d", game_shared.score);
            pthread_mutex_unlock(&game_mutex);
            break;
        } 
        
        else if (KW_EVENT_IS_PRESS(event)) {
            pthread_mutex_lock(&game_mutex);
            game_shared.score += 10;
            snprintf(game_shared.subtext, 128, "Score: %d", game_shared.score);
            pthread_mutex_unlock(&game_mutex);
        }

        struct kw_state state;
        memset(&state, 0, sizeof(state));
        if (ioctl(fd, KW_IOCTL_GET_STATE, &state) == 0) {
            pthread_mutex_lock(&game_mutex);
            game_shared.fill_percent = (int)state.score;
            if (game_shared.fill_percent >= 80)
                snprintf(game_shared.message, 128, "!! DRAIN NOW !!");
            else
                snprintf(game_shared.message, 128, "DRAIN THE PIPE!");
            pthread_mutex_unlock(&game_mutex);
        }
    }
    ioctl(fd, KW_IOCTL_STOP);
    sleep(2);
    currentScreen = 0;
}

void game_pipedream_draw(void)  {
    pthread_mutex_lock(&game_mutex);
    int  fill  = game_shared.fill_percent;
    int  score = game_shared.score;
    char msg[128], sub[128];
    strncpy(msg, game_shared.message, 128);
    strncpy(sub, game_shared.subtext,  128);
    pthread_mutex_unlock(&game_mutex);

    mvprintw(4, 10, "=== PIPE DREAM ===");
    mvprintw(6, 10, "Score: %d", score);

    const int BAR_WIDTH = 40;
    const int BAR_ROW   = 8;
    const int BAR_COL   = 10;

    int filled = (fill * BAR_WIDTH) / 100;

    if (fill >= 80) attron(A_BOLD); // Makes it in bold

    mvprintw(BAR_ROW, BAR_COL, "PIPE [");
    for (int i = 0; i < BAR_WIDTH; i++)
        mvaddch(BAR_ROW, BAR_COL + 6 + i, i < filled ? '#' : '-');
    mvprintw(BAR_ROW, BAR_COL + 6 + BAR_WIDTH, "] %3d%%", fill);

    if (fill >= 80) attroff(A_BOLD);

    mvprintw(10, BAR_COL, "%-40s", msg);
    mvprintw(11, BAR_COL, "%-40s", sub);
    mvprintw(13, BAR_COL, "Keys: [A] [S] [D] [F]");
}