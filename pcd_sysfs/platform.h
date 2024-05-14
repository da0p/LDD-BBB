#ifndef PCD_PLATFORM_H
#define PCD_PLATFORM_H

#include <linux/mod_devicetable.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>

#define MAX_DEVICES 4

enum PCD_DEV_DATA {
    PCDEVA1X,
    PCDEVB1X,
    PCDEVC1X,
    PCDEVD1X,
};

struct platform_data
{
    int size;
    int perm;
    const char *serial_number;
};

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

#endif