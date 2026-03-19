#ifndef KW_DRIVER_H
#define KW_DRIVER_H

#include <linux/atomic.h>
#include "../shared/kw_ioctl.h"
#include <linux/wait.h>

extern atomic_t open_count;
extern struct kw_state  current_state;
extern struct kw_config current_config;

extern wait_queue_head_t my_wq;
extern int data_ready;
extern char kernel_buf[256];
extern int buf_len;

#endif