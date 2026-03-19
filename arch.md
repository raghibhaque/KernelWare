# KernelWare Architecture

A WarioWare-style micro-game engine where mini-games are driven by a Linux kernel module. `./game` is the userspace binary that communicates with `/dev/kernelware`.

---

## Startup Sequence

```
./game
  |
  |-- open("/dev/kernelware")         // fail fast if driver not loaded
  |-- input_init(driverFD)            // store fd for input thread
  |-- initscr() / noecho() / curs_set // ncurses setup
  |
  |-- pthread_create --> input_thread   (thread 0)
  |-- pthread_create --> render_thread  (thread 1)
  |-- pthread_create --> game_thread    (thread 2)
  |
  `-- pthread_join x3                 // main blocks forever
```

---

## Thread Overview

### Thread 0 — Input Thread (`input.c`)

Reads key events and forwards them to the driver.

```
kw_input_thread()
  |
  |-- open(KW_KBD_DEV or /dev/input/event18)
  |     |-- success --> hardware keyboard loop
  |     `-- fail    --> ssh_input_loop() (reads raw stdin chars)
  |
  |-- [hardware loop]
  |     read(kbd_fd, struct input_event)
  |     if EV_KEY + key_down:
  |       if currentScreen <= 0  --> currentScreen++  (advance menu)
  |       else if mapped key (A/S/D/F):
  |         last_key = char
  |         write(driverFD, KW_EVENT_BTN(i))  --> kernel input handler
  |       if nav key (1/2/3):
  |         currentScreen = 1  (start signal)
  |
  `-- [ssh loop]
        read(STDIN_FILENO, char)
        if currentScreen <= 0 --> currentScreen++
        else if mapped char:
          last_key = char
          write(driverFD, KW_EVENT_BTN(i))
```

### Thread 1 — Render Thread (`main.c`)

Redraws the ncurses screen at ~20 fps.

```
render_thread()
  loop every 50ms:
    if input_active --> skip (game owns the terminal)
    clear()
    switch currentScreen:
      -1  --> GAME OVER screen + final score
       0  --> Start / title screen
      1-N --> HUD (score, lives, time bar)
              ioctl(KW_IOCTL_GET_STATE) --> compute time-remaining %
              games[currentScreen-1].draw()  --> per-game overlay
    refresh()
```

### Thread 2 — Game Loop Thread (`main.c`)

Orchestrates game sessions and delegates to mini-games.

```
game_thread()
  loop:
    wait until currentScreen != 0      // player pressed start

    // Session init
    ioctl(KW_IOCTL_SET_DIFF, 10000ms)  // initial timer
    game_shared.lives = 3, score = 0
    ioctl(KW_IOCTL_START, 0)           // kernel picks first game

    // Inner game loop
    loop:
      ioctl(KW_IOCTL_GET_STATE) --> game_id
      if game_id out of range --> break

      currentScreen = game_id
      won = games[game_id-1].run(driverFD)   // blocking mini-game

      update score / lives
      if lives == 0 --> break

      // Difficulty ramp: -10% timer every 3 wins (floor 2000ms)
      if won && score % 3 == 0:
        timer_ms = timer_ms * 9 / 10
        ioctl(KW_IOCTL_SET_DIFF, timer_ms)

      ioctl(KW_IOCTL_START, next_game_id)    // start next game in kernel

    currentScreen = -1   // GAME OVER
    wait for any key --> loop back to start
```

---

## Mini-Games

Each game implements `game_def_t { .run, .draw }` registered in `games/games.c`.

### Game 1 — Pipe Dream (`game_pipedream.c`)

```
game_pipedream_run(fd):
  fill = 100%
  loop:
    read(fd, event)           // blocking: waits for kernel event
    if KW_EVENT_TIMEOUT  --> lose (return 0)
    if KW_EVENT_IS_PRESS --> fill -= 5%
      if fill == 0:
        ioctl(KW_IOCTL_STOP) --> win (return 1)
```

Player presses A/S/D/F to drain a pipe from 100% to 0% before the timer fires.

### Game 2 — Kill It (`game_killit.c`)

```
game_killit_run(fd):
  loop:
    ioctl(KW_IOCTL_GET_STATE) --> state.prompt = random PID string
    if state.score >= 100 --> timeout, lose

    input_active = 1          // hand terminal control to this game
    getch() loop:             // reads typed digits with 100ms timeout
      check state.score >= 100 each frame --> timed_out
      collect digits until Enter

    write(fd, typed_pid)      // send to kernel for comparison
    read(fd, result)
    if result == 0x01 --> correct, ioctl(STOP), win (return 1)
    else              --> "Wrong! Try again"
```

Player types the PID shown on screen before the timer expires.

---

## Kernel Driver (`/dev/kernelware`)

### Module Init (`kernelware_main.c`)

```
my_module_init():
  alloc_chrdev_region()    // register char device
  cdev_init / cdev_add     // attach file_operations
  class_create / device_create  --> /dev/kernelware (mode 0666)
  my_proc_init()           // /proc/kernelware_proc entry
  kw_timer_init()          // init hrtimer
  kw_state_init()          // init spinlock + wait queues
