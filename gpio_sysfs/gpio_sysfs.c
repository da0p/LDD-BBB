#include "asm-generic/errno-base.h"
#include "linux/err.h"
#include "linux/of.h"
#include "linux/sysfs.h"
#include <linux/device/class.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/device.h>
#include <linux/mutex.h>

#define LABEL_SIZE 20

struct gpiodev_priv_data
{
    char label[LABEL_SIZE];
    struct gpio_desc* desc;
    struct mutex plock;
};

struct gpiodrv_priv_data
{
    int total_devices;
    struct class *gpio_class;
    struct device **dev;
};

struct gpiodrv_priv_data gpio_drv_data;

ssize_t direction_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int ret;
    /* Here, we don't need to extract dev_data from dev->parent because of device_create_with_groups */
    struct gpiodev_priv_data *dev_data = dev_get_drvdata(dev);
    int dir;
    char *dir_label;

    mutex_lock(&dev_data->plock);

    dir = gpiod_get_direction(dev_data->desc);
    if (dir < 0) {
        return dir;
    }
    /* if dir = 0, then show "out", if dir=1, then show "in" */
    dir_label = (dir == 0) ? "out" : "in";

    ret = sprintf(buf, "%s\n", dir_label);

    mutex_unlock(&dev_data->plock);

    return ret;
}

ssize_t direction_store(struct device *dev, struct device_attribute *attr, const char* buf, size_t count)
{
    struct gpiodev_priv_data *dev_data = dev_get_drvdata(dev);
    int ret;

    mutex_lock(&dev_data->plock);

    if (sysfs_streq(buf, "in")) {
        ret = gpiod_direction_input(dev_data->desc);
    } else if (sysfs_streq(buf, "out")) {
        ret = gpiod_direction_output(dev_data->desc, 0);
    } else {
        ret = -EINVAL;
    }

    ret = ret ? : count;

    mutex_unlock(&dev_data->plock);

    return ret;
}

ssize_t value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int ret;
    struct gpiodev_priv_data *dev_data = dev_get_drvdata(dev);
    int value;

    mutex_lock(&dev_data->plock);

    value = gpiod_get_value(dev_data->desc);

    ret = sprintf(buf, "%d\n", value);

    mutex_unlock(&dev_data->plock);

    return ret;
}

ssize_t value_store(struct device *dev, struct device_attribute *attr, const char* buf, size_t count)
{
    struct gpiodev_priv_data *dev_data = dev_get_drvdata(dev);
    int ret;
    long value;

    mutex_lock(&dev_data->plock);

    ret = kstrtol(buf, 0, &value);
    if (ret) {
        return ret;
    }

    gpiod_set_value(dev_data->desc, value);

    mutex_unlock(&dev_data->plock);

    return count;
}

ssize_t label_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    int ret;
    struct gpiodev_priv_data *dev_data = dev_get_drvdata(dev);

    mutex_lock(&dev_data->plock);

    ret = sprintf(buf, "%s\n", dev_data->label);

    mutex_unlock(&dev_data->plock);

    return ret;
}

static DEVICE_ATTR_RW(direction);
static DEVICE_ATTR_RW(value);
static DEVICE_ATTR_RO(label);

struct attribute *attr[] = {
    &dev_attr_direction.attr,
    &dev_attr_value.attr,
    &dev_attr_label.attr,
    NULL
};

struct attribute_group attr_grp = {
    .attrs = attr
};

static const struct attribute_group *gpio_attr_grps[] =
{
    &attr_grp,
    NULL
};

int gpio_sysfs_probe(struct platform_device *pdev)
{
    struct device_node *parent = pdev->dev.of_node;
    struct device_node *child = NULL;
    const char *label;
    int ret;
    int i = 0;
    struct gpiodev_priv_data *dev_data;

    gpio_drv_data.total_devices = of_get_child_count(parent);
    if (!gpio_drv_data.total_devices) {
        dev_info(&pdev->dev, "No devices found\n");
        return -EINVAL;
    }
    dev_info(&pdev->dev, "Child count = %d\n", gpio_drv_data.total_devices);

    gpio_drv_data.dev = devm_kzalloc(&pdev->dev, sizeof(struct device*) * gpio_drv_data.total_devices, GFP_KERNEL);

    for_each_available_child_of_node(parent, child) {
        dev_data = devm_kzalloc(&pdev->dev, sizeof(*dev_data), GFP_KERNEL);
        if (!dev_data) {
            dev_err(&pdev->dev, "Cannot allocate memory\n");
            return -ENOMEM;
        }

        mutex_init(&dev_data->plock);

        if (of_property_read_string(child, "label", &label)) {
            dev_warn(&pdev->dev, "Missing label information\n");
            snprintf(dev_data->label, sizeof(dev_data->label), "unknown gpio%d\n", i);
        } else {
            strcpy(dev_data->label, label);
            dev_info(&pdev->dev, "GPIO label = %s\n", dev_data->label);
        }

        dev_data->desc = devm_fwnode_get_gpiod_from_child(&pdev->dev, "bone", &child->fwnode, GPIOD_ASIS, dev_data->label);
        if (IS_ERR(dev_data->desc)) {
            ret = PTR_ERR(dev_data->desc);
            if (ret == -ENOENT) {
                dev_err(&pdev->dev, "Cannot get gpio descriptor\n");
            }
            return ret;
        }

        ret = gpiod_direction_output(dev_data->desc, 0);
        if (ret) {
            return ret;
        }

        gpio_drv_data.dev[i] = device_create_with_groups(gpio_drv_data.gpio_class, &pdev->dev, 0, dev_data, gpio_attr_grps, dev_data->label);
        if (IS_ERR(gpio_drv_data.dev[i])) {
            ret = PTR_ERR(gpio_drv_data.dev[i]);
            dev_err(&pdev->dev, "Cannot create device with groups\n");
            return ret;
        }

        i++;
    }

    return 0;
}

int gpio_sysfs_remove(struct platform_device *pdev)
{
    int i;

    dev_info(&pdev->dev, "Remove called\n");

    for (i = 0; i < gpio_drv_data.total_devices; i++) {
        device_unregister(gpio_drv_data.dev[i]);
    }
    return 0;
}

struct of_device_id gpio_device_match[] = {
   {
        .compatible = "org,bone-gpio-sysfs"
   },
   {}
};

struct platform_driver gpiosysfs_platform_drv = {
    .probe = gpio_sysfs_probe,
    .remove = gpio_sysfs_remove,
    .driver = {
        .name = "bone-gpio-sysfs",
        .of_match_table = of_match_ptr(gpio_device_match)
    }
};

int __init gpio_sysfs_init(void)
{
    gpio_drv_data.gpio_class = class_create(THIS_MODULE, "bone_gpios");
    if (IS_ERR(gpio_drv_data.gpio_class)) {
        pr_err("Error in creating class\n");
        return PTR_ERR(gpio_drv_data.gpio_class);
    };

    platform_driver_register(&gpiosysfs_platform_drv);
    pr_info("gpio_sysfs module load success\n");

    return 0;
}

void __exit gpio_sysfs_exit(void)
{
    platform_driver_register(&gpiosysfs_platform_drv);

    class_destroy(gpio_drv_data.gpio_class);

    pr_info("gpio_sysfs_exit\n");
}

module_init(gpio_sysfs_init);
module_exit(gpio_sysfs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phuong Dao");
MODULE_DESCRIPTION("GPIO sysfs experiment");