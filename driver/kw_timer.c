// Kernel timer logic for the mini-games

#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include "kw_timer.h"
#include "kw_state.h"
#include "kw_driver.h"
#include "../shared/kw_ioctl.h"

static struct hrtimer game_timer; // nanosecond kernal timer
static bool timer_active = false;

// this function is called when a game timer hits 0 before the user submits the correct answer/ winning condition
// checks if user answered correctly
// updated game state (next game/ timeout)
static enum hrtimer_restart kw_timer_callback(struct hrtimer *timer) {
    unsigned long flags;
    bool correct;

    spin_lock_irqsave(&game_state.lock, flags);
    correct = game_state.answer_correct;
    spin_unlock_irqrestore(&game_state.lock, flags);

    if (correct)
        kw_state_next_game();
    else
        kw_state_timeout();

    kernel_buf[0] = KW_EVENT_TIMEOUT;
    buf_len = 1;
    data_ready = 1;
    wake_up_interruptible(&my_wq);

    timer_active = false;
    return HRTIMER_NORESTART;
}

// called in kernelware_main.c when loading the module
int kw_timer_init(void) {
    hrtimer_setup(&game_timer, kw_timer_callback,CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    timer_active = false;
    return 0;
}

// called in kw_games.c to start the countdown the mini-games
int kw_timer_start(unsigned int timeout_ms) {
    ktime_t ktime_interval;

    if (timer_active)
        hrtimer_cancel(&game_timer);

    ktime_interval = ktime_set(0, timeout_ms * 1000000ULL);
    hrtimer_start(&game_timer, ktime_interval, HRTIMER_MODE_REL);
    timer_active = true;
    return 0;
}

// called in kw_games.c when the game is stopped prematurely
void kw_timer_stop(void) {
    if (timer_active) {
        hrtimer_cancel(&game_timer);
        timer_active = false;
    }
}

// called in kernelware_main.c to cancel timers and unloading the module
void kw_timer_exit(void) {
    if (timer_active) {
        hrtimer_cancel(&game_timer);
        timer_active = false;
    }
}

bool kw_timer_is_active(void) { // not called in the project anywhere yet
    return timer_active;
}
