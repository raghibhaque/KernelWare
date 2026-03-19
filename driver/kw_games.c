#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/random.h>
#include <linux/string.h>
#include <linux/ktime.h>
#include <linux/utsname.h>
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
#define TYPEFASTER_TARGET 20
#define LB_THREAD_COUNT 3

static DECLARE_COMPLETION(pipe_done);
static struct task_struct *pipe_thread;
static int active_game_id;
unsigned int timer_duration_ms = 10000;
static void *memleak_ptr = NULL;
static int typefaster_count = 0;

static int fill_count = 0; // pipe dream

// Load Balancer
static struct task_struct *lb_threads[LB_THREAD_COUNT];
static bool lb_alive[LB_THREAD_COUNT];
static int lb_remaining = 0;

// Hack the Host
static char hack_saved_hostname[65];
static bool hack_won = false;

// Rot Brain
static int rot_n = 0;
static char rot_answer[64];


void kw_set_timer_ms(unsigned int ms) {
    timer_duration_ms = ms;
}



static int pipe_writer_fn(void *data) {
    while (!kthread_should_stop()) {
        msleep(FILL_INTERVAL);
        if (kthread_should_stop())
            break;

        if (fill_count >= PIPE_BUF_MAX) {
            kernel_buf[0] = KW_EVENT_TIMEOUT;
            buf_len = 1;
            fill_count = 0;
        } else {
            fill_count++;
            kernel_buf[0] = FILL_BYTE;
            buf_len = 1;
            current_state.score = (fill_count * 100) / PIPE_BUF_MAX;
        }

        data_ready = 1;
        wake_up_interruptible(&my_wq);
    }
    complete(&pipe_done);
    return 0;
}


// Load Balancer
static int lb_thread_fn(void *data)
{
    while (!kthread_should_stop())
        schedule();
    return 0;
}

unsigned char lb_kill_thread(const char *input)
{
    int id = input[0] - '0';
    if (id < 1 || id > LB_THREAD_COUNT)
        return 0x02;

    int idx = id - 1;
    if (!lb_alive[idx])
        return 0x02;

    kthread_stop(lb_threads[idx]);
    lb_threads[idx] = NULL;
    lb_alive[idx] = false;
    lb_remaining--;

    return (lb_remaining <= 0) ? KW_EVENT_CORRECT : 0x01;
}


// Hack the Host
void kw_hackhost_win(void) { hack_won = true; }

int hackhost_change(const char *new_name, int len)
{
    if (len <= 0 || len > __NEW_UTS_LEN)
        return 0;
    strncpy(init_uts_ns.name.nodename, new_name, __NEW_UTS_LEN);
    init_uts_ns.name.nodename[__NEW_UTS_LEN] = '\0';
    return 1;
}


//Rot Brain
static void rotbrain_apply(const char *input, char *output, int n)
{
    for (int i = 0; input[i]; i++) {
        char c = input[i];
        if (c >= 'a' && c <= 'z')
            output[i] = 'a' + (c - 'a' + n) % 26;
        else if (c >= 'A' && c <= 'Z')
            output[i] = 'A' + (c - 'A' + n) % 26;
        else
            output[i] = c;
    }
    output[strlen(input)] = '\0';
}


void rotbrain_start(void)
{
    const char *words[] = {"kernel", "module", "driver", "process", "thread"};
    int word_idx = get_random_u32() % 5;
    rot_n = (get_random_u32() % 12) + 1;  // ROT-1 to ROT-12

    strncpy(rot_answer, words[word_idx], sizeof(rot_answer) - 1);
    rotbrain_apply(words[word_idx], current_state.prompt, rot_n);

    current_state.score = 0;
    data_ready = 0;
}


int rotbrain_check_answer(const char *input)
{
    return strncmp(input, rot_answer, strlen(rot_answer)) == 0;
}



