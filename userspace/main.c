#include <stdio.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <ncurses.h>
#include <sys/ioctl.h>
#include "../shared/kw_ioctl.h"

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
void drawDifficultyMenu();
void drawVictory();
void drawDefeat();


typedef enum {
    SCREEN_SELECT = 0,
    SCREEN_DIFF     = 1,
    SCREEN_PLAYING  = 2,
    SCREEN_VICTORY  = 3,
    SCREEN_DEFEAT   = 4
} GameState;


typedef enum {
    DIFF_NONE   = 0,
    DIFF_EASY   = 1,
    DIFF_MEDIUM = 2,
    DIFF_HARD   = 3
} Difficulty;


typedef struct {
    volatile GameState state;
    volatile Difficulty difficulty;
    volatile int mistakes;
    volatile int timeLeft;
    volatile int score;
    volatile int resetGame;
} GameContext;

GameContext ctx = {SCREEN_SELECT, DIFF_NONE, 0, 30, 0, 0};

int maxMistakes[4] = {0, 5, 3, 1};
int timeLimit[4]   = {0, 60, 45, 30};


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
           
           if (ctx.state == SCREEN_SELECT && last_key >= '1' && last_key <= '7'){
            currentScreen = last_key - '0';
            ctx.state = SCREEN_DIFF;
           }

           if (ctx.state == SCREEN_DIFF) {
            if (last_key == 'e'){
                ctx.difficulty = DIFF_EASY;
            }
            if (last_key == 'm'){
                ctx.difficulty = DIFF_MEDIUM;
            }
            if (last_key == 'h'){
                ctx.difficulty = DIFF_HARD;
            }

            if (ctx.difficulty !=DIFF_NONE)
            {
                ctx.timeLeft  = timeLimit[ctx.difficulty];
                    ctx.mistakes  = 0;
                    ctx.score     = 0;
                    ctx.resetGame = 1;
                    ctx.state     = SCREEN_PLAYING;
            }
            
           }

            if ((ctx.state == SCREEN_VICTORY || ctx.state == SCREEN_DEFEAT) && last_key == 'r')
            {
                currentScreen  = 0;
                ctx.state      = SCREEN_SELECT;
                ctx.difficulty = DIFF_NONE;
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

        switch (ctx.state)
{
    case SCREEN_SELECT:
        mvprintw(5, 10, "Press 1-7 to choose a minigame");
        break;
    case SCREEN_DIFF:
        drawDifficultyMenu();
        break;
    case SCREEN_PLAYING:
        switch (currentScreen)
        {
            case 1:
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
        }
        break;
    case SCREEN_VICTORY:
        drawVictory();
        break;
    case SCREEN_DEFEAT:
        drawDefeat();
        break;
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
    struct kw_state state;

    if (ctx.resetGame) {
        struct kw_config cfg;
        cfg.timeout_ms = timeLimit[ctx.difficulty] * 1000;
        cfg.difficulty = ctx.difficulty;
        cfg.lives      = maxMistakes[ctx.difficulty];
        ioctl(driverFD, KW_IOCTL_SET_CONFIG, &cfg);
        prev_key = ' ';
        ctx.resetGame = 0;
    }

    ioctl(driverFD, KW_IOCTL_GET_STATE, &state);

    char* diff_names[] = {"", "Easy", "Medium", "Hard"};
    mvprintw(4,  10, "MEMORY LEAK GAME");
    mvprintw(5,  10, "Difficulty: %s | Lives: %d | Time: %ds",
             diff_names[ctx.difficulty], state.lives, ctx.timeLeft);
    mvprintw(7,  10, "A - Allocate Kernel Memory (outstanding: %d)", state.score);
    mvprintw(8,  10, "F - Free Kernel Memory");
    mvprintw(10, 10, "Get allocations to 0 before time runs out!");

    if (last_key != prev_key)
    {
        if (last_key == 'a') write(driverFD, "A", 1);
        if (last_key == 'f') write(driverFD, "F", 1);
        prev_key = last_key;
    }

    if (state.score == 0 && prev_key != ' ')
        ctx.state = SCREEN_VICTORY;

    if (state.lives == 0)
        ctx.state = SCREEN_DEFEAT;
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

void drawDifficultyMenu()
{
    mvprintw(4, 10, "SELECT DIFFICULTY");
    mvprintw(6, 10, "E - Easy   (5 lives | 60s)");
    mvprintw(7, 10, "M - Medium (3 lives | 45s)");
    mvprintw(8, 10, "H - Hard   (1 life  | 30s)");
}

void drawVictory()
{
    char* diff_names[] = {"", "Easy", "Medium", "Hard"};
    mvprintw(4,  10, "*** VICTORY! ***");
    mvprintw(6,  10, "All memory freed successfully!");
    mvprintw(8,  10, "Difficulty : %s", diff_names[ctx.difficulty]);
    mvprintw(9,  10, "Time Left  : %ds", ctx.timeLeft);
    mvprintw(11, 10, "Press R to return to menu");
}

void drawDefeat()
{
    char* diff_names[] = {"", "Easy", "Medium", "Hard"};
    mvprintw(4,  10, "*** DEFEAT ***");
    mvprintw(6,  10, "Too many bad frees!");
    mvprintw(8,  10, "Difficulty : %s", diff_names[ctx.difficulty]);
    mvprintw(9,  10, "Time Left  : %ds", ctx.timeLeft);
    mvprintw(11, 10, "Press R to return to menu");
}


void* timer_thread(void* arg)
{
    while (1)
    {
        if (ctx.state == SCREEN_PLAYING)
        {
            sleep(1);
            ctx.timeLeft--;
            if (ctx.timeLeft <= 0)
                ctx.state = SCREEN_DEFEAT;
        }
        else
        {
            usleep(100000);
        }
    }
    return NULL;
}

int main() {



    driverFD = open("/dev/kw" , O_RDWR);
    if(driverFD < 0)
    {
        perror("Failed to open the driver");
        return 1;
    }


    pthread_t threads[NUMOFTHREADS];
    


    if (pthread_create(&threads[0], NULL, timer_thread, NULL) != 0) {
        perror("pthread_create");
        return 1;
    }

    if (pthread_create(&threads[1], NULL, render_thread, NULL) != 0) {
        perror("pthread_create");
        return 1;
    }

   if (pthread_create(&threads[2], NULL, input_thread, NULL) != 0) {
    perror("pthread_create");
    return 1;
    }

   
    // wait for the threads
    for (int i = 0; i < NUMOFTHREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}