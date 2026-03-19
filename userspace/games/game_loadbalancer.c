#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ncurses.h>
#include <sys/ioctl.h>
#include "../../shared/kw_ioctl.h"
#include "games.h"

extern volatile int currentScreen;

#define LB_THREAD_COUNT 3

int game_loadbalancer_run(int fd)
{
    pthread_mutex_lock(&game_mutex);
    snprintf(game_shared.message, 300, "Kill all kthreads before time runs out!");
    snprintf(game_shared.subtext,  128, "Type a thread number and press Enter");
    game_shared.typed[0]  = '\0';
    game_shared.typed_len = 0;
    // encode alive bitmask in solved: bit 0 = thread 1, bit 1 = thread 2, bit 2 = thread 3
    game_shared.solved = 0x07;
    pthread_mutex_unlock(&game_mutex);

    int won = 0;
    unsigned char event;
    while (1) {
        ssize_t n = read(fd, &event, 1);
        if (n <= 0) break;

        if (event == KW_EVENT_TIMEOUT) {
            pthread_mutex_lock(&game_mutex);
            snprintf(game_shared.message, 300, "TIME'S UP! Threads escaped.");
            game_shared.subtext[0] = '\0';
            pthread_mutex_unlock(&game_mutex);
            break;
        }

        if (event == KW_EVENT_CORRECT) {
            pthread_mutex_lock(&game_mutex);
            game_shared.solved = 0x00;
            snprintf(game_shared.message, 300, "ALL THREADS TERMINATED!");
            game_shared.subtext[0] = '\0';
            pthread_mutex_unlock(&game_mutex);
            won = 1;
            break;
        }

        if (event == 0x01) {
            // one thread killed — read typed to find which ID
            pthread_mutex_lock(&game_mutex);
            int id = game_shared.typed[0] - '0';
            if (id >= 1 && id <= LB_THREAD_COUNT) {
                game_shared.solved &= ~(1 << (id - 1));
                snprintf(game_shared.message, 300, "kw_lb_%d terminated.", id);
            }
            snprintf(game_shared.subtext, 128, "Type the next thread ID");
            game_shared.typed[0]  = '\0';
            game_shared.typed_len = 0;
            pthread_mutex_unlock(&game_mutex);
        } else if (event == 0x02) {
            pthread_mutex_lock(&game_mutex);
            snprintf(game_shared.message, 300, "Invalid ID or already dead.");
            game_shared.typed[0]  = '\0';
            game_shared.typed_len = 0;
            pthread_mutex_unlock(&game_mutex);
        }
    }

    ioctl(fd, KW_IOCTL_STOP);
    sleep(1);
    currentScreen = 0;
    return won;
}

void game_loadbalancer_draw(void)
{
    pthread_mutex_lock(&game_mutex);
    char msg[300], sub[128], typed[64];
    int  mask = game_shared.solved;
    strncpy(msg,   game_shared.message, 300);
    strncpy(sub,   game_shared.subtext,  128);
    strncpy(typed, game_shared.typed,    64);
    pthread_mutex_unlock(&game_mutex);

    mvprintw(7,  10, "=== LOAD BALANCER ===");
    mvprintw(9,  10, "%-44s", msg);
    mvprintw(10, 10, "%-44s", sub);

    for (int i = 0; i < LB_THREAD_COUNT; i++) {
        bool alive = (mask >> i) & 1;
        if (alive)
            mvprintw(12 + i, 10, "[%d] kw_lb_%d  [ RUNNING ]", i + 1, i + 1);
        else
            mvprintw(12 + i, 10, "[%d] kw_lb_%d  [  DEAD   ]", i + 1, i + 1);
    }

    mvprintw(16, 10, "Your input: %s_", typed);
    mvprintw(17, 10, "[1-3] type | [Enter] submit");
}
