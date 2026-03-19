#ifndef KW_TIMER_H
#define KW_TIMER_H

#include <linux/types.h>

int kw_timer_init(void); // Call this function during the module initalisation

// When the timer expires, it will call the registered callback which updates the game state and wakes up waiting threads.
int kw_timer_start(unsigned int timeout_ms); // timout_ms == milliseconds before the timer firse a "failed to submit an answer on time"

void kw_timer_stop(void); // stop current timer

void kw_timer_exit(void); // call this function during the module exit

bool kw_timer_is_active(void); // this doesn't get called (for now)

#endif