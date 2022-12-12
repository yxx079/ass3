/*
 *  chardev.c: Creates a read-only char device that says how many times
 *  you've read from the dev file
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>	
#include <charDeviceDriver.h>
#include <linux/slab.h>
MODULE_LICENSE("GPL");


DEFINE_MUTEX  (msg_lock);
#define msgLen 4096
#define msgSize 1000
#define RESET_COUNTER 0 
static int counter = 0;

static long device_ioctl(struct file *file,	 unsigned int ioctl_num,unsigned long ioctl_param)
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


void cleanup_module(void)
{
	message_t* temp;
	message_t* to_f;
	temp=messages;

	while(temp!=NULL){
        to_f=temp;
        temp=temp->next;
        kfree(to_f->msg);
        kfree(to_f);
	}
	unregister_chrdev(Major, DEVICE_NAME);
}
static int device_release(struct inode *inode, struct file *file)
{
    mutex_lock (&msg_lock);
	Device_Open--;		
	mutex_unlock (&msg_lock);
	
	module_put(THIS_MODULE);

	return 0;
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
    counter++;
    try_module_get(THIS_MODULE);

    return SUCCESS;
}



static ssize_t
device_write(struct file *filp, 
             const char *buff, 
             size_t len,
              loff_t * off)
{   int out;
	message_t* message;
	message_t* temp;
	
	mutex_lock(&msg_lock);
	if(len-1>msgLen){
        mutex_unlock(&msg_lock);
        return -EINVAL;
	}
	if(messages_size>msgSize){
        mutex_unlock(&msg_lock);
        return -EAGAIN;
	}
	message=kmalloc(sizeof(message_t),GFP_KERNEL);
	message->msg=kmalloc(len,GFP_KERNEL);
	message->next=NULL;
	message->length=len-1;
	out=copy_from_user(message->msg,buff,len);
	if(out>0){
        kfree(message->msg);
        kfree(message);
        mutex_unlock(&msg_lock);
        return -EFAULT;
	}
	if(messages==NULL){
        messages=message;
	}
	else{
        temp=messages;
        while(temp->next!=NULL){
            temp=temp->next;
        }
        temp->next=message;
	}
	messages_size+=1;
	mutex_unlock(&msg_lock);
	return len;
}


static ssize_t device_read(struct file *filp,	
			   char *buffer,	
			   size_t length,	
			   loff_t * offset)
{
	int out;
	size_t len;
	message_t* message;
	mutex_lock(&msg_lock);
	if(messages==NULL){
        mutex_unlock(&msg_lock);
        return -EAGAIN;
	}
	message=messages;
	messages=messages->next;
	len=message->length+1;
    out=copy_to_user(buffer,message->msg,len);
	if(out>0){
        mutex_unlock(&msg_lock);
        return -EFAULT;
	}
	messages_size-=1;
	kfree(message->msg);
	kfree(message);
	mutex_unlock(&msg_lock);
	return len;
}
