#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <asm-generic/errno-base.h>
#include <linux/device.h>
#include <linux/device/class.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include <linux/uaccess.h>
#include <linux/mod_devicetable.h>

#include "platform.h"

#define MAX_DEVICES 4

/* Device private data structure */
struct pcdev_priv_data
{
    struct platform_data pl_data;
    char *buffer;
    dev_t dev_num;
    struct cdev cdev;
};

/* Driver private data structure */
struct pcdrv_priv_data
{
    int total_devices;
    dev_t dev_num_base;
    struct class *class_pcd;
    struct device *device_pcd;
};

struct pcdrv_priv_data pcdrv_data;

int check_permission(int perm, struct file* filp)
{
    if ((perm == RD_ONLY) && ((filp->f_mode & FMODE_READ) && !(filp->f_mode & FMODE_WRITE))) {
        return 0;
    }

    if ((perm == WR_ONLY) && (!(filp->f_mode & FMODE_READ) && (filp->f_mode & FMODE_WRITE))) {
        return 0;
    }

    if ((perm == RD_WR) && ((filp->f_mode & FMODE_READ) || (filp->f_mode & FMODE_WRITE))) {
        return 0;
    }

    return -EPERM;
}

int pl_pcd_open(struct inode *inode, struct file *filp)
{
    int ret;
    int minor_n;

    struct pcdev_priv_data * priv_data;

    /*1. Find out on which device file open was attempted by the user space */
    minor_n = MINOR(inode->i_rdev);

    /*2. Get device's private data */
    priv_data = container_of(inode->i_cdev, struct pcdev_priv_data, cdev);

    /*3. Save device's private data */
    filp->private_data = priv_data;

    /*4. Check permission */
    ret = check_permission(priv_data->pl_data.perm, filp);

    if (!ret) {
        pr_info("%s: Open succeeds\n", __func__);
    } else {
        pr_err("%s: Open fails\n", __func__);
    }

    return ret;
}

int pl_pcd_release(struct inode* inode, struct file *filp)
{
    pr_info("%s: Release succeeds\n", __func__);

    return 0;
}

loff_t pl_pcd_lseek(struct file *filp, loff_t offset, int whence)
{
    struct pcdev_priv_data *priv_data = (struct pcdev_priv_data*) filp->private_data;
    loff_t f_pos;

    switch (whence) {
        case SEEK_SET:
            if (offset > priv_data->pl_data.size || offset < 0) {
                return -EINVAL;
            }
            filp->f_pos = offset;
            break;
        case SEEK_CUR:
            f_pos = filp->f_pos + offset;
            if (f_pos > priv_data->pl_data.size || f_pos < 0) {
                return -EINVAL;
            }
            filp->f_pos = f_pos;
            break;
        case SEEK_END:
            f_pos = priv_data->pl_data.size + offset;
            if (f_pos > priv_data->pl_data.size || f_pos < 0) {
                return -EINVAL;
            }
            filp->f_pos = f_pos;
            break;
        default:
            return -EINVAL;
    }

    return filp->f_pos;
}

ssize_t pl_pcd_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos)
{
    struct pcdev_priv_data *priv_data = (struct pcdev_priv_data*) filp->private_data;
    if (*f_pos + count > priv_data->pl_data.size) {
        count = priv_data->pl_data.size - *f_pos;
    }

    /* Copy data to user */
    if (copy_to_user(buff, &priv_data->buffer[*f_pos], count)) {
        return -EFAULT;
    }

    /* Update the current file's position */
    *f_pos += count;

    return count;
}

ssize_t pl_pcd_write(struct file *filp, const char __user *buff, size_t count, loff_t *f_pos)
{

    struct pcdev_priv_data *priv_data = (struct pcdev_priv_data*) filp->private_data;
    if (*f_pos + count > priv_data->pl_data.size) {
        count = priv_data->pl_data.size - *f_pos;
    }

    if (count == 0) {
        return -ENOMEM;
    }

    /* Write data taken from user */
    if (copy_from_user(&priv_data->buffer[*f_pos], buff, count)) {
        return -EFAULT;
    }

    /* Update the current file's position */
    *f_pos += count;

    return count;
}

struct file_operations fops = 
{
    .open = pl_pcd_open,
    .release = pl_pcd_release,
    .read = pl_pcd_read,
    .write = pl_pcd_write,
    .llseek = pl_pcd_lseek,
    .owner = THIS_MODULE
};

