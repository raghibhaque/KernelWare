#include <stdio.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <ncurses.h>


#define NUMOFTHREADS 3
volatile char last_key = ' '; // shared variable, we dont want the system to optomise it

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
           usleep(1000); 
        }

        
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
        mvprintw(5, 10, "Key Pressed: %c", last_key);
        refresh();

        usleep(50000);   // small delay so cpu isn't maxed
    }

    endwin();
    return NULL;
}
    


int main() {

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