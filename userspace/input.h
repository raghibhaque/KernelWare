#ifndef INPUT_H
#define INPUT_H

extern volatile char last_key;
extern volatile int active_game;
extern volatile int input_active;

void input_init(int fd);
void *kw_input_thread(void *arg);

#endif