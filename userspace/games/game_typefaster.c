#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ncurses.h>
#include <sys/ioctl.h>
#include "../../shared/kw_ioctl.h"
#include "games.h"

#define TARGET_PRESSES 20

int game_typefaster_run(int fd)
{
    GAME_SET_MSG("MASH ANY KEY!", "Hit any key as fast as you can!");

    while (1) {
        struct kw_state state = {0};
        ioctl(fd, KW_IOCTL_GET_STATE, &state);

        
        pthread_mutex_lock(&game_mutex);
        game_shared.score      = state.score;
        game_shared.fill_percent = state.score;  
        snprintf(game_shared.message, 128, "MASH ANY KEY!");
        snprintf(game_shared.subtext,  128, "Progress: %d%%", state.score);
        pthread_mutex_unlock(&game_mutex);

        unsigned char event = 0;
        int n = read(fd, &event, 1);
        if (n <= 0) continue;

        if (event == KW_EVENT_TIMEOUT) {
            if (state.score >= 100) {
                GAME_SET_MSG("FAST ENOUGH!", "You hit the target!");
                sleep(2);
                return 1;
            } else {
                GAME_SET_MSG("TOO SLOW!", "Not enough keypresses.");
                sleep(2);
                return 0;
            }
        }

        pthread_mutex_lock(&game_mutex);
        game_shared.fill_percent = (int)event;
        snprintf(game_shared.subtext, 128, "Progress: %d%%", (int)event);
        pthread_mutex_unlock(&game_mutex);
    }

    return 0;
}

void game_typefaster_draw(void)
{
    pthread_mutex_lock(&game_mutex);
    char msg[128], sub[128];
    int  progress = game_shared.fill_percent;
    strncpy(msg, game_shared.message, 128);
    strncpy(sub, game_shared.subtext,  128);
    pthread_mutex_unlock(&game_mutex);

    const int BAR = 30;
    int filled = (progress * BAR) / 100;
    if (filled > BAR) filled = BAR;

    mvprintw(7,  10, "=== TYPE FASTER ===");
    mvprintw(9,  10, "%-40s", msg);
    mvprintw(10, 10, "%-40s", sub);
    mvprintw(12, 10, "PROGRESS [");
    for (int i = 0; i < BAR; i++)
        mvaddch(12, 20 + i, i < filled ? '#' : '-');
    mvprintw(12, 20 + BAR, "] %3d%%", progress);
    mvprintw(14, 10, "Target: %d keypresses", TARGET_PRESSES);
}