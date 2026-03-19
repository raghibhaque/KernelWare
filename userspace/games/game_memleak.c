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
    int ml_score = 0, ml_lives = 3;

    pthread_mutex_lock(&game_mutex);
    snprintf(game_shared.message, 128, "Allocate and free kernel memory!");
    snprintf(game_shared.subtext,  128, "A = alloc  F = free");
    game_shared.fill_percent = ml_score;
    game_shared.solved       = ml_lives;
    pthread_mutex_unlock(&game_mutex);

    while (1) {
        unsigned char response;
        int n = read(fd, &response, 1);
        if (n <= 0)
            continue;

        if (response == KW_EVENT_TIMEOUT) {
            GAME_SET_MSG("TIME'S UP!", "");
            sleep(1);
            return 0;
        }

        pthread_mutex_lock(&game_mutex);
        switch (response) {
        case 0x01:
            snprintf(game_shared.message, 128, "Allocated! Now press F to free it.");
            snprintf(game_shared.subtext,  128, "Score: %d/%d", ml_score, WIN_SCORE);
            break;
        case 0x02:
            ml_lives--;
            snprintf(game_shared.message, 128, "LEAK! Already allocated!");
            snprintf(game_shared.subtext,  128, "Lives lost. Free first.");
            break;
        case 0x03:
            ml_score++;
            snprintf(game_shared.message, 128, "Freed! Press A to alloc again.");
            snprintf(game_shared.subtext,  128, "Score: %d/%d", ml_score, WIN_SCORE);
            break;
        case 0x04:
            ml_lives--;
            snprintf(game_shared.message, 128, "DOUBLE FREE! Nothing to free.");
            snprintf(game_shared.subtext,  128, "Lives lost.");
            break;
        default:
            break;
        }
        game_shared.fill_percent = ml_score;
        game_shared.solved       = ml_lives;
        pthread_mutex_unlock(&game_mutex);

        if (ml_score >= WIN_SCORE) {
            GAME_SET_MSG("MEMORY CLEAN!", "All allocations freed correctly.");
            sleep(1);
            ioctl(fd, KW_IOCTL_STOP);
            return 1;
        }

        if (ml_lives <= 0) {
            GAME_SET_MSG("MEMORY LEAKED!", "Too many bad allocs or double frees.");
            sleep(1);
            return 0;
        }
    }

    return 0;
}

void game_memleak_draw(void)
{
    pthread_mutex_lock(&game_mutex);
    char msg[128], sub[128];
    int score = game_shared.fill_percent;
    int lives = game_shared.solved;
    strncpy(msg, game_shared.message, 128);
    strncpy(sub, game_shared.subtext,  128);
    pthread_mutex_unlock(&game_mutex);

    mvprintw(7,  10, "=== MEMORY LEAK ===");
    mvprintw(9,  10, "Score: %d/%d   Lives: %d", score, WIN_SCORE, lives);
    mvprintw(11, 10, "%-50s", msg);
    mvprintw(12, 10, "%-50s", sub);
    mvprintw(14, 10, "A = Allocate kernel memory");
    mvprintw(15, 10, "F = Free kernel memory");
}