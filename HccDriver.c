
#include <linux/module.h>
#include <linux/platform_device.h>


#define DATA_SIZE 4
#define HCC_NAME "hcc_generic_driver"
#define SETTINGS "Settings from HCC device\n"

static int register_hcc_device(void);
static void unregister_hcc_device(void);
static int device_probe(struct platform_device *pdev);
static int device_remove(struct platform_device *pdev);
static ssize_t device_file_read(
                        struct file *file_ptr
                       , char __user *user_buffer
                       , size_t count
                       , loff_t *position);

static ssize_t settings_show(struct device *dev,
                             struct device_attribute *attr,
                             char *buf);
static DEVICE_ATTR_RO(settings);


static ssize_t trigger_store(struct device *dev,
                             struct device_attribute *attr,
                             const char *buf, size_t count);
static DEVICE_ATTR_WO(trigger);                             

static struct platform_device *pdev = NULL;

static struct file_operations driver_fops = 
{
    .owner   = THIS_MODULE,
    .read    = device_file_read,
};

struct device_data {
    int device_id;
    char name[32];
    char dev_addr[DATA_SIZE];
};

static struct attribute *hcc_attrs[] = 
{
    &dev_attr_settings.attr,
    &dev_attr_trigger.attr,
};

static const struct attribute_group hcc_group = 
{
    .attrs = hcc_attrs,
    .bin_attrs = NULL,
};

static const struct attribute_group *hcc_groups[] = {
    &hcc_group,
    NULL,
};

static struct platform_driver hcc_driver = {
    .probe = device_probe,
    .remove = device_remove,
    .driver = {
        .name = HCC_NAME,
        .dev_groups = hcc_groups,
    },
};

static int device_file_major_number = 0;
static const char device_name[] = "hcc";
static struct class *hcc_class = NULL;


static ssize_t settings_show(struct device *dev,
                             struct device_attribute *attr,
                             char *buf)
{
    struct device_data *data = dev_get_drvdata(dev);
    return snprintf(buf, PAGE_SIZE, "Device ID: %d, Name: %s\n", data->device_id, data->name);
}

static ssize_t trigger_store(struct device *dev,
                             struct device_attribute *attr,
                             const char *buf, size_t count)
{

    return count;
}

static ssize_t device_file_read(
                        struct file *file_ptr
                       , char __user *user_buffer
                       , size_t count
                       , loff_t *position)
{
    return simple_read_from_buffer(user_buffer, count, position, SETTINGS, strlen(SETTINGS));
}




static int register_hcc_device(void)
{
    int result = 0;
    printk( KERN_NOTICE "hcc-driver: register_device() is called.\n" );
    result = register_chrdev( 0, device_name, &driver_fops );
    if( result < 0 )
    {
            printk( KERN_WARNING "hcc-driver:  can\'t register character device with error code = %i\n", result );
            return result;
    }
    device_file_major_number = result;

    // create file (no need to perform mknod)
    hcc_class = class_create("HCC_CLASS");
    if (IS_ERR(hcc_class)) {
        return PTR_ERR(hcc_class);
    }

    device_create(hcc_class, NULL, MKDEV(device_file_major_number, 0), NULL, device_name);

    return 0;
}


static void unregister_hcc_device(void)
{
    printk( KERN_NOTICE "hcc-driver: unregister_device() is called\n" );
    if(device_file_major_number != 0)
    {
        device_destroy(hcc_class, MKDEV(device_file_major_number, 0));
        class_destroy(hcc_class);
        unregister_chrdev(device_file_major_number, device_name);
    }
}

static int device_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct device_data *data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);

    if (!data) 
    {
        dev_err(dev, "Failed to allocate memory for device data\n");
        return -ENOMEM;
    }
    data->device_id = pdev->id;
    snprintf(data->name, sizeof(data->name), "hcc_device_%d", pdev->id);
    dev_set_drvdata(dev, data);
    dev_info(dev, "HCC driver probe called for device %s\n", dev_name(dev));

    return 0;
}

static int device_remove(struct platform_device *pdev)
{
    return 0;
}

static int my_init(void)
{
    int err = -1;

    pdev = platform_device_alloc(HCC_NAME, 0);
    if (!pdev)
    {
        printk(KERN_INFO "Cannot allocate device...\n");
    }

    err = platform_device_add(pdev);
    if (err)
    {
        printk(KERN_INFO "Cannot add device...\n");
        goto exit;
    }
    
    platform_device_put(pdev);

    printk(KERN_INFO "Loading hcc module...\n");
    register_hcc_device();
    platform_driver_register(&hcc_driver);
exit:
    return  err;
}

static void my_exit(void)
{
    platform_driver_unregister(&hcc_driver);
    platform_device_del(pdev);

    unregister_hcc_device();
    return;
}

module_init(my_init);
module_exit(my_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Krzysztof Richert");
