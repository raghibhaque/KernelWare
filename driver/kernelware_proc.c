#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "kernelware.h"
#include "kw_state.h"

#define PROC_DIR_NAME "kernelware"
#define PROC_FILE_NAME "stats"

static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_stats;

static int my_proc_show(struct seq_file *m, void *v) {
    //drivers
    seq_printf(m, "open_count:  %d\n", drv_state.open_count);
    seq_printf(m, "read_count:  %d\n", drv_state.read_count);
    seq_printf(m, "write_count: %d\n", drv_state.write_count);

    // game state (from kw_ioctl.h)
    char buf[512];
    int len = kw_state_get_info(buf, sizeof(buf));
    if (len > 0)
        seq_printf(m, "%s", buf);
    return 0;
}

static int my_proc_open(struct inode *inode, struct file *file) {
    return single_open(file, my_proc_show, NULL);
}

static const struct proc_ops my_proc_ops = {
    .proc_open    = my_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

int my_proc_init(void) {
    proc_dir = proc_mkdir(PROC_DIR_NAME, NULL);
    if (!proc_dir) {
        pr_err("KernelWare: failed to create /proc/%s\n", PROC_DIR_NAME);
        return -ENOMEM;
    }

    proc_stats = proc_create(PROC_FILE_NAME, 0444, proc_dir, &my_proc_ops);
    if (!proc_stats) {
        pr_err("KernelWare: failed to create /proc/%s/%s\n", PROC_DIR_NAME, PROC_FILE_NAME);
        remove_proc_entry(PROC_DIR_NAME, NULL);
        return -ENOMEM;
    }

    pr_info("KernelWare: created /proc/%s/%s\n", PROC_DIR_NAME, PROC_FILE_NAME);
    return 0;
}

void my_proc_exit(void) {
    remove_proc_entry(PROC_FILE_NAME, proc_dir);   // file first
    remove_proc_entry(PROC_DIR_NAME, NULL);         // then dir
    pr_info("KernelWare: removed /proc/%s\n", PROC_DIR_NAME);
}