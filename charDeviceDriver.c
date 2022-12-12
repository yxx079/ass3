/*
 *  chardev.c: Creates a read-only char device that says how many times
 *  you've read from the dev file
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>	/* for put_user */
#include <charDeviceDriver.h>
#include "ioctl.h"
#include <linux/slab.h>
#include <linux/list.h>
DEFINE_MUTEX  (msg_lock);
static int counter = 0;
LIST_HEAD(messages);
MODULE_LICENSE("GPL");




static long device_ioctl(struct file *file,	/* see include/linux/fs.h */
		 unsigned int ioctl_num,	
		 unsigned long ioctl_param)
{

	/*
	 * Switch according to the ioctl called
	 */
	if (ioctl_num == RESET_COUNTER) {
	    counter = 0;
	
	    return 5; /* can pass integer as return value */
	}

	else {
	    /* no operation defined - return failure */
	    return -EINVAL;

	}
}

int init_module(void)
{
        Major = register_chrdev(0, DEVICE_NAME, &fops);

	if (Major < 0) {
	  printk(KERN_ALERT "Registering char device failed with %d\n", Major);
	  return Major;
	}

	printk(KERN_INFO "I was assigned major number %d. To talk to\n", Major);
	printk(KERN_INFO "the driver, create a dev file with\n");
	printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);
	printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n");
	printk(KERN_INFO "the device file.\n");
	printk(KERN_INFO "Remove the device file and module when done.\n");

	return SUCCESS;
}


void cleanup_module(void)
{
    message_t * msg;
    while(!list_empty(&messages)){
        msg=list_first_entry_or_null(&messages,message_t,list);
        list_del(&msg->list);
        kfree(msg->data);
        kfree(msg);
    }
	/*  Unregister the device */
	unregister_chrdev(Major, DEVICE_NAME);
}


static int device_open(struct inode *inode, struct file *file)
{

    mutex_lock (&msg_lock);
    if (Device_Open) {
	mutex_unlock (&msg_lock);
	return -EBUSY;
    }
    Device_Open++;
    mutex_unlock (&msg_lock);
    sprintf(msg, "I already told you %d times Hello world!\n", counter++);
    try_module_get(THIS_MODULE);

    return SUCCESS;
}

static int device_release(struct inode *inode, struct file *file)
{
    mutex_lock (&msg_lock);
	Device_Open--;		/* We're now ready for our next caller */
	mutex_unlock (&msg_lock);
	
	module_put(THIS_MODULE);

	return 0;
}

static ssize_t 
device_read(struct file *filp,char *buffer,	size_t length,loff_t * offset)
{
	message_t* mess;
	size_t len;
	int result;
	mutex_lock(&msg_lock);
    if(list_empty(&messages)){
        mutex_unlock(&msg_lock);
        return -EAGAIN;
    }
    mess=list_first_entry(&messages,message_t,list);
    len=mess->length+1;
    result=copy_to_user(buffer,mess->data,len);
    if(result>0){
        mutex_unlock(&msg_lock);
        return -EFAULT;
    }
    msg_num=msg_num-1;
    list_del(&mess->list);
    kfree(mess->data);
    kfree(mess);
    mutex_unlock(&msg_lock);
	return len;
}

/* Called when a process writes to dev file: echo "hi" > /dev/hello  */
static ssize_t
device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	message_t* mess;
	int result;
	mutex_lock(&msg_lock);

	if(len-1>MESSAGE_LENGTH){
        mutex_unlock(&msg_lock);
        return -EINVAL;
	}
	if(msg_num==MESSAGE_SIZE){
        mutex_unlock(&msg_lock);
        return -EAGAIN;
	}
	mess=kmalloc(sizeof(message_t),GFP_KERNEL);
	mess->data=kmalloc(len,GFP_KERNEL);
	result=copy_from_user(mess->data,buff,len);
	if(result>0){
        kfree(mess->data);
        kfree(mess);
        mutex_unlock(&msg_lock);
        return -EFAULT;
    }
	INIT_LIST_HEAD(&mess->list);

	list_add(&mess->list,&messages);
	mess->length=len-1;
	msg_num=msg_num+1;
	mutex_unlock(&msg_lock);

	return len;
}
