#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ncurses.h>
#include <sys/ioctl.h>
#include "../../shared/kw_ioctl.h"
#include "games.h"

extern volatile int input_active;

int game_killit_run(int fd)
{
    pthread_mutex_lock(&game_mutex);
    snprintf(game_shared.message, 128, "Type the PID to kill it!");
    snprintf(game_shared.subtext,  128, "");
    pthread_mutex_unlock(&game_mutex);

    struct kw_state state;
    char input[32];

    while (1) {
        memset(&state, 0, sizeof(state));
        ioctl(fd, KW_IOCTL_GET_STATE, &state);

        if (state.score >= 100)
            break;

        pthread_mutex_lock(&game_mutex);
        snprintf(game_shared.message, 128, "Kill PID: %s", state.prompt);
        pthread_mutex_unlock(&game_mutex);

        memset(input, 0, sizeof(input));
        int len = 0;
        int timed_out = 0;

        input_active = 1;
        usleep(60000);  // let render thread finish its current frame
        clear();
        mvprintw(1,  10, "=== KernelWare ===");
        mvprintw(7,  10, "=== KILL IT ===");
        mvprintw(9,  10, "Kill PID: %s", state.prompt);
        mvprintw(12, 10, "Type the PID and press Enter");
        refresh();

        curs_set(1);
        timeout(100);

        while (len < (int)sizeof(input) - 1) {
            memset(&state, 0, sizeof(state));
            ioctl(fd, KW_IOCTL_GET_STATE, &state);
            if (state.score >= 100) { timed_out = 1; break; }

            pthread_mutex_lock(&game_mutex);
            int cur_score = game_shared.score;
            int cur_lives = game_shared.lives;
            pthread_mutex_unlock(&game_mutex);
            mvprintw(3, 10, "Score: %-5d  Lives: %d", cur_score, cur_lives);

            int tw_rem = 100 - state.score;
            if (tw_rem < 0) tw_rem = 0;
            const int TW = 30;
            int tf = (tw_rem * TW) / 100;
            if (tw_rem < 20) attron(A_BOLD);
            mvprintw(5, 10, "TIME [");
            for (int i = 0; i < TW; i++)
                mvaddch(5, 16 + i, i < tf ? '#' : '-');
            mvprintw(5, 16 + TW, "] %3d%%", tw_rem);
            if (tw_rem < 20) attroff(A_BOLD);

            mvprintw(14, 10, "> %s ", input);
            refresh();

            int c = getch();
            if (c == ERR) continue;
            if (c == '\n' || c == 13) break;
            if ((c == 127 || c == KEY_BACKSPACE) && len > 0)
                input[--len] = '\0';
            else if (c >= '0' && c <= '9')
                input[len++] = (char)c;
        }

        timeout(-1);
        curs_set(0);
        input_active = 0;

        if (timed_out)
            break;

        if (len == 0)
            continue;

        write(fd, input, strlen(input));

        unsigned char result;
        read(fd, &result, 1);

        if (result == 0x01) {
            pthread_mutex_lock(&game_mutex);
            snprintf(game_shared.message, 128, "CORRECT!");
            snprintf(game_shared.subtext, 128, "");
            pthread_mutex_unlock(&game_mutex);
            input_active = 0;
            ioctl(fd, KW_IOCTL_STOP);
            return 1;
        }

        pthread_mutex_lock(&game_mutex);
        snprintf(game_shared.subtext, 128, "Wrong! Try again");
        pthread_mutex_unlock(&game_mutex);
    }

    input_active = 0;
    pthread_mutex_lock(&game_mutex);
    snprintf(game_shared.message, 128, "TIME'S UP!");
    snprintf(game_shared.subtext, 128, "Final score: %d", game_shared.score);
    pthread_mutex_unlock(&game_mutex);
    sleep(2);
    return 0;
}

void game_killit_draw(void)
{
    pthread_mutex_lock(&game_mutex);
    char msg[128], sub[128];
    strncpy(msg, game_shared.message, 128);
    strncpy(sub, game_shared.subtext,  128);
    pthread_mutex_unlock(&game_mutex);

    mvprintw(7,  10, "=== KILL IT ===");
    mvprintw(9,  10, "%-40s", msg);
    mvprintw(10, 10, "%-40s", sub);
    mvprintw(12, 10, "Type the PID and press Enter");
}
