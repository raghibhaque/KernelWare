#include "input.h"
#include "games/games.h"
#include "../shared/kw_ioctl.h"

#include <stdio.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <ncurses.h>


#define NUMOFTHREADS 3
volatile char last_key = ' '; // shared variable, we dont want the system to optomise it
volatile int currentScreen = 0;
volatile int input_active = 0;
int driverFD = 0;


void drawGame1();



void* thread_function(void* arg) {
    (void)arg;
    if (currentScreen >= 0 && currentScreen < num_games) {
        games[currentScreen].run(driverFD);
    }
    return NULL;
}

void *input_thread(void *arg) {
    return kw_input_thread(arg);  // your function from input.c
}

void* render_thread(void* arg)
{
    initscr();
    noecho();
    curs_set(0);

    
    
    while (1)
    {
        clear();
        mvprintw(1,10,"MiniGame Console");
        mvprintw(2,10,"Press 1-7 to switch games");

        switch(currentScreen)
        {
            case 1: if(num_games > 0) games[0].draw(); break;
            case 2: if(num_games > 1) games[1].draw(); break;
            case 3: if(num_games > 2) games[2].draw(); break;
            case 4: if(num_games > 3) games[3].draw(); break;
            case 5: if(num_games > 4) games[4].draw(); break;
            case 6: if(num_games > 5) games[5].draw(); break;
            case 7: if(num_games > 6) games[6].draw(); break;
            default:
                mvprintw(5, 10, "Press 1-7 to choose a minigame");
        }


        mvprintw(18, 10, "Key Pressed: %c", last_key);
        refresh();

        usleep(50000);   // small delay so cpu isn't maxed
    }

    endwin();
    return NULL;
}
    
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


int main() {



    driverFD = open("/dev/kernelware", O_RDWR);
    if(driverFD < 0)
    {
        perror("Failed to open the driver");
        return 1;
    }
    input_init(driverFD);


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

    if (pthread_create(&threads[2], NULL, thread_function, &ids[2]) != 0) {
        perror("pthread_create");
        return 1;
    }

   
    // wait for the threads
    for (int i = 0; i < NUMOFTHREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}