```

### File Operations

| Operation | Behavior |
|-----------|----------|
| `open`    | Enforces single-opener (atomic). Clears state. |
| `release` | Calls `kw_game_stop()`, decrements open count. |
| `write`   | Copies bytes from userspace; calls `kw_game_handle_input()`; wakes read waitqueue. |
| `read`    | Blocks on `wait_event_interruptible(my_wq)` until `data_ready`. Returns event byte. |
| `ioctl`   | See table below. |

### ioctl Commands

| Command | Direction | Action |
|---------|-----------|--------|
| `KW_IOCTL_START` | `_IO` | `kw_game_start(game_id)` — picks game, arms timer |
| `KW_IOCTL_STOP` | `_IO` | `kw_game_stop()` + `kw_state_next_game()` |
| `KW_IOCTL_SET_DIFF` | `_IOW` | Sets `timer_duration_ms` |
| `KW_IOCTL_GET_STATE` | `_IOR` | Copies `kw_state` to userspace; computes `score` as % of elapsed time |
| `KW_IOCTL_SET_CONFIG` | `_IOW` | Sets lives/difficulty from `kw_config` struct |

### Timer (`kw_timer.c`)

```
kw_timer_start(ms):
  hrtimer_start(CLOCK_MONOTONIC, ms)

kw_timer_callback() [interrupt context]:
  if answer_correct --> kw_state_next_game()
  else              --> kw_state_timeout()
  kernel_buf[0] = KW_EVENT_TIMEOUT
  wake_up_interruptible(&my_wq)   // unblocks userspace read()
```

### Game Logic (`kw_games.c`)

```
kw_game_start(game_id):
  if game_id == 0 --> kw_state_start_round(), pick random first game
  set deadline_ns = now + timer_duration_ms
  if game_id == 2 (Kill It): generate random PID into state.prompt
  kw_timer_start(timer_duration_ms)

kw_game_handle_input(event):  [called from kw_write]
  case 2 (Kill It):
    strcmp(kernel_buf, state.prompt)
    match   --> kernel_buf[0] = 0x01 (correct)
    no match --> kernel_buf[0] = 0x00 (wrong)
    wake read waitqueue

kw_games_pick(prev):
  random pick from {1, 2}, never same as prev
```

---

## Data Flow Diagram

```
Keyboard / SSH
     |
     v
input_thread
     |  write(driverFD, KW_EVENT_BTN)
     v
/dev/kernelware  <-- kw_write --> kw_game_handle_input()
     |                                     |
     | wake my_wq                          | update kernel_buf
     v                                     |
game_thread                                |
  game_N.run() <-- blocking read() <-------+
     |
     |  ioctl(KW_IOCTL_STOP / START / GET_STATE / SET_DIFF)
     v
kw_ioctl() [driver]
     |
     +-- kw_game_start() --> kw_timer_start()
     |                            |
     |                    [hrtimer fires]
     |                     kw_timer_callback()
     |                     writes KW_EVENT_TIMEOUT
     |                     wakes my_wq
     v
game_thread receives timeout event --> return 0 (lose)

render_thread
  every 50ms:
    ioctl(KW_IOCTL_GET_STATE)   // reads deadline_ns, computes time %
    games[id].draw()            // reads game_shared (mutex-protected)
```

---

## Shared State

| Variable | Type | Protected by | Written by | Read by |
|----------|------|-------------|------------|---------|
| `game_shared` | `game_shared_t` | `game_mutex` | game `.run()` | render_thread |
| `currentScreen` | `volatile int` | none (atomic writes) | game_thread, input_thread | all threads |
| `input_active` | `volatile int` | none | game_killit | input_thread, render_thread |
| `last_key` | `volatile char` | none | input_thread | (legacy, render) |
| `current_state` | `kw_state` | kernel only | kw_game_start, kw_ioctl | kw_ioctl GET_STATE |
| `game_state` | `kw_game_state` | `spinlock` | kw_state_* | kw_timer_callback |

---

## File Structure

```
KernelWare/
|-- shared/
|   `-- kw_ioctl.h          # ioctl cmds, kw_state, kw_config structs (kernel+user)
|
|-- driver/                 # Linux kernel module
|   |-- kernelware_main.c   # char device init, file_operations (open/read/write/ioctl)
|   |-- kw_games.c          # game start/stop/input dispatch, pipe_writer kthread
|   |-- kw_timer.c          # hrtimer setup and timeout callback
|   |-- kw_state.c          # game_state spinlock struct, round/next/timeout/reset
|   |-- kernelware_proc.c   # /proc/kernelware_proc read handler
|   |-- kernelware.h        # shared kernel globals (kernel_buf, my_wq, current_state)
|   |-- kw_driver.h         # drv_state struct
|   |-- kw_games.h
|   |-- kw_state.h
|   `-- kw_timer.h
|
|-- userspace/              # ./game binary
|   |-- main.c              # entry point, 3 pthreads, render loop
|   |-- input.c             # keyboard event device + SSH fallback input
|   |-- input.h
|   `-- games/
|       |-- games.h         # game_def_t interface, game_shared_t, GAME_SET_MSG macro
|       |-- games.c         # games[] registry
|       |-- game_pipedream.c
|       `-- game_killit.c
|
`-- scripts/
    |-- load.sh             # insmod + module setup
    |-- unload.sh           # rmmod
    `-- QuickStart.sh       # build driver, load, build userspace
```
