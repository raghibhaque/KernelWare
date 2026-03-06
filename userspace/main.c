#include <stdio.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <ncurses.h>


#define NUMOFTHREADS 3
volatile char last_key = ' '; // shared variable, we dont want the system to optomise it
volatile int currentScreen = 0;
int driverFD = 0;


void drawGame1();
void drawGame2();
void drawGame3();
void drawGame4();
void drawGame5();
void drawGame6();
void drawGame7();


void* thread_function(void* arg) {
    int id = *(int*)arg;
    printf("Thread %d running\n", id);
    return NULL;
}

void* input_thread(void* arg)
{
    int fd = open("/dev/kw", O_RDONLY);

    if (fd < 0) {
        perror("Failed to open /dev/kw"); // flag since if its less than 0, it has failed
        return NULL;
    }

    char buffer[16];

    while (1)
    {
        int bytes = read(fd, buffer, sizeof(buffer));

        if (bytes > 0)
        {
           last_key = buffer[0];
           
           if (last_key >= '1' && last_key <= '7')
           {
           currentScreen = last_key - '0';
           }
        }
           usleep(1000); 
    }


    

    close(fd);
    return NULL;
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
            case 1 :
            drawGame1();
            break;

            case 2:
            drawGame2();
            break;

        case 3:
            drawGame3();
            break;

        case 4:
            drawGame4();
            break;

        case 5:
            drawGame5();
            break;

        case 6:
            drawGame6();
            break;

        case 7:
            drawGame7();
            break;

        default:
            mvprintw(5,10,"Press 1-7 to choose a minigame");
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

void drawGame2() {
    mvprintw(3, 5, "MINIGAME 2");
}

void drawGame3() {
    mvprintw(3, 5, "MINIGAME 3");
}

void drawGame4() {
    mvprintw(3, 5, "MINIGAME 4");
}

void drawGame5() {
    mvprintw(3, 5, "MINIGAME 5");
}

void drawGame6() {
    mvprintw(3, 5, "MINIGAME 6");
}

void drawGame7() {
    mvprintw(3, 5, "MINIGAME 7");
}

int main() {



    driverFD = open("/dev/kw" , O_RDWR);
    if(driverFD < 0)
    {
        perror("Failed to open the driver");
        return 1;
    }


    pthread_t threads[NUMOFTHREADS];
    int ids[NUMOFTHREADS];
    for (int i = 0; i < NUMOFTHREADS; i++)
     {
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