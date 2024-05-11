#include <asm-generic/errno-base.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>

#define NO_OF_DEVS 4

/* Device's memory*/
#define MEM_SIZE_DEV1 1024
#define MEM_SIZE_DEV2 512
#define MEM_SIZE_DEV3 1024
#define MEM_SIZE_DEV4 512

/* Permission */
#define R_ONLY_PERM  0x01
#define W_ONLY_PERM  0x10
#define RW_PERM      0x11

/* pseudo-device's memory*/
char dev1_buffer[MEM_SIZE_DEV1];
char dev2_buffer[MEM_SIZE_DEV2];
char dev3_buffer[MEM_SIZE_DEV3];
char dev4_buffer[MEM_SIZE_DEV4];

/* Device private data structure */
struct pcd_private_data
{
    char *buffer;
    unsigned size;
    const char *serial_number;
    int perm;
    struct cdev cdev;
};

/* Driver private data structure */
struct pcd_drv_private_data
{
    int total_devices;
    struct pcd_private_data pcd_data[NO_OF_DEVS];
    /* This holds device number */
    dev_t dev;
    struct class *pcd_class;
    struct device *pcd_device;
};

struct pcd_drv_private_data pcd_priv_data = 
{
    .total_devices = NO_OF_DEVS,
    .pcd_data = {
        [0] = {
            .buffer = dev1_buffer,
            .size = MEM_SIZE_DEV1,
            .serial_number = "PCD0001",
            .perm = R_ONLY_PERM,
        },
        [1] = {
            .buffer = dev2_buffer,
            .size = MEM_SIZE_DEV2,
            .serial_number = "PCD0002",
            .perm = W_ONLY_PERM,
        },
        [2] = {
            .buffer = dev3_buffer,
            .size = MEM_SIZE_DEV3,
            .serial_number = "PCD0003",
            .perm = RW_PERM,
        },
        [3] = {
            .buffer = dev4_buffer,
            .size = MEM_SIZE_DEV4,
            .serial_number = "PCD0004",
            .perm = RW_PERM,
        },
    }
};

int check_permission(int perm, struct file *filp)
{
    if ((perm == R_ONLY_PERM) && ((filp->f_mode & FMODE_READ) && !(filp->f_mode & FMODE_WRITE))) {
        return 0;
    }

    if ((perm == W_ONLY_PERM) && ((filp->f_mode & FMODE_WRITE) && !(filp->f_mode & FMODE_READ))) {
        return 0;
    }

    if ((perm == RW_PERM) && ((filp->f_mode & FMODE_WRITE) || (filp->f_mode & FMODE_READ))) {
        return 0;
    }

    return -EPERM;
}

int pcd_open(struct inode *inode, struct file *filp)
{
    int ret;
    int minor_n;

    struct pcd_private_data *p_priv_data;

    /*1. Find out on which device file open was attempted by the user space */
    minor_n = MINOR(inode->i_rdev);

    /*2. Get device's private data*/
    p_priv_data = container_of(inode->i_cdev, struct pcd_private_data, cdev);

    /*3. Save it for other methods */
    filp->private_data = p_priv_data;

    /*4. Check permission */
    ret = check_permission(p_priv_data->perm, filp);

    if (!ret) {
        pr_info("Open was successful");
    } else {
        pr_info("Open was not successful");
    }

    return ret;
}

int pcd_release(struct inode *inode, struct file *filp)
{
    pr_info("Release was successful\n");

    return 0;
}

loff_t pcd_lseek(struct file *filp, loff_t offset, int whence)
{
    struct pcd_private_data *p_priv_data = (struct pcd_private_data*) filp->private_data;
    loff_t f_pos;

    pr_info("lseek requested\n");
    pr_info("Current value of the file position = %lld\n", filp->f_pos);

    switch (whence) {
        case SEEK_SET:
            if (offset > p_priv_data->size || offset < 0) {
                return -EINVAL;
            }
            filp->f_pos = offset;
            break;
        case SEEK_CUR:
            f_pos = filp->f_pos + offset;
            if (f_pos > p_priv_data->size || f_pos < 0) {
                return -EINVAL;
            }
            filp->f_pos = f_pos;
            break;
        case SEEK_END:
            f_pos = p_priv_data->size + offset;
            if (f_pos > p_priv_data->size || f_pos < 0) {
                return -EINVAL;
            }
            filp->f_pos = f_pos;
            break;
        default:
            return -EINVAL;
    }

    return filp->f_pos;
}