int pl_drv_probe(struct platform_device* dev)
{
    int ret;

    struct pcdev_priv_data *dev_data;

    struct platform_data *pl_data = (struct platform_data*)dev_get_platdata(&dev->dev);

    pr_info("%s: A device is detected\n", __func__);

    /*1. Get the platform data */
    if (!pl_data) {
        pr_err("%s: No platform data available\n", __func__);
        return -EINVAL;
    }

    /*2. Dynamically allocate memory for the device private data */
    dev_data = devm_kzalloc(&dev->dev, sizeof(*dev_data), GFP_KERNEL);
    if (!dev_data) {
        pr_err("%s: Can't allocate memory\n", __func__);
        return -ENOMEM;
    }

    dev_data->pl_data.size = pl_data->size;
    dev_data->pl_data.perm = pl_data->perm;
    dev_data->pl_data.serial_number = pl_data->serial_number;

    pr_info("%s: Serial No: %s\n", __func__, dev_data->pl_data.serial_number);
    pr_info("%s: Size: %d\n", __func__, dev_data->pl_data.size);
    pr_info("%s: Permission: %d\n", __func__, dev_data->pl_data.perm);

    /*3. Allocate memory for buffer */
    dev_data->buffer = devm_kzalloc(&dev->dev, dev_data->pl_data.size, GFP_KERNEL);
    if (!dev_data->buffer) {
        pr_err("%s: Cannot allocate memory\n", __func__);
        return -ENOMEM;
    }

    /*4. Get the device number */
    dev_data->dev_num = pcdrv_data.dev_num_base + dev->id;

    /*5. Do cdev init and cdev add */
    cdev_init(&dev_data->cdev, &fops);
    dev_data->cdev.owner = THIS_MODULE;
    ret = cdev_add(&dev_data->cdev, dev_data->dev_num, 1);
    if (ret < 0) {
        pr_err("%s: cdev_add failed\n", __func__);
        return ret;
    }

    dev_set_drvdata(&dev->dev, dev_data);

    /*6. Create device file for the detected platform device */
    pcdrv_data.device_pcd = device_create(pcdrv_data.class_pcd, NULL, dev_data->dev_num, NULL, "pcdev-%d", dev->id);
    if (IS_ERR(pcdrv_data.device_pcd)) {
        pr_err("Device create failed\n");
        ret = PTR_ERR(pcdrv_data.device_pcd);
        cdev_del(&dev_data->cdev);
        return ret;
    }

    pcdrv_data.total_devices++; 

    pr_info("%s: Probe platform device successfully\n", __func__);

    return 0;
}

int pl_drv_remove(struct platform_device* dev)
{
    struct pcdev_priv_data *priv_data = (struct pcdev_priv_data*) dev->dev.driver_data;
    /*1. Remove a device that was created with device_create() */
    device_destroy(pcdrv_data.class_pcd, priv_data->dev_num);

    /*2. Remove a cdev entry from the system */
    cdev_del(&priv_data->cdev);

    pcdrv_data.total_devices--;

    pr_info("%s: Remove platform device\n", __func__);
    
    return 0;
}

struct platform_device_id pcdev_ids[] = {
    [0] = {
        .name = "pcdev-A1X"
    },
    [1] = {
        .name = "pcdev-B1X"
    },
    [2] = {
        .name = "pcdev-C1X"
    },
    [3] = {
        .name = "pcdev-D1X"
    }
};

struct platform_driver pl_drv =
{
    .probe = pl_drv_probe,
    .remove = pl_drv_remove,
    .id_table = pcdev_ids,
    .driver = {
        .name = "pl-pcd-dev"
    }
};

static int __init pldev_drv_init(void)
{   
    int ret;
    /*1. Dynamically allocate a device number for MAX_DEVICES */
    ret = alloc_chrdev_region(&pcdrv_data.dev_num_base, 0, MAX_DEVICES, "pc-devs");
    if (ret < 0) {
        pr_err("Alloc chrdev failed\n");
        goto exit;
    }

    /*2. Create device class under /sys/class */
    pcdrv_data.class_pcd = class_create(THIS_MODULE, "pcd_class");
    if (IS_ERR(pcdrv_data.class_pcd)) {
        pr_err("Class creation failed\n");
        ret = PTR_ERR(pcdrv_data.class_pcd);
        goto unregister_device;
    }

    /* Register a platform driver */
    platform_driver_register(&pl_drv);

    pr_info("%s: Initialize platform driver successfully\n", __func__);

unregister_device:
    unregister_chrdev_region(pcdrv_data.dev_num_base, MAX_DEVICES);
exit:
    return ret;
}

static void __exit pldev_drv_exit(void)
{
    /*1. Unregister platform driver */    
    platform_driver_unregister(&pl_drv);

    /*2. Class detroy */
    class_destroy(pcdrv_data.class_pcd);

    /*3. Unregister device numbers for MAX_DEVICES */
    unregister_chrdev_region(pcdrv_data.dev_num_base, MAX_DEVICES);

    pr_info("%s: Remove platform driver successfully\n", __func__);
}

module_init(pldev_drv_init);
module_exit(pldev_drv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phuong Dao");
MODULE_DESCRIPTION("Pseudo platform device driver");
MODULE_INFO(board, "Virtual Machine x86");