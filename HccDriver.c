
#include <linux/init.h>
#include <linux/kernel.h>       /* Needed for KERN_INFO */
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/export.h>
#include <linux/uaccess.h>


// static ssize_t device_file_read(
//                         struct file *file_ptr
//                        , char __user *user_buffer
//                        , size_t count
//                        , loff_t *position)
// {
//     return 0;
// }

// static struct file_operations simple_driver_fops = 
// {
//     .owner   = THIS_MODULE,
//     .read    = device_file_read,
// };

// static int device_file_major_number = 0;
// static const char device_name[] = "hcc";

// int register_hcc_device(void)
// {
//     int result = 0;
//     printk( KERN_NOTICE "hcc-driver: register_device() is called.\n" );
//     result = register_chrdev( 0, device_name, &simple_driver_fops );
//     if( result < 0 )
//     {
//             printk( KERN_WARNING "hcc-driver:  can\'t register character device with error code = %i\n", result );
//             return result;
//     }
//     device_file_major_number = result;
//     printk( KERN_NOTICE "hcc-driver: registered character device with major number = %i and minor numbers 0...255\n", device_file_major_number );
//     return 0;
// }


// void unregister_hcc_device(void)
// {
//     printk( KERN_NOTICE "hcc-driver: unregister_device() is called\n" );
//     if(device_file_major_number != 0)
//     {
//         unregister_chrdev(device_file_major_number, device_name);
//     }
// }

static int my_init(void)
{
    printk(KERN_INFO "Loading hcc module...\n");
    //register_hcc_device();
    return  0;
}

static void my_exit(void)
{
    //unregister_hcc_device();
    return;
}

module_init(my_init);
module_exit(my_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Krzysztof Richert");
