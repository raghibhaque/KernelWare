/* kw_ioctl.h — included by both driver/ and userspace/ */
#ifndef KW_IOCTL_H
#define KW_IOCTL_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

/* ---- structs ---- */

struct kw_state {
    uint8_t  game_id;
    uint32_t score;
    uint8_t  lives;
    uint8_t  difficulty;
    uint64_t deadline_ns;
    char     prompt[256];
};

struct kw_config {
    uint32_t timeout_ms;
    uint8_t  difficulty;
    uint8_t  lives;
};

struct kw_stats {
    uint32_t games_played;
    uint32_t games_won;
    uint32_t games_lost;
    uint32_t total_buttons_pressed;
    uint32_t high_score;
};

/* ---- event bytes returned by read() ---- */
#define KW_EVENT_TIMEOUT       0xFF
#define KW_EVENT_BTN(i)        (((i) << 4) | 0x01)
#define KW_EVENT_GET_BTN(e)    (((e) >> 4) & 0x0F)
#define KW_EVENT_IS_PRESS(e)   (((e) & 0x0F) == 0x01)

/* ---- write() command bytes ---- */
#define KW_CMD_START_ROUND     0xA0

/* ---- ioctl commands ---- */
#define KW_MAGIC               'K'

#define KW_IOCTL_START         _IO (KW_MAGIC, 1)  /* start a round          */
#define KW_IOCTL_STOP          _IO (KW_MAGIC, 2)  /* abort current round    */
#define KW_IOCTL_GET_SCORE     _IOR(KW_MAGIC, 3, uint32_t)
#define KW_IOCTL_SET_CONFIG    _IOW(KW_MAGIC, 4, struct kw_config)
#define KW_IOCTL_SET_DIFF      _IOW(KW_MAGIC, 5, uint8_t)
#define KW_IOCTL_GET_STATS     _IOR(KW_MAGIC, 6, struct kw_stats)
#define KW_IOCTL_GET_STATE     _IOR(KW_MAGIC, 7, struct kw_state)
#define KW_IOCTL_RESET_SCORE   _IO (KW_MAGIC, 8)

#endif /* KW_IOCTL_H */