#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include "kw_state.h"
#include "kw_driver.h"
#include "kw_timer.h"
#include "kw_games.h"

struct kw_game_state game_state;

// called in /kernelware_main.c to initalise the game state struct (spinlockm wiat queues) when loading the module
void kw_state_init(void) {
    pr_info("Initialising game state...\n");

    memset(&game_state, 0, sizeof(game_state)); // clears something and then re-initalises spinlocks and waitqueus again.

    spin_lock_init(&game_state.lock);
    init_waitqueue_head(&game_state.read_queue);
    init_waitqueue_head(&game_state.write_queue);

    pr_info("Game state initialised.\n");
}

// called in kw_games.c to advance player to the next game before the time runs out
void kw_state_start_round(char playerName[16]) { // usernames aren't implemented yet - might do leaderboard if we've time
    unsigned long flags;

    pr_info("Starting new round | difficulty=%d\n", game_state.difficulty);

    spin_lock_irqsave(&game_state.lock, flags);

    // Reset game's state
    strncpy(game_state.username, playerName, sizeof(game_state.username));
    game_state.previous_game = -1;
    game_state.current_game_id = kw_games_pick(-1);
    current_state.game_id = game_state.current_game_id;
    game_state.lives = 3;
    game_state.score = 0;
    game_state.difficulty = 10;
    game_state.games_played = 0;
    game_state.game_active = true;
    game_state.answer_correct = false;
    game_state.new_game_ready = true;

    game_state.deadline_ns = ktime_get_ns() + (5ULL * 1000000000ULL);

    wake_up_interruptible(&game_state.read_queue);

    spin_unlock_irqrestore(&game_state.lock, flags);
}

// called in /kernelware_main.c when KW_IOCTL_STOP is called to advance the user to the next games
void kw_state_next_game(void) {
    unsigned long flags;

    if (game_state.games_played % 3 == 0 && game_state.difficulty > 1)
        game_state.difficulty--;

    spin_lock_irqsave(&game_state.lock, flags);

    game_state.previous_game = game_state.current_game_id;
    game_state.current_game_id = kw_games_pick(game_state.previous_game);
    current_state.game_id = game_state.current_game_id;

    game_state.answer_correct = false;
    game_state.deadline_ns = ktime_get_ns() + (5ULL * 1000000000ULL);

    game_state.new_game_ready = true;
    wake_up_interruptible(&game_state.read_queue);

    spin_unlock_irqrestore(&game_state.lock, flags);
}

// called in kw_timer.c - decrements 1 of the user's lives and selects the next game at random
void kw_state_timeout(void) {
    unsigned long flags;

    spin_lock_irqsave(&game_state.lock, flags);

    if (game_state.lives > 0) {
        game_state.lives--;
        if (game_state.lives == 0)
            game_state.game_active = false;
    }

    game_state.previous_game = game_state.current_game_id;
    game_state.current_game_id = kw_games_pick(game_state.previous_game);
    current_state.game_id = game_state.current_game_id;

    game_state.new_game_ready = true;
    wake_up_interruptible(&game_state.read_queue);

    spin_unlock_irqrestore(&game_state.lock, flags);
}

void kw_state_reset(void) { // not called in the project anywhere yet
    unsigned long flags;

    pr_info("Resetting game state.\n");

    spin_lock_irqsave(&game_state.lock, flags);

    game_state.game_active = false;
    game_state.new_game_ready = false;
    game_state.current_game_id = 0;
    game_state.difficulty = 10;
    game_state.games_played = 0;
    game_state.score = 0;
    game_state.lives = 0;
    game_state.answer_correct = false;
    memset(game_state.username, 0, sizeof (game_state.username));
    memset(game_state.prompt, 0, sizeof(game_state.prompt));
    memset(game_state.answer_buffer, 0, sizeof(game_state.answer_buffer));

    spin_unlock_irqrestore(&game_state.lock, flags);

    pr_info("Game state reset complete\n");
}

int kw_state_get_info(char *buf, size_t size) { // not called in the project anywhere yet
    unsigned long flags;
    int len;

    if (!buf || size == 0)
        return -EINVAL;

    spin_lock_irqsave(&game_state.lock, flags);

    len = snprintf(buf, size, // writes game state info to buffer and returns to proc
                    "Player: %s\nGame: %d\nScore: %d\nLives: %d\nGames played: %d\nActive: %s\nPrompt: %s\n",
                    game_state.username,
                    game_state.current_game_id,
                    game_state.score,
                    game_state.lives,
                    game_state.games_played,
                    game_state.game_active ? "yes" : "no",
                    game_state.prompt);

    spin_unlock_irqrestore(&game_state.lock, flags);

    return len;
}
