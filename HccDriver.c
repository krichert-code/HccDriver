
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/sock.h>


#define NETLINK_HCC 31
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

struct user_addr {
    u32 user_pid;
    struct list_head list;  /* list of all fox structures */
};

static int device_file_major_number = 0;
static const char device_name[] = "hcc";
static struct class *hcc_class = NULL;

static struct sock *nl_sk = NULL;
static struct user_addr *user_pids_list = NULL;

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
    struct device_data *data = dev_get_drvdata(dev);
    struct sk_buff *skb_out;
    struct nlmsghdr *nlh_out;
    struct user_addr *element;
    int res_send;
    
    memcpy(data->dev_addr, buf, min(count, sizeof(data->dev_addr) - 1));
    printk(KERN_INFO "Trigger store called for device %d (%s) address: %s\n", data->device_id, data->name, data->dev_addr);
    if (user_pids_list != NULL) 
    {
        list_for_each_entry(element, &user_pids_list->list, list) {
            printk(KERN_INFO "Notifying user with PID: %d\n", element->user_pid);
            skb_out = nlmsg_new(count, GFP_KERNEL);
            if (unlikely(!skb_out)) 
            {
                pr_info("Failed to allocate new skb\n");
                return -ENOMEM;
            }
            nlh_out = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, count, 0);
            NETLINK_CB(skb_out).dst_group = 0;
            strncpy(nlmsg_data(nlh_out), buf, count);
            res_send = nlmsg_unicast(nl_sk, skb_out, element->user_pid);
            if (unlikely(res_send < 0)) 
            {
                dev_err(dev, "Error while sending back to user\n");
            } 
            else 
            {
                dev_info(dev, "Sent back to user %d\n", res_send);
            }
        }
    }
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




static void nl_release_handler(struct sock *sk, unsigned long *groups)
{
    // if (sk->sk_protocol == NETLINK_HCC) 
    // {
    // }
    // pr_info("Netlink socket released port (user data) = %d\n", *(u32 *)sk->sk_user_data);
}

static void nl_recv_msg(struct sk_buff *skb)
{
    struct nlmsghdr *nlh;
    int pid;
    char buf[8] = "ABCDEF";
    nlh = (struct nlmsghdr *)skb->data;
    pid = nlh->nlmsg_pid;  // PID proces user-space
    pr_info("Netlink received msg: %s\n", (char *)nlmsg_data(nlh));

    // new user registration
    if (user_pids_list == NULL) 
    {
        user_pids_list = kmalloc(sizeof(*user_pids_list), GFP_KERNEL);
        if (!user_pids_list) 
        {
            pr_err("Failed to allocate memory for user pids list\n");
            return;
        }
    
        INIT_LIST_HEAD(&user_pids_list->list);
        struct user_addr *new_user_pid = kmalloc(sizeof(*new_user_pid), GFP_KERNEL);
        if (!new_user_pid) 
        {
            pr_err("Failed to allocate memory for new user pid\n");
            return;
        }
    
        new_user_pid->user_pid = pid;
        printk(KERN_INFO "New user registered with PID: %d\n", pid);
        list_add(&new_user_pid->list, &user_pids_list->list);
    }

    // Odpowiedź do userspace
    struct sk_buff *skb_out;
    struct nlmsghdr *nlh_out;
    int msg_size = strlen(buf);
    int res_send;
    u32 *ctx = kmalloc(sizeof(*ctx), GFP_KERNEL);
    *ctx = pid;
    skb->sk->sk_user_data = ctx;
    skb_out = nlmsg_new(msg_size, 0);
    if (!skb_out) 
    {
        pr_info("Failed to allocate new skb\n");
        return;
    }
    nlh_out = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
    NETLINK_CB(skb_out).dst_group = 0;
    strncpy(nlmsg_data(nlh_out), buf, msg_size);
    res_send = nlmsg_unicast(nl_sk, skb_out, pid);
    if (res_send < 0)
        pr_info("Error while sending back to user\n");
    else
        pr_info("Sent back to user %d\n", res_send);
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
    struct netlink_kernel_cfg cfg = {
        .input = nl_recv_msg,
        .release = nl_release_handler,
    };

    if (!data) 
    {
        dev_err(dev, "Failed to allocate memory for device data\n");
        return -ENOMEM;
    }
    data->device_id = pdev->id;
    snprintf(data->name, sizeof(data->name), "hcc_device_%d", pdev->id);
    dev_set_drvdata(dev, data);
    dev_info(dev, "HCC driver probe called for device %s\n", dev_name(dev));

    nl_sk = netlink_kernel_create(&init_net, NETLINK_HCC, &cfg);
    if (!nl_sk) 
    {
        dev_info( dev, "hcc-driver:  Error creating netlink socket\n" );
        return -ENOMEM;
    }

    return 0;
}

static int device_remove(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    dev_set_drvdata(dev, NULL);
    devm_kfree(dev, dev_get_drvdata(dev));
    if (nl_sk) 
    {
        netlink_kernel_release(nl_sk);
        nl_sk = NULL;
    }
    printk(KERN_INFO "hcc-driver removed for device %s\n", dev_name(dev));

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
