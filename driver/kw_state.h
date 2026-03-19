// kw_state.h - Game state machine interface

// This manages the central game state that is shared between:
// The timer callback (runs in interrupt context)
// The read/write handlers (runs in process context)
// The ioctl handlers (runs in process context)

#ifndef KW_STATE_H
#define KW_STATE_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

struct kw_game_state {
    char username[16];
    int current_game_id;
    int score;
    int lives;
    int difficulty; // scaled x10: 10 = 1.0x, 9 = 0.9x, etc.
    u64 deadline_ns; // time to complete mini-game (nanoseconds)
    int games_played;
    bool game_active;
    bool answer_correct;
    bool new_game_ready;
    int previous_game;

    char prompt[256];
    char answer_buffer[64];

    // Buttons pressed? - defined in ioctl
    // Count button presses and append to user's proc

    spinlock_t lock;
    wait_queue_head_t read_queue;
    wait_queue_head_t write_queue;
};

extern struct kw_game_state game_state;

void kw_state_init(void); // Call this function during the module initalisation

void kw_state_start_round(char playerName[16]); // start game.

void kw_state_next_game(void); // next mini-game.

void kw_state_timeout(void); // timed out - failed to answer within the time limit.

void kw_state_reset(void); // restart / close game.

int kw_state_get_info(char *buf, size_t size); // Writes to given buffer and returns the byte size of the uploaded content

#endif