ssize_t pcd_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos)
{
    struct pcd_private_data * p_priv_data = (struct pcd_private_data*)filp->private_data;

    pr_info("Read requested for %zu bytes\n", count);
    pr_info("Current file position = %lld\n", *f_pos);

    /* Adjust count */
    if (*f_pos + count > p_priv_data->size) {
        count = p_priv_data->size - *f_pos;
    }

    /* Copy to user */
    if (copy_to_user(buff, &p_priv_data->buffer[*f_pos], count)) {
        return -EFAULT;
    }

    /* Update the current file position */
    *f_pos += count;

    pr_info("Number of bytes successfully read = %zu\n", count);
    pr_info("Updated file position = %lld\n", *f_pos);

    /* Return number of bytes which have been successfully read */
    return count;
}

ssize_t pcd_write(struct file *filep, const char __user *buff, size_t count, loff_t *f_pos)
{
    struct pcd_private_data* p_priv_data = (struct pcd_private_data*) filep->private_data;

    pr_info("Write requested for %zu\n", count);

    if (*f_pos + count > p_priv_data->size) {
        count = p_priv_data->size - *f_pos;
    }

    if (count == 0) {
        return -ENOMEM;
    }

    if (copy_from_user(&p_priv_data->buffer[*f_pos], buff, count)) {
        return -EFAULT;
    }

    pr_info("Number of bytes successfully written = %zu\n", count);

    return count;
}

struct file_operations pcd_fops =
    {
        .open = pcd_open,
        .release = pcd_release,
        .read = pcd_read,
        .write = pcd_write,
        .owner = THIS_MODULE};

static int __init pcd_init(void)
{
    int ret;
    int i;

    pr_info("Loading pcd\n");

    /*1. Dynamically allocate a device number */
    ret = alloc_chrdev_region(&pcd_priv_data.dev, 0, NO_OF_DEVS, "pcd_devices");
    if (ret < 0)
    {
        pr_err("Allocate chrdev failed\n");
        goto out;
    }

    /*4. Create device class under /sys/class */
    pcd_priv_data.pcd_class = class_create(THIS_MODULE, "pcd_class");
    if (IS_ERR(pcd_priv_data.pcd_class))
    {
        pr_err("Class creation failed\n");
        ret = PTR_ERR(pcd_priv_data.pcd_class);
        goto delete_pseudo_cdev;
    }

    for (i = 0; i < NO_OF_DEVS; i++) {
        pr_info("Device number %d <major>:<minor> = %d:%d\n",
            i + 1,
            MAJOR(pcd_priv_data.dev + i), 
            MINOR(pcd_priv_data.dev + i));

        /*2. Initialize the cdev structure with fops */
        cdev_init(&pcd_priv_data.pcd_data[i].cdev, &pcd_fops);
        /*3. Register a device (cdev structure) with VFS */
        pcd_priv_data.pcd_data[i].cdev.owner = THIS_MODULE;
        ret = cdev_add(&pcd_priv_data.pcd_data[i].cdev, pcd_priv_data.dev + i, 1);
        if (ret < 0)
        {
            pr_err("cdev add failed\n");
            goto unregister_chardev;
        }
        /*5. Populate the sysfs with device information */
        pcd_priv_data.pcd_device = device_create(pcd_priv_data.pcd_class, NULL, pcd_priv_data.dev + i, NULL, "pcd-%d", i + 1);
        if (IS_ERR(pcd_priv_data.pcd_device))
        {
            pr_err("Device create failed\n");
            ret = PTR_ERR(pcd_priv_data.pcd_device);
            goto delete_pseudo_class;
        }
    }

    pr_info("Module init was successful\n");

    return 0;

delete_pseudo_class:
delete_pseudo_cdev:
    for (; i >= 0; i--) {
        device_destroy(pcd_priv_data.pcd_class, pcd_priv_data.dev + i);
        cdev_del(&pcd_priv_data.pcd_data[i].cdev);
    }
    class_destroy(pcd_priv_data.pcd_class);
unregister_chardev:
    unregister_chrdev_region(pcd_priv_data.dev, NO_OF_DEVS);
out:
    pr_info("Module insertion failed\n");
    return ret;
}

static void __exit pcd_cleanup(void)
{
    int i;
    pr_info("Unloading pcd\n");
    for (i = 0; i < NO_OF_DEVS; i++) {
        device_destroy(pcd_priv_data.pcd_class, pcd_priv_data.dev + i);
        cdev_del(&pcd_priv_data.pcd_data[i].cdev);
    }
    class_destroy(pcd_priv_data.pcd_class);
    unregister_chrdev_region(pcd_priv_data.dev, NO_OF_DEVS);
    pr_info("Module unloaded\n");
}

module_init(pcd_init);
module_exit(pcd_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phuong Dao");
MODULE_DESCRIPTION("Pseudo character device driver");
MODULE_INFO(board, "Virtual Machine x86");