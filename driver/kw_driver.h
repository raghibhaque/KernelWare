#ifndef KW_STATE_H
#define KW_STATE_H

#include <linux/atomic.h>
#include "../shared/kw_ioctl.h"

extern atomic_t open_count;
extern struct kw_state  current_state;
extern struct kw_config current_config;

#endif