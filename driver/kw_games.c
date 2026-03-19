#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/random.h>
#include <linux/string.h>
#include <linux/ktime.h>
#include "../shared/kw_ioctl.h"
#include "kw_games.h"
#include "kw_driver.h"
#include "kw_state.h"
#include "kw_timer.h"
#include <linux/completion.h>
#include <linux/slab.h>

#define PIPE_BUF_MAX ((int)(sizeof(kernel_buf) - 1))
#define FILL_BYTE 0xBB
#define FILL_INTERVAL 200

static DECLARE_COMPLETION(pipe_done);
static struct task_struct *pipe_thread;
static int active_game_id;
unsigned int timer_duration_ms = 10000;
static void *memleak_ptr = NULL;

void kw_set_timer_ms(unsigned int ms) {
    timer_duration_ms = ms;
}

static int fill_count = 0;

static int pipe_writer_fn(void *data) {
    while (!kthread_should_stop()) {
        msleep(FILL_INTERVAL);
        if (kthread_should_stop())
            break;
        kernel_buf[0] = FILL_BYTE;
        buf_len = 1;
        data_ready = 1;
        wake_up_interruptible(&my_wq);
    }
    complete(&pipe_done);
    return 0;
}

int kw_games_pick(int prev) {
    int ids[] = {1, 2};
    int n = 2;
    int pick;
    do {
        pick = ids[get_random_u32() % n];
    } while (pick == prev && n > 1);
    return pick;
}

int kw_game_start(int game_id) {
    if (game_id == 0) {
        kw_state_start_round("");
        game_id = game_state.current_game_id;
    }

    active_game_id = game_id;
    current_state.game_id = game_id;
    current_state.deadline_ns = ktime_get_ns() + (u64)timer_duration_ms * 1000000ULL;

    // clear any stale event from the previous game
    data_ready = 0;
    buf_len = 0;
    kernel_buf[0] = 0;

    if (memleak_ptr) {
    kfree(memleak_ptr);
    memleak_ptr = NULL;
    }

    if (game_id == 2) {
        u32 pid = get_random_u32() % 65534 + 1;
        snprintf(current_state.prompt, sizeof(current_state.prompt), "%u", pid);
        kw_timer_start(timer_duration_ms);
        return 0;
    }

    kw_timer_start(timer_duration_ms);
    return 0;
}

void kw_game_stop(void) {
    kw_timer_stop();
    if (pipe_thread) {
        if (!IS_ERR(pipe_thread))
            kthread_stop(pipe_thread);
        pipe_thread = NULL;
    }

    if (memleak_ptr) {
    kfree(memleak_ptr);
    memleak_ptr = NULL;
    }

    reinit_completion(&pipe_done);
}


// Pipe dream
void pipe_drain(void)
{
    if (fill_count > 0)
        fill_count -= 5;
    if (fill_count < 0)
        fill_count = 0;

    kernel_buf[0] = FILL_BYTE;
    buf_len = 1;
    data_ready = 1;
    wake_up_interruptible(&my_wq);
}


void kw_game_handle_input(unsigned char event)
{
    switch (active_game_id) {
        case 1 :
        if (event == 'A') {
            if (memleak_ptr == NULL) {
                memleak_ptr = kmalloc(1024, GFP_KERNEL);
                if (memleak_ptr) {
                    current_state.score++;
                    kernel_buf[0] = 0x01;  
                } else {
                    kernel_buf[0] = 0x00;  
                }
            } else {
                current_state.lives--;
                kernel_buf[0] = 0x02; // leak
            }
        } else if (event == 'F') {
            if (memleak_ptr != NULL) {
                kfree(memleak_ptr);
                memleak_ptr = NULL;
                kernel_buf[0] = 0x03;      
            } else {
                current_state.lives--;
                kernel_buf[0] = 0x04;   
            }
        }
        buf_len = 1;
        data_ready = 1;
        wake_up_interruptible(&my_wq);
        break;

    case 2:  // kill it -> compare typed PID to prompt
        if (strcmp(kernel_buf, current_state.prompt) == 0) {
            current_state.score++;
            u32 pid = get_random_u32() % 65534 + 1;
            snprintf(current_state.prompt, sizeof(current_state.prompt), "%u", pid);
            kernel_buf[0] = 0x01;
        } else {
            kernel_buf[0] = 0x00;
        }
        buf_len = 1;
        break;
    }
}
