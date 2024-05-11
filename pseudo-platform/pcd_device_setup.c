#include <linux/platform_device.h>
#include <linux/module.h>

#include "platform.h"

#define NO_OF_DEVICES 4


/*1. Create 2 platform data*/
struct platform_data pcdev_pdata[NO_OF_DEVICES] = {
    [0] = {.size = 512, .perm = RD_WR, .serial_number = "PCDEV1111"},
    [1] = {.size = 1024, .perm = RD_WR, .serial_number = "PCDEV2222"},
    [2] = {.size = 128, .perm = RD_ONLY, .serial_number = "PCDEV3333"},
    [3] = {.size = 32, .perm = WR_ONLY, .serial_number = "PCDEV4444"}
};

/* Release acts as a callback when platform device is not longer used
 * Especially in the case that we use dynamic memory allocation for
 * platform devices.
 */
void pcdev_release(struct device* dev)
{
    pr_info("pcdev_release\n");
}

/*2. Create 2 platform devices */
struct platform_device pl_pcdev_1 = {
    .name = "pcdev-A1X",
    .id = 0,
    .dev = {
        .platform_data = &pcdev_pdata[0],
        .release = pcdev_release
    }
};

struct platform_device pl_pcdev_2 = {
    .name = "pcdev-B1X",
    .id = 1,
    .dev = {
        .platform_data = &pcdev_pdata[1],
        .release = pcdev_release
    }
};

struct platform_device pl_pcdev_3 = {
    .name = "pcdev-C1X",
    .id = 2,
    .dev = {
        .platform_data = &pcdev_pdata[2],
        .release = pcdev_release
    }
};

struct platform_device pl_pcdev_4 = {
    .name = "pcdev-D1X",
    .id = 3,
    .dev = {
        .platform_data = &pcdev_pdata[3],
        .release = pcdev_release
    }
};

struct platform_device *platform_devices[NO_OF_DEVICES] = 
{
    &pl_pcdev_1,
    &pl_pcdev_2,
    &pl_pcdev_3,
    &pl_pcdev_4
};

static int __init pcdev_platform_init(void)
{
    pr_info("%s: Platform device initialization\n", __func__);

    platform_add_devices(platform_devices, NO_OF_DEVICES);

    return 0;
}

static void __exit pcdev_platform_exit(void)
{
    platform_device_unregister(&pl_pcdev_1);
    platform_device_unregister(&pl_pcdev_2);
    platform_device_unregister(&pl_pcdev_3);
    platform_device_unregister(&pl_pcdev_4);

    pr_info("%s: Platform device removed\n", __func__);
}

module_init(pcdev_platform_init);
module_exit(pcdev_platform_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phuong Dao");
MODULE_DESCRIPTION("Module to register platform devices");