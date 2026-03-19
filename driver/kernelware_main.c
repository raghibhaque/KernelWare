#include "kernelware.h"

#include "../shared/kw_ioctl.h"
#include "kw_driver.h"
#include "kw_games.h"

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/sched.h>

static dev_t dev_num;
static struct cdev my_cdev;
static struct class *my_class;

DECLARE_WAIT_QUEUE_HEAD(my_wq);  // wait queue
int data_ready = 0;              // condition flag
char kernel_buf[256];
int buf_len = 0;

atomic_t open_count = ATOMIC_INIT(0);
struct kw_state  current_state  = {0};
struct kw_config current_config = {0};

// Define shared struct
struct my_driver_state drv_state = {0};


static ssize_t kw_read(struct file *file, char __user *buf, size_t len, loff_t *off)
{
    drv_state.read_count++;
    if (wait_event_interruptible(my_wq, data_ready != 0))
        return -ERESTARTSYS;

    int bytes = min(len, (size_t)buf_len);
    if (copy_to_user(buf, kernel_buf, bytes))
        return -EFAULT;

    data_ready = 0;
    *off = 0;    // ← reset offset so next read() blocks again instead of returning EOF
    return bytes;
}


static ssize_t kw_write(struct file *file, const char __user *buf, size_t len, loff_t *off)
{
    drv_state.write_count++;
    int bytes = min(len, sizeof(kernel_buf) - 1);

    if (copy_from_user(kernel_buf, buf, bytes))
        return -EFAULT;

    buf_len = bytes;
    kernel_buf[bytes] = '\0';


    kw_game_handle_input(kernel_buf[0]); // game by game input handling


    data_ready = 1;
    wake_up_interruptible(&my_wq);

    return bytes;
}



static long kw_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    switch (cmd) {

    case KW_IOCTL_START:
        current_state.game_id = 1;
        kw_game_start(current_state.game_id);
        return 0;

    case KW_IOCTL_GET_STATE:
        // copy state struct to userspace
        if (copy_to_user((struct kw_state __user *)arg,
                         &current_state,
                         sizeof(struct kw_state)))
            return -EFAULT;
        return 0;

    case KW_IOCTL_SET_CONFIG:
        // copy config struct from userspace
        if (copy_from_user(&current_config,
                           (struct kw_config __user *)arg,
                           sizeof(struct kw_config)))
            return -EFAULT;

        current_state.lives      = current_config.lives;
        current_state.difficulty = current_config.difficulty;
        current_state.score      = 0;
        return 0;

    default:
        return -ENOTTY;  // standard "not a valid ioctl" error
    }
}


static int kw_open(struct inode *inode, struct file *filp)
{
    drv_state.open_count++;
    if (atomic_inc_return(&open_count) > 1) {
        atomic_dec(&open_count);
        return -EBUSY;
    }

    // clear leftover state from previous session 
    memset(&current_state, 0, sizeof(current_state));
    data_ready = 0;
    buf_len    = 0;
    memset(kernel_buf, 0, sizeof(kernel_buf));

    pr_info("kernelware: opened\n");
    return 0;
}

static int kw_release(struct inode *inode, struct file *filp)
{
    drv_state.open_count--;
    kw_game_stop();
    atomic_dec(&open_count);
    pr_info("kernelware: closed\n");
    return 0;
}


static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = kw_open,
    .release = kw_release,
    .read = kw_read,
    .write = kw_write,
    .unlocked_ioctl = kw_ioctl
};


//sets permissions
static char *kernelware_devnode(const struct device *dev, umode_t *mode)
{
    if (mode)
        *mode = 0666;
    return NULL;
}


static int __init my_module_init(void)
{
    if (alloc_chrdev_region(&dev_num, 0, 1, "kernelware") < 0) {
        pr_err("KernelWare: failed to alloc dev region\n");
        return -1;
    }

    cdev_init(&my_cdev, &fops);
    if (cdev_add(&my_cdev, dev_num, 1) < 0) {
        pr_err("KernelWare: failed to add cdev\n");
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    my_class = class_create("kernelware");
    if (IS_ERR(my_class)) {
        pr_err("KernelWare: failed to create class\n");
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }
    my_class->devnode = kernelware_devnode; // fixes permissions 
    
    device_create(my_class, NULL, dev_num, NULL, "kernelware");

    if (my_proc_init() != 0) {
        device_destroy(my_class, dev_num);
        class_destroy(my_class);
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    pr_info("KernelWare: loaded\n");
    return 0;
}

static void __exit my_module_exit(void) {
    kw_game_stop();              // ← add this first
    my_proc_exit();
    device_destroy(my_class, dev_num);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(dev_num, 1);
    printk(KERN_INFO "KernelWare: unloaded\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("KernelWare: HardwareIntegrated Micro-Game Engine");
MODULE_AUTHOR("Aidan, Cathal, Raghib, Tom");