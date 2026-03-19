#include "input.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <stdlib.h>
#include "../shared/kw_ioctl.h"

static int drv_fd = -1;

// To update the menu screen
extern volatile int currentScreen;
extern volatile int input_active;

void input_init(int fd)
{
    drv_fd = fd;
}

// Maps linux keycodes to a button index and character
struct key_mapping {
    int  keycode;
    int  btn_index;
    char character;
};
// Same but for mapping number keycodes to screens
struct nav_mapping {
    int keycode;
    int screen;
};

//Maps asdf keys, to add more keys ADD TO STRUCT
static struct key_mapping keymap[] = {
    { KEY_A, 0, 'a' },
    { KEY_S, 1, 's' },
    { KEY_D, 2, 'd' },
    { KEY_F, 3, 'f' },
};
static int keymap_size = sizeof(keymap) / sizeof(keymap[0]);


//For Nav keys - all route to screen 1 (start signal)
static struct nav_mapping navmap[] = {
    { KEY_1, 1 },
    { KEY_2, 1 },
    { KEY_3, 1 },
};
static int navmap_size = sizeof(navmap) / sizeof(navmap[0]);


static int kw_getch(void)
{
    unsigned char c;
    read(STDIN_FILENO, &c, 1);
    return c;
}

// logic to manage user ssh input
static void ssh_input_loop(void)
{
    while (1) {
        // yield stdin to the game when it's handling its own input (e.g. kill-it)
        if (input_active) { usleep(10000); continue; }

        int c = kw_getch();

        if (currentScreen <= 0) {
            currentScreen++;  // -1 (game over) -> 0 (start), 0 (start) -> 1 (game)
            continue;
        }

        for (int i = 0; i < keymap_size; i++) {
            if (keymap[i].character == c) {
                last_key = c;
                if (drv_fd >= 0) {
                    unsigned char event_byte = KW_EVENT_BTN(keymap[i].btn_index);
                    if (write(drv_fd, &event_byte, 1) < 0)
                        perror("input: write to driver");
                }
                break;
            }
        }
    }
}

void *kw_input_thread(void *arg) // arg isn't used but compiler will give a waring as in pthread signature
{
    (void)arg;

    struct input_event ev;

    const char *kbd_dev = getenv("KW_KBD_DEV");
    if (!kbd_dev)
        kbd_dev = "/dev/input/event18";  // fallback default

    int kbd_fd = open(kbd_dev, O_RDONLY);
    if (kbd_fd < 0) {
        ssh_input_loop();
        return NULL;
    }

    while (1)
    {
        ssize_t n = read(kbd_fd, &ev, sizeof(ev));
        if (n < (ssize_t)sizeof(ev)) {
            perror("input: read error");
            break;
        }

        if (ev.type != EV_KEY || ev.value != 1)
            continue;

        // Checks keypressa, sets last key,and writes to driver
        for (int i = 0; i < keymap_size; i++)
        {
            if (keymap[i].keycode != ev.code)
                continue;

            if (currentScreen <= 0) { currentScreen++; break; }
            if (input_active) break;

            last_key = keymap[i].character;

            if (drv_fd >= 0) {
                unsigned char event_byte = KW_EVENT_BTN(keymap[i].btn_index);
                if (write(drv_fd, &event_byte, 1) < 0)
                    perror("input: write to driver");
            }

            break;
        }

        // checks nav keys
        for (int i = 0; i < navmap_size; i++) {
            if (navmap[i].keycode == ev.code) {
                currentScreen = navmap[i].screen;
                break;
            }
}
    }

    close(kbd_fd);
    return NULL;
}
