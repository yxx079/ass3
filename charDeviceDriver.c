#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>	/* for put_user */
#include <charDeviceDriver.h>
#include "ioctl.h"
#include <linux/slab.h>
#include <linux/list.h>

MODULE_LICENSE("GPL");

DEFINE_MUTEX  (msg_lock);
#define MESSAGE_LENGTH 4*1024
#define MESSAGE_SIZE 1000
static int counter = 0;
LIST_HEAD(messages);

static long device_ioctl(struct file *file,
		 unsigned int ioctl_num,	
		 unsigned long ioctl_param)
{

	
	if (ioctl_num == RESET_COUNTER) {
	    counter = 0;
	
	    return 5; 
	}

	else {
	    
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

/*
 * This function is called when the module is unloaded
 */
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

/* Called when a process closes the device file. */
static int device_release(struct inode *inode, struct file *file)
{
    mutex_lock (&msg_lock);
	Device_Open--;		
	mutex_unlock (&msg_lock);
	
	module_put(THIS_MODULE);

	return 0;
}


static ssize_t device_read(struct file *filp,	/* see include/linux/fs.h   */
			   char *buffer,	/* buffer to fill with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{
	message_t* msg;
	size_t len;
	int result;
	mutex_lock(&msg_lock);
    if(list_empty(&messages)){
        mutex_unlock(&msg_lock);
        return -EAGAIN;
    }
    msg=list_first_entry(&messages,message_t,list);
    len=msg->length+1;
    result=copy_to_user(buffer,msg->data,len);
    if(result>0){
        mutex_unlock(&msg_lock);
        return -EFAULT;
    }
    msgNum=msgNum-1;
    list_del(&msg->list);
    kfree(msg->data);
    kfree(msg);
    mutex_unlock(&msg_lock);
	return len;
}


static ssize_t
device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	message_t* msg;
	int result;
	mutex_lock(&msg_lock);

	if(len-1>MESSAGE_LENGTH){
        mutex_unlock(&msg_lock);
        return -EINVAL;
	}
	if(msgNum==MESSAGE_SIZE){
        mutex_unlock(&msg_lock);
        return -EAGAIN;
	}
	msg=kmalloc(sizeof(message_t),GFP_KERNEL);
	msg->data=kmalloc(len,GFP_KERNEL);
	result=copy_from_user(msg->data,buff,len);
	if(result>0){
        kfree(msg->data);
        kfree(msg);
        mutex_unlock(&msg_lock);
        return -EFAULT;
    }
	INIT_LIST_HEAD(&msg->list);

	list_add(&msg->list,&messages);
	msg->length=len-1;
	msgNum=msgNum+1;
	mutex_unlock(&msg_lock);

	return len;
}
