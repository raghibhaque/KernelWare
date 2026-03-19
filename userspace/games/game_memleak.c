#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ncurses.h>
#include <sys/ioctl.h>
#include "../../shared/kw_ioctl.h"
#include "games.h"

#define WIN_SCORE 5

int game_memleak_run(int fd)
{
    GAME_SET_MSG("Allocate and free kernel memory!", "A = alloc  F = free");

    struct kw_state state;
    int allocated = 0;  

    while (1) {
        memset(&state, 0, sizeof(state));
        ioctl(fd, KW_IOCTL_GET_STATE, &state);

        if (state.score >= WIN_SCORE) {
            GAME_SET_MSG("MEMORY CLEAN!", "All allocations freed correctly.");
            sleep(2);
            ioctl(fd, KW_IOCTL_STOP);
            return 1;
        }

        if (state.lives <= 0) {
            GAME_SET_MSG("MEMORY LEAKED!", "Too many bad allocs or double frees.");
            sleep(2);
            return 0;
        }

        pthread_mutex_lock(&game_mutex);
        game_shared.score = state.score;
        game_shared.lives = state.lives;
        snprintf(game_shared.subtext, 128,
        "Status: %s", allocated ? "[ ALLOCATED ]" : "[ FREE ]");
        pthread_mutex_unlock(&game_mutex);

        unsigned char response;
        int n = read(fd, &response, 1);
        if (n <= 0)
            continue;

        pthread_mutex_lock(&game_mutex);
        switch (response) {
        case 0x01:
            allocated = 1;
            snprintf(game_shared.message, 128, "Allocated! Now free it.");
            snprintf(game_shared.subtext,  128, "Score: %d/%d", state.score + 1, WIN_SCORE);
            break;
        case 0x02:
            snprintf(game_shared.message, 128, "LEAK! Already allocated!");
            snprintf(game_shared.subtext,  128, "Lives lost. Free first.");
            break;
        case 0x03:
            allocated = 0;
            snprintf(game_shared.message, 128, "Freed correctly! Alloc again.");
            snprintf(game_shared.subtext,  128, "Score: %d/%d", state.score, WIN_SCORE);
            break;
        case 0x04:
            snprintf(game_shared.message, 128, "DOUBLE FREE! Nothing to free.");
            snprintf(game_shared.subtext,  128, "Lives lost.");
            break;
        default:
            break;
        }
        pthread_mutex_unlock(&game_mutex);
    }

    return 0;
}

void game_memleak_draw(void)
{
    pthread_mutex_lock(&game_mutex);
    char msg[128], sub[128];
    int score = game_shared.score;
    int lives = game_shared.lives;
    strncpy(msg, game_shared.message, 128);
    strncpy(sub, game_shared.subtext,  128);
    pthread_mutex_unlock(&game_mutex);

    mvprintw(4,  10, "=== MEMORY LEAK ===");
    mvprintw(6,  10, "Score: %d/%d   Lives: %d", score, WIN_SCORE, lives);
    mvprintw(8,  10, "%-50s", msg);
    mvprintw(9,  10, "%-50s", sub);
    mvprintw(11, 10, "A = Allocate kernel memory");
    mvprintw(12, 10, "F = Free kernel memory");
}