// ioctl file for kernel and userspace - could be in driver and setup differently in the markfile to reduce the amount of directories
#ifndef KW_IOCTL_H
#define KW_IOCTL_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

struct kw_state {
    int game_id;
    int score;
    int lives;
    int difficulty;
    uint64_t deadline_ns;
    char prompt[256];
};

struct kw_config {
    int timeout_ms;
    int difficulty;
    int lives;
};

struct kw_stats {
    char username[16];
    int games_played;
    int games_won;
    int games_lost;
    int total_buttons_pressed; // Check where this needs to be defined in the project
    int high_score;
    int total_score;
};

// event bytes returned by read()
#define KW_EVENT_TIMEOUT 0xFF
#define KW_EVENT_BTN(i) (((i) << 4) | 0x01)
#define KW_EVENT_GET_BTN(e) (((e) >> 4) & 0x0F)
#define KW_EVENT_IS_PRESS(e) (((e) & 0x0F) == 0x01)

// write() command bytes
#define KW_CMD_START_ROUND 0xA0

// ioctl commands
#define KW_MAGIC 'K'

// IOR == read
// IOW == write
#define KW_IOCTL_START         _IO (KW_MAGIC, 1)
#define KW_IOCTL_STOP          _IO (KW_MAGIC, 2)
#define KW_IOCTL_GET_SCORE     _IOR(KW_MAGIC, 3, int)
#define KW_IOCTL_SET_CONFIG    _IOW(KW_MAGIC, 4, struct kw_config)
#define KW_IOCTL_SET_DIFF      _IOW(KW_MAGIC, 5, int)
#define KW_IOCTL_GET_STATS     _IOR(KW_MAGIC, 6, struct kw_stats)
#define KW_IOCTL_GET_STATE     _IOR(KW_MAGIC, 7, struct kw_state)
#define KW_IOCTL_RESET_SCORE   _IO (KW_MAGIC, 8)

#endif