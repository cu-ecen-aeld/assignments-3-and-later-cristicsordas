/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>		/* kmalloc() */
#include <linux/string.h>
#include "aesdchar.h"

int aesd_major =   AESD_MAJOR; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Cristian Csordas"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
	struct aesd_dev *dev = NULL; /* device information */

    PDEBUG("open");

	dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = dev; /* for other methods */

    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
	struct aesd_dev *dev; /* device information */

    PDEBUG("release");

	dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
	filp->private_data = NULL;

    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    struct aesd_buffer_entry *entry = NULL;
    struct aesd_dev *dev = NULL;
    size_t entry_offset=0U;
    size_t size_buff = 0U;
    unsigned long bytes_not_copied = 0U;

    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);

    dev = (struct aesd_dev *)(filp->private_data);
    if(dev == NULL)
    {
        PDEBUG("read error, dev NULL");
        return -ENOMEM;
    }

    if (mutex_lock_interruptible(&aesd_device.lock))
    {
        PDEBUG("read mutex lock error");
		return -ERESTARTSYS;
    }

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->circular_buffer, *f_pos, &entry_offset);
    if(entry == NULL)
    {
        PDEBUG("read entry error, no entry found");
        *f_pos = 0;
        goto exit;
    }
    PDEBUG("read, entry found, size: %zu, offset: %lld",entry->size, entry_offset);
    size_buff = entry->size - entry_offset;
    if(size_buff > count)
    {
        size_buff = count;
    }
    PDEBUG("read, copy to user, size: %lld",size_buff);
    bytes_not_copied = copy_to_user(buf, entry->buffptr+entry_offset, size_buff);
    retval = size_buff - bytes_not_copied;
    *f_pos = *f_pos + retval;

exit:
	mutex_unlock(&aesd_device.lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    struct aesd_dev *dev = NULL;
    unsigned long bytes_not_copied = 0U;

    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    dev = (struct aesd_dev *)(filp->private_data);
    if(dev == NULL)
    {
        PDEBUG("write error, dev NULL");
        return retval;
    }

    if (mutex_lock_interruptible(&aesd_device.lock))
    {
        PDEBUG("write mutex lock error");
		return -ERESTARTSYS;
    }

    if(dev->entry.buffptr == NULL)
    {
        dev->entry.buffptr = kmalloc(count, GFP_KERNEL);
    }
    else
    {
        dev->entry.buffptr = krealloc(dev->entry.buffptr, dev->entry.size + count, GFP_KERNEL);
    }

    if(dev->entry.buffptr == NULL)
    {
        PDEBUG("write kmalloc error");
        retval = -ENOMEM;
        goto fail;
    }
    bytes_not_copied = copy_from_user((void*)&dev->entry.buffptr[dev->entry.size], buf, count);
    PDEBUG("write copy to user: bytes written %zu", bytes_not_copied);
    retval = count - bytes_not_copied;
    dev->entry.size += retval;
    if(strnchr(dev->entry.buffptr, dev->entry.size,'\n'))
    {
        char *removed_entry = aesd_circular_buffer_add_entry(&dev->circular_buffer, &dev->entry);
        PDEBUG("write added entry: %lld", dev->entry.size);
        if(removed_entry != NULL)
        {
            kfree(removed_entry);
        }
        dev->entry.buffptr = NULL;
        dev->entry.size = 0U;
    }

fail:
	mutex_unlock(&aesd_device.lock);
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

void aesd_cleanup_module(void)
{
    int i = 0;
    struct aesd_buffer_entry *entry = NULL;

    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    for(i=0; i<AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;i++)
    {
        entry = &aesd_device.circular_buffer.entry[i];
        if(entry->buffptr != NULL)
        {
            kfree(entry->buffptr);
        }
    }
    mutex_destroy(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    PDEBUG("open");

    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        PDEBUG(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    mutex_init(&aesd_device.lock);
	if (mutex_lock_interruptible(&aesd_device.lock))
		return -ERESTARTSYS;

    aesd_circular_buffer_init(&aesd_device.circular_buffer);
	mutex_unlock(&aesd_device.lock);

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }

    return result;
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
