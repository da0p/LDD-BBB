#ifndef PCD_SYSCALLS_H
#define PCD_SYSCALLS_H

#include <linux/fs.h>
#include <linux/device.h>

int check_permission(int perm, struct file* filp);

int pl_pcd_open(struct inode *inode, struct file *filp);

int pl_pcd_release(struct inode* inode, struct file *filp);

loff_t pl_pcd_lseek(struct file *filp, loff_t offset, int whence);

ssize_t pl_pcd_read(struct file *filp, char __user *buff, size_t count, loff_t *f_pos);

ssize_t pl_pcd_write(struct file *filp, const char __user *buff, size_t count, loff_t *f_pos);

#endif