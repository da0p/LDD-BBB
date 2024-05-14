#include "pcd_syscalls.h"
#include "platform.h"

#define RD_ONLY 0x01
#define WR_ONLY 0x10
#define RD_WR 0x11

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