int kw_game_start(int game_id) {
    if (game_id == 0)
        game_id = kw_games_pick(active_game_id);

    active_game_id = game_id;
    current_state.game_id = game_id;
    fill_count = 0;
    reinit_completion(&pipe_done);

    data_ready = 0;
    buf_len = 0;
    kernel_buf[0] = 0;

    current_state.deadline_ns = ktime_get_ns() + (u64)timer_duration_ms * 1000000ULL;

    if (memleak_ptr) {
        kfree(memleak_ptr);
        memleak_ptr = NULL;
    }

    if (game_id == 2) {
        rotbrain_start();
        kw_timer_start(timer_duration_ms);
        return 0;
    }

    if (game_id == 3) {
        current_state.prompt[0] = '\0';  // userspace sets the real PID via KW_IOCTL_SET_PROMPT
        kw_timer_start(timer_duration_ms);
        return 0;
    }

    if (game_id == 4) {
        kw_timer_start(timer_duration_ms);
        return 0;
    }

    if (game_id == 5) {
        typefaster_count = 0;
        current_state.score = 0;
        current_state.prompt[0] = '\0';
        kw_timer_start(timer_duration_ms);
        return 0;
    }

    if (game_id == 6) {
        lb_remaining = 0;
        for (int i = 0; i < LB_THREAD_COUNT; i++) {
            lb_alive[i] = false;
            lb_threads[i] = kthread_run(lb_thread_fn, NULL, "kw_lb_%d", i + 1);
            if (!IS_ERR(lb_threads[i])) {
                lb_alive[i] = true;
                lb_remaining++;
            } else {
                lb_threads[i] = NULL;
            }
        }
        snprintf(current_state.prompt, sizeof(current_state.prompt), "1 2 3");
        kw_timer_start(timer_duration_ms);
        return 0;
    }

    if (game_id == 7) {
        strncpy(hack_saved_hostname, init_uts_ns.name.nodename, sizeof(hack_saved_hostname) - 1);
        hack_saved_hostname[sizeof(hack_saved_hostname) - 1] = '\0';
        strncpy(current_state.prompt, hack_saved_hostname, sizeof(current_state.prompt) - 1);
        kw_timer_start(timer_duration_ms);
        return 0;
    }

    // game 1 - Pipe Dream
    pipe_thread = kthread_run(pipe_writer_fn, NULL, "kw_pipe_writer");
    if (IS_ERR(pipe_thread)) {
        int err = PTR_ERR(pipe_thread);
        pipe_thread = NULL;
        return err;
    }
    kw_timer_start(timer_duration_ms);
    return 0;
}

int kw_games_pick(int prev) {
    int ids[] = {1, 2, 3, 4, 5, 6, 7};
    int n = 7;
    int pick;
    do {
        pick = ids[get_random_u32() % n];
    } while (pick == prev && n > 1);
    return pick;
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

    for (int i = 0; i < LB_THREAD_COUNT; i++) {
        if (lb_threads[i]) {
            kthread_stop(lb_threads[i]);
            lb_threads[i] = NULL;
            lb_alive[i] = false;
        }
    }
    lb_remaining = 0;

    if (hack_saved_hostname[0] && !hack_won) {
        strncpy(init_uts_ns.name.nodename, hack_saved_hostname, __NEW_UTS_LEN);
        init_uts_ns.name.nodename[__NEW_UTS_LEN] = '\0';
    }
    hack_saved_hostname[0] = '\0';
    hack_won = false;

    reinit_completion(&pipe_done);
}


// Pipe dream
void pipe_drain(void)
{
    if (fill_count > 0)
        fill_count -= 5;
    if (fill_count < 0)
        fill_count = 0;

    current_state.score = (fill_count * 100) / PIPE_BUF_MAX;
}

// call in kernelware_main.c and processes a button press in "kw_write" for the kill it mini-game
// void kw_game_handle_input(unsigned char event) {
//     current_state.score = (fill_count * 100) / PIPE_BUF_MAX;
// }

void kw_game_handle_input(unsigned char event)
{
    switch (active_game_id) {
    case 1:  // Pipe Dream
        if (KW_EVENT_IS_PRESS(event))
            pipe_drain();
        break;

    case 4:  // Memory Leak — btn 0 = A key (alloc), btn 5 = F key (free)
        if (!KW_EVENT_IS_PRESS(event)) break;
        if (KW_EVENT_GET_BTN(event) == 0) {  // A
            if (memleak_ptr == NULL) {
                memleak_ptr = kmalloc(1024, GFP_KERNEL);
                if (memleak_ptr) {
                    kernel_buf[0] = 0x01;
                } else {
                    kernel_buf[0] = 0x00;
                }
            } else {
                current_state.lives--;
                kernel_buf[0] = 0x02;
            }
        } else if (KW_EVENT_GET_BTN(event) == 5) {  // F
            if (memleak_ptr != NULL) {
                kfree(memleak_ptr);
                memleak_ptr = NULL;
                current_state.score++;
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
    
        case 5 : 
            typefaster_count++;
            current_state.score = (typefaster_count * 100) / TYPEFASTER_TARGET;
            if (current_state.score > 100) current_state.score = 100;
            if (typefaster_count >= TYPEFASTER_TARGET)
            game_state.answer_correct = true;
            kernel_buf[0] = (unsigned char)(current_state.score & 0xFF);
            buf_len = 1;
            data_ready = 1;
            wake_up_interruptible(&my_wq);
            break;
    }
}
