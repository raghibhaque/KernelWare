#include "input.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "games/games.h"
#include "../shared/kw_ioctl.h"

static int drv_fd = -1;

// To update the menu screen
extern volatile int currentScreen;

extern game_def_t games[];
extern int num_games;

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
    { KEY_B, 1, 'b' },
    { KEY_C, 2, 'c' },
    { KEY_D, 3, 'd' },
    { KEY_E, 4, 'e' },
    { KEY_F, 5, 'f' },
    { KEY_G, 6, 'g' },
    { KEY_H, 7, 'h' },
    { KEY_I, 8, 'i' },
    { KEY_J, 9, 'j' },
    { KEY_K, 10, 'k' },
    { KEY_L, 11, 'l' },
    { KEY_M, 12, 'm' },
    { KEY_N, 13, 'n' },
    { KEY_O, 14, 'o' },
    { KEY_P, 15, 'p' },
    { KEY_Q, 16, 'q' },
    { KEY_R, 17, 'r' },
    { KEY_S, 18, 's' },
    { KEY_T, 19, 't' },
    { KEY_U, 20, 'u' },
    { KEY_V, 21, 'v' },
    { KEY_W, 22, 'w' },
    { KEY_X, 23, 'x' },
    { KEY_Y, 24, 'y' },
    { KEY_Z, 25, 'z' },
};
static int keymap_size = sizeof(keymap) / sizeof(keymap[0]);


//For Nav keys
static struct nav_mapping navmap[] = {
    { KEY_1, 1 },
    { KEY_2, 2 },
    { KEY_3, 3 },
};
static int navmap_size = sizeof(navmap) / sizeof(navmap[0]);


void *kw_input_thread(void *arg) // arg isn't used but compiler will give a waring as in pthread signature
{
    (void)arg;

    struct input_event ev;

    const char *kbd_dev = getenv("KW_KBD_DEV");
    if (!kbd_dev)
        kbd_dev = "/dev/input/event18";  // fallback default

    int kbd_fd = open(kbd_dev, O_RDONLY);
    if (kbd_fd < 0) {
        perror("input: failed to open keyboard device");
        fprintf(stderr, "Hint: set KW_KBD_DEV=/dev/input/eventX\n");
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
        
        // checks nav keys
        if (active_game == -1) {
            for (int i = 0; i < navmap_size; i++) {
                if (navmap[i].keycode == ev.code) {
                    currentScreen = navmap[i].screen;
                    break;
                }
            }
        }
    

        //Finds the games input mode
        int game_idx = currentScreen - 1;
        input_mode_t mode = (game_idx >= 0 && game_idx < num_games)
            ? games[game_idx].input_mode : INPUT_MODE_BUTTONS;

        if (mode == INPUT_MODE_BUTTONS) {
            // Checks keypress, sets last key,and writes to driver
            for (int i = 0; i < keymap_size; i++)
            {
                if (keymap[i].keycode != ev.code)
                    continue;

                last_key = keymap[i].character;

                if (drv_fd >= 0) {
                    unsigned char event_byte = KW_EVENT_BTN(keymap[i].btn_index);
                    if (write(drv_fd, &event_byte, 1) < 0)
                        perror("input: write to driver");
                }

                break;
            }
        }

        else if (mode == INPUT_MODE_TEXT)
        {
            if (ev.code == KEY_ENTER)
            {
                pthread_mutex_lock(&game_mutex);
                int len = game_shared.typed_len;
                char to_send[64];
                memcpy(to_send, game_shared.typed, len);
                pthread_mutex_unlock(&game_mutex);

                if (len > 0 && drv_fd >= 0) {
                    if (write(drv_fd, to_send, len) < 0)
                        perror("input: write answer to driver");
                }

                pthread_mutex_lock(&game_mutex);
                memset(game_shared.typed, 0, sizeof(game_shared.typed));
                game_shared.typed_len = 0;
                pthread_mutex_unlock(&game_mutex);
            }
            else if (ev.code == KEY_BACKSPACE)
            {
                pthread_mutex_lock(&game_mutex);
                if (game_shared.typed_len > 0) {
                    game_shared.typed_len--;
                    game_shared.typed[game_shared.typed_len] = '\0';
                }
                pthread_mutex_unlock(&game_mutex);
            }
            else
            {
                for (int i = 0; i < keymap_size; i++) {
                    if (keymap[i].keycode != ev.code)
                        continue;

                    pthread_mutex_lock(&game_mutex);
                    if (game_shared.typed_len < (int)sizeof(game_shared.typed) - 1) {
                        game_shared.typed[game_shared.typed_len++] = keymap[i].character;
                        game_shared.typed[game_shared.typed_len]   = '\0';
                    }
                    pthread_mutex_unlock(&game_mutex);
                    break;
                }
            }
        }


        
    }

    close(kbd_fd);
    return NULL;
}