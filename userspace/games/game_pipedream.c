#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ncurses.h>
#include <sys/ioctl.h>
#include "../../shared/kw_ioctl.h"
#include "games.h"

int game_pipedream_run(int fd)
{
    pthread_mutex_lock(&game_mutex);
    game_shared.fill_percent = 0;
    snprintf(game_shared.message, 128, "DRAIN THE PIPE!");
    snprintf(game_shared.subtext, 128, "Press A / S / D / F to drain");
    pthread_mutex_unlock(&game_mutex);

    int local_fill = 100;
    pthread_mutex_lock(&game_mutex);
    game_shared.fill_percent = local_fill;
    pthread_mutex_unlock(&game_mutex);

    unsigned char event;
    while (1) {
        ssize_t n = read(fd, &event, 1);
        if (n <= 0) break;

        if (event == KW_EVENT_TIMEOUT) {
            pthread_mutex_lock(&game_mutex);
            snprintf(game_shared.message, 128, "PIPE OVERFLOWED!");
            snprintf(game_shared.subtext, 128, "Drain faster next time!");
            pthread_mutex_unlock(&game_mutex);
            sleep(2);
            return 0;
        }

        if (KW_EVENT_IS_PRESS(event)) {
            local_fill -= 5;
            if (local_fill < 0) local_fill = 0;
            pthread_mutex_lock(&game_mutex);
            game_shared.fill_percent = local_fill;
            if (local_fill >= 80)
                snprintf(game_shared.message, 128, "!! HURRY UP !!");
            else
                snprintf(game_shared.message, 128, "DRAIN THE PIPE!");
            snprintf(game_shared.subtext, 128, "Pipe: %d%%", local_fill);
            pthread_mutex_unlock(&game_mutex);

            if (local_fill == 0) {
                ioctl(fd, KW_IOCTL_STOP);
                return 1;
            }
        }
    }
    return 0;
}

void game_pipedream_draw(void)  {
    pthread_mutex_lock(&game_mutex);
    int  fill = game_shared.fill_percent;
    char msg[128], sub[128];
    strncpy(msg, game_shared.message, 128);
    strncpy(sub, game_shared.subtext,  128);
    pthread_mutex_unlock(&game_mutex);

    mvprintw(7,  10, "=== PIPE DREAM ===");
    mvprintw(9,  10, "%-40s", fill >= 80 ? "!! HURRY UP !!" : msg);
    mvprintw(10, 10, "%-40s", sub);

    const int BW = 20;
    int filled = (fill * BW) / 100;
    if (fill >= 80) attron(A_BOLD);
    mvprintw(13, 10, "PIPE [");
    for (int i = 0; i < BW; i++)
        mvaddch(13, 16 + i, i < filled ? '#' : '-');
    mvprintw(13, 16 + BW, "] %3d%%", fill);
    if (fill >= 80) attroff(A_BOLD);

    mvprintw(15, 10, "Keys: [A] [S] [D] [F]");
}