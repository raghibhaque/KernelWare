#include "input.h"
#include "games/games.h"
#include "../shared/kw_ioctl.h"

#include <stdio.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <ncurses.h>
#include <sys/ioctl.h>
#include <string.h>

#define NUMOFTHREADS 3
volatile char last_key = ' ';
volatile int currentScreen = 0;
volatile int input_active = 0;
int driverFD = 0;

volatile int active_game = -1;
//void drawGame1();




void *game_manager_thread(void *arg) {
    (void)arg;
    
    while (1) {
        // wait for start screen input
        while (currentScreen == 0) usleep(100000);

        // reset session
        unsigned int timer_ms = 10000;
        ioctl(driverFD, KW_IOCTL_SET_DIFF, timer_ms);

        pthread_mutex_lock(&game_mutex);
        game_shared.lives = 3;
        game_shared.score = 0;
        pthread_mutex_unlock(&game_mutex);

        ioctl(driverFD, KW_IOCTL_START, 0);

        // game loop
        while (1) {
            struct kw_state state = {0};
            ioctl(driverFD, KW_IOCTL_GET_STATE, &state);
            int game_id = state.game_id;
            if (game_id < 1 || game_id > num_games) break;

            currentScreen = game_id;
            int won = games[game_id - 1].run(driverFD);

            pthread_mutex_lock(&game_mutex);
            if (won) game_shared.score++;
            else     game_shared.lives--;
            int remaining = game_shared.lives;
            int score     = game_shared.score;
            pthread_mutex_unlock(&game_mutex);

            // sync session score/lives into kernel so /proc/kernelware/stats reflects them 
            struct kw_config sync_cfg = { .lives = remaining, .score = score };
            ioctl(driverFD, KW_IOCTL_SET_CONFIG, &sync_cfg);

            if (!won && remaining <= 0) break;

            // speed up timer by 10% every 3 wins
            if (won && score > 0 && score % 3 == 0) {
                timer_ms = timer_ms * 9 / 10;
                if (timer_ms < 2000) timer_ms = 2000;
                ioctl(driverFD, KW_IOCTL_SET_DIFF, timer_ms);
            }

            ioctl(driverFD, KW_IOCTL_START, 0);
        }

        // submit final score to kernel leaderboard
        pthread_mutex_lock(&game_mutex);
        int final_score = game_shared.score;
        pthread_mutex_unlock(&game_mutex);
        ioctl(driverFD, KW_IOCTL_SUBMIT_SCORE, final_score);

        // game over, wait for any key to return to start
        currentScreen = -1;
        while (currentScreen == -1) usleep(100000);
    }

    return NULL;
}


void *input_thread(void *arg) {
    return kw_input_thread(arg);  // your function from input.c
}


void* render_thread(void* arg)
{
    //initscr();
    noecho();
    curs_set(0);

    
    
    (void)arg;
    while (1)
    {
        if (input_active) { usleep(50000); continue; }
        clear();

        if (currentScreen == -1) {
            pthread_mutex_lock(&game_mutex);
            int final_score = game_shared.score;
            pthread_mutex_unlock(&game_mutex);
            mvprintw(5, 10, "=== GAME OVER ===");
            mvprintw(7, 10, "Final Score: %d", final_score);
            mvprintw(9, 10, "Press any key to play again");
        } else if (currentScreen == 0) {
            mvprintw(5, 10, "=== KernelWare ===");
            mvprintw(8, 10, "Press any key to start");
        } else {
            mvprintw(1, 10, "=== KernelWare ===");

            pthread_mutex_lock(&game_mutex);
            int score = game_shared.score;
            int lives = game_shared.lives;
            pthread_mutex_unlock(&game_mutex);

            mvprintw(3, 10, "Score: %-5d  Lives: %d", score, lives);

            struct kw_state tw = {0};
            ioctl(driverFD, KW_IOCTL_GET_STATE, &tw);
            int tw_rem = 100 - tw.score;
            if (tw_rem < 0) tw_rem = 0;
            const int TW = 30;
            int tf = (tw_rem * TW) / 100;
            if (tw_rem < 20) attron(A_BOLD);
            mvprintw(5, 10, "TIME [");
            for (int i = 0; i < TW; i++)
                mvaddch(5, 16 + i, i < tf ? '#' : '-');
            mvprintw(5, 16 + TW, "] %3d%%", tw_rem);
            if (tw_rem < 20) attroff(A_BOLD);

            if (currentScreen >= 1 && currentScreen <= num_games)
                games[currentScreen - 1].draw();
        }

        refresh();
        usleep(50000);
    }

    endwin();
    return NULL;
}

/*
void drawGame1()
{
    static char prev_key = ' ';

    mvprintw(4,10,"MEMORY LEAK GAME");
    mvprintw(6,10,"A - Allocate Kernel Memory");
    mvprintw(7,10,"F - Free Kernel Memory");
    mvprintw(9,10,"Fix the memory leak!");

    if(last_key != prev_key)
    {
        if(last_key == 'a')
            write(driverFD, "A", 1);

        if(last_key == 'f')
            write(driverFD, "F", 1);

        prev_key = last_key;
    }
}
*/

int main() {



    driverFD = open("/dev/kernelware", O_RDWR);
    if(driverFD < 0)
    {
        perror("Failed to open the driver");
        return 1;
    }
    input_init(driverFD);

    initscr();
    noecho();
    curs_set(0);


    pthread_t threads[NUMOFTHREADS];
    int ids[NUMOFTHREADS];
    for (int i = 0; i < NUMOFTHREADS; i++) {
        ids[i] = i;
    }


    if (pthread_create(&threads[0], NULL, input_thread, NULL) != 0) {
        perror("pthread_create");
        return 1;
    }

    if (pthread_create(&threads[1], NULL, render_thread, NULL) != 0) {
        perror("pthread_create");
        return 1;
    }

    if (pthread_create(&threads[2], NULL, game_manager_thread, &ids[2]) != 0) {
        perror("pthread_create");
        return 1;
    }

   
    // wait for the threads
    for (int i = 0; i < NUMOFTHREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}