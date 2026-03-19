#ifndef KERNELWARE_H
#define KERNELWARE_H

// Shared state that proc file can read
struct my_driver_state {
    int open_count;
    int read_count;
    int write_count;
};

extern struct my_driver_state drv_state;

#define LEADERBOARD_SIZE 5
extern int leaderboard[LEADERBOARD_SIZE];

// Proc lifecycle - called from kernelware_main.c
int  my_proc_init(void);
void my_proc_exit(void);

#endif