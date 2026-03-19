#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <ncurses.h>
#include <sys/ioctl.h>
#include "../../shared/kw_ioctl.h"
#include "games.h"

extern volatile int input_active;
extern volatile int currentScreen;

int game_killit_run(int fd)
{
    // Spawn a real child process to be killed
    pid_t child = fork();
    if (child == 0) {
        prctl(PR_SET_NAME, "kw_killit_target");
        while (1) pause();
        _exit(0);
    }

    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", (int)child);

    // Tell the driver what PID to validate against
    ioctl(fd, KW_IOCTL_SET_PROMPT, pid_str);

    pthread_mutex_lock(&game_mutex);
    snprintf(game_shared.message, 300, "KILL PID: %s", pid_str);
    snprintf(game_shared.subtext,  128, "Type the PID and press Enter");
    game_shared.typed[0] = '\0';
    game_shared.typed_len = 0;
    pthread_mutex_unlock(&game_mutex);

    int won = 0;
    unsigned char event;
    while (1) {
        ssize_t n = read(fd, &event, 1);
        if (n <= 0) break;

        if (event == KW_EVENT_CORRECT) {
            kill(child, SIGKILL);
            waitpid(child, NULL, 0);
            pthread_mutex_lock(&game_mutex);
            snprintf(game_shared.message, 300, "CORRECT! PID %s eliminated.", pid_str);
            game_shared.subtext[0] = '\0';
            pthread_mutex_unlock(&game_mutex);
            won = 1;
            break;
        } else if (event == KW_EVENT_TIMEOUT) {
            kill(child, SIGKILL);
            waitpid(child, NULL, 0);
            pthread_mutex_lock(&game_mutex);
            snprintf(game_shared.message, 300, "TIME'S UP! PID %s escaped.", pid_str);
            game_shared.subtext[0] = '\0';
            pthread_mutex_unlock(&game_mutex);
            break;
        }
    }

    ioctl(fd, KW_IOCTL_STOP);
    sleep(1);
    currentScreen = 0;
    return won;
}

void game_killit_draw(void)
{
    pthread_mutex_lock(&game_mutex);
    char msg[300], sub[128], typed[64];
    strncpy(msg,   game_shared.message, 300);
    strncpy(sub,   game_shared.subtext,  128);
    strncpy(typed, game_shared.typed,    64);
    pthread_mutex_unlock(&game_mutex);

    mvprintw(7,  10, "=== KILL IT ===");
    mvprintw(9,  10, "%-40s", msg);
    mvprintw(10, 10, "%-40s", sub);
    mvprintw(12, 10, "Your input: %s_", typed);
    mvprintw(14, 10, "[0-9] type | [Enter] submit | [Backspace] delete");
}
