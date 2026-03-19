# Adding a Minigame to KernelWare

KernelWare follows a WarioWare-style flow: the kernel driver picks a random game, starts a timer, and sends events to userspace. Adding a new minigame requires changes in both the **driver** and **userspace**.

---

## Overview

Each minigame has two sides:
- **Driver side** (`driver/kw_games.c`) — assigns a game ID, sets up any kernel-side state (e.g. a prompt), and handles input/write events from userspace.
- **Userspace side** (`userspace/games/`) — implements the game loop (`run`) and the ncurses display (`draw`).

---

## Step 1: Assign a Game ID

Open `driver/kw_games.c` and add your new game's ID to the `ids[]` array in `kw_games_pick`:

```c
int ids[] = {1, 2, 3};  // add your new ID here
int n = 3;
```

---

## Step 2: Set Up Kernel-Side State (Driver)

In `kw_game_start()` in `driver/kw_games.c`, add a case for your game ID. Use this to set up any data userspace will need (e.g. a prompt string):

```c
if (game_id == 3) {
    // example: set a prompt that userspace can read via KW_IOCTL_GET_STATE
    snprintf(current_state.prompt, sizeof(current_state.prompt), "your prompt here");
    kw_timer_start(timer_duration_ms);
    return 0;
}
```

If your game needs to react to input sent via `write()` from userspace, add a case in `kw_game_handle_input()`:

```c
case 3:
    // kernel_buf contains the bytes written by userspace
    // set kernel_buf[0] to a result byte, then set buf_len = 1
    // userspace will read this result back
    kernel_buf[0] = 0x01;  // e.g. 0x01 = correct, 0x00 = wrong
    buf_len = 1;
    break;
```

---

## Step 3: Create the Userspace Game File

Create `userspace/games/game_yourname.c`. It must implement two functions:

```c
int  game_yourname_run(int fd);   // returns 1 on win, 0 on loss/timeout
void game_yourname_draw(void);    // called by the render thread ~20fps
```

### The `run` function

This runs in its own thread. Read from `fd` to receive events:

- `KW_EVENT_TIMEOUT` (0xFF) — the timer expired; return 0 (loss)
- `KW_EVENT_IS_PRESS(event)` — a key was pressed

Use `game_shared` and `game_mutex` to pass display state to the render thread:

```c
#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include "../../shared/kw_ioctl.h"
#include "games.h"

int game_yourname_run(int fd)
{
    GAME_SET_MSG("Your prompt here", "hint text");

    unsigned char event;
    while (1) {
        ssize_t n = read(fd, &event, 1);
        if (n <= 0) break;

        if (event == KW_EVENT_TIMEOUT) {
            GAME_SET_MSG("TIME'S UP!", "");
            sleep(2);
            return 0;
        }

        if (KW_EVENT_IS_PRESS(event)) {
            // handle keypress — write to fd if you need kernel to evaluate input
            // call ioctl(fd, KW_IOCTL_STOP) and return 1 on win
        }
    }
    return 0;
}
```

You can also call `ioctl(fd, KW_IOCTL_GET_STATE, &state)` at any time to read `state.prompt`, `state.score`, etc.

### The `draw` function

Called by the render thread. Use ncurses `mvprintw` / `mvaddch` to draw your game UI. Always lock `game_mutex` when reading `game_shared`:

```c
void game_yourname_draw(void)
{
    pthread_mutex_lock(&game_mutex);
    char msg[128], sub[128];
    strncpy(msg, game_shared.message, 128);
    strncpy(sub, game_shared.subtext,  128);
    pthread_mutex_unlock(&game_mutex);

    mvprintw(7,  10, "=== YOUR GAME ===");
    mvprintw(9,  10, "%-40s", msg);
    mvprintw(10, 10, "%-40s", sub);
}
```

Draw below row 6 — rows 1-5 are reserved for the global header (title, score/lives, timer bar).

---

## Step 4: Register the Game

In `userspace/games/games.c`, declare your functions and add an entry to the `games[]` array:

```c
int  game_yourname_run(int fd);
void game_yourname_draw(void);

game_def_t games[] = {
    { .name = "PIPE DREAM - ...", .run = game_pipedream_run, .draw = game_pipedream_draw },
    { .name = "KILL IT - ...",    .run = game_killit_run,    .draw = game_killit_draw    },
    { .name = "YOUR GAME - ...",  .run = game_yourname_run,  .draw = game_yourname_draw  },
};
int num_games = sizeof(games) / sizeof(games[0]);
```

The index in this array (1-based) must match the game ID you assigned in Step 1.

---

## Key Interfaces Summary

| What | Where | Notes |
|---|---|---|
| Game ID list | `driver/kw_games.c` — `kw_games_pick()` | Add your ID to `ids[]` |
| Kernel setup | `driver/kw_games.c` — `kw_game_start()` | Set prompt, start timer |
| Kernel input handler | `driver/kw_games.c` — `kw_game_handle_input()` | React to userspace `write()` |
| Shared display state | `userspace/games/games.h` — `game_shared_t` | Use `game_mutex` to access |
| Events from driver | `read(fd, &event, 1)` | `KW_EVENT_TIMEOUT`, `KW_EVENT_IS_PRESS` |
| Driver state | `ioctl(fd, KW_IOCTL_GET_STATE, &state)` | Reads `kw_state` struct |
| Signal win | `ioctl(fd, KW_IOCTL_STOP)` then `return 1` | Call from `run()` |
| Signal loss | `return 0` from `run()` | After showing a message |
