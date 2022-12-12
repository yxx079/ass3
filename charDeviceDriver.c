/*
 *  chardev.c: Creates a read-only char device that says how many times
 *  you've read from the dev file
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>	/* for put_user */
#include <charDeviceDriver.h>

#include <linux/gfp.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <asm/uaccess.h>

#define CHAR_MAG_MAX		(2 << 10)
#define CHAR_MSG_QUEUE_MAX	(CHAR_MAG_MAX * (2 << 4))
struct char_msg_queue {
	char *buf;
	size_t qhead, qtail;
	size_t qsize, qmask;
};

struct char_msg_queue mqueue;

static void char_queue_init(struct char_msg_queue *q, size_t qlen)
{
	q->buf = kzalloc(qlen, GFP_KERNEL);
	q->qhead = q->qtail = 0;

	q->qsize = qlen;
	q->qmask = (qlen - 1);
	
	return;
}

static void char_queue_free(struct char_msg_queue *q)
{
	kfree(q->buf);
	
	return;
}

static int char_queue_empty(struct char_msg_queue *q)
{
	if (q->qhead == q->qtail) {
		return 1;
	}

	return 0;
}

static int char_queue_full(struct char_msg_queue *q)
{
	size_t pos = (q->qtail + 1) & q->qmask;
	if (pos == q->qhead) {
		return 1;
	}
	
	return 0;
}

static inline size_t char_queue_count(struct char_msg_queue *queue)
{
    return (((queue->qtail + queue->qsize) - queue->qhead) & queue->qmask);
}

static void char_queue_enqueue(struct char_msg_queue *q, const char *data, size_t len)
{
    char *qdata;
	size_t left, right;

    qdata = q->buf + q->qtail;

	right = 0;
	if (q->qtail + len > q->qsize) {
		left = q->qsize - q->qtail;
		right = len - left;
	} else {
		left = len;
	}
	memcpy(qdata, data, left);
	if (right) {
		memcpy(q->buf, data + left, right);
	}
    q->qtail = (q->qtail + len) & q->qmask;
	
    return;
}

static size_t char_queue_dequeue(struct char_msg_queue *q, char *data, size_t len)
{
	char *qdata;
	size_t readn;
	size_t qlen = char_queue_count(q);
	size_t left, right;

	if (qlen < len) {
		readn = qlen;
	} else {
		readn = len;
	}

	right = 0;
	qdata = q->buf + q->qhead;
	if (q->qhead + readn > q->qsize) {
		left = q->qsize - q->qhead;
		right = readn - left;
	} else {
		left = readn;
	}
	memcpy(data, qdata, left);
	if (right) {
		memcpy(data + left, q->buf, right);
	}
	
	q->qhead = (q->qhead + readn) & q->qmask;

    return readn;
}


/* 
 * This function is called whenever a process tries to do an ioctl on our
 * device file. We get two extra parameters (additional to the inode and file
 * structures, which all device functions get): the number of the ioctl called
 * and the parameter given to the ioctl function.
 *
 * If the ioctl is write or read/write (meaning output is returned to the
 * calling process), the ioctl call returns the output of this function.
 *
 */


DEFINE_MUTEX  (devLock);

/*
 * Methods
 */

/* 
 * Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int device_open(struct inode *inode, struct file *file)
{
    mutex_lock (&devLock);
    if (Device_Open) {
		mutex_unlock (&devLock);
		return -EBUSY;
    }
	
    Device_Open++;
    mutex_unlock (&devLock);
	
    //sprintf(msg, "I already told you %d times Hello world!\n", counter++);

	try_module_get(THIS_MODULE);

	printk(KERN_INFO "device_open\n");
	
    return SUCCESS;
}

/* Called when a process closes the device file. */
static int device_release(struct inode *inode, struct file *file)
{
    mutex_lock (&devLock);
	Device_Open--;		/* We're now ready for our next caller */
	mutex_unlock (&devLock);
	/* 
	 * Decrement the usage count, or else once you opened the file, you'll
	 * never get get rid of the module. 
	 */
	module_put(THIS_MODULE);

	return 0;
}

/* 
 * Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
static ssize_t device_read(struct file *filp,	/* see include/linux/fs.h   */
			   char *buffer,	/* buffer to fill with data */
			   size_t length,	/* length of the buffer     */
			   loff_t * offset)
{
	/* result of function calls */
	size_t readn;
	char *buf;

	buf = kzalloc(length, GFP_KERNEL);
	mutex_lock (&devLock);
	if (char_queue_empty(&mqueue)) {
		printk(KERN_INFO "char_queue_empty\n");
		mutex_unlock (&devLock);
		kfree(buf);
		return -EAGAIN;
	}
	readn = char_queue_dequeue(&mqueue, buf, length);
	if (copy_to_user(buffer, buf, readn)) {
		printk(KERN_INFO "copy_to_user error\n");
	}

	*offset += readn;
	mutex_unlock (&devLock);
	
	/* 
	 * Most read functions return the number of bytes put into the buffer
	 */
	kfree(buf);
	return readn;
}

/* Called when a process writes to dev file: echo "hi" > /dev/hello  */
static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	char *buf;

	if (len > CHAR_MAG_MAX)
		return -EINVAL;

	buf = kzalloc(len, GFP_KERNEL);
	if (copy_from_user(buf, buff, len)) {
		printk(KERN_INFO "copy_from_user error\n");
	}
	mutex_lock (&devLock);
	if (char_queue_full(&mqueue)) {
		mutex_unlock (&devLock);
		printk(KERN_INFO "char_queue_empty\n");
		kfree(buf);
		return -EBUSY;
	}

	char_queue_enqueue(&mqueue, buf, len);
	*off += len;
	mutex_unlock (&devLock);

	kfree(buf);
	return len;
}

/*
 * This function is called when the module is loaded
 */
int init_module(void)
{
	Major = register_chrdev(0, DEVICE_NAME, &fops);
	if (Major < 0) {
	  printk(KERN_INFO "Registering char device failed with %d\n", Major);
	  return Major;
	}

	printk(KERN_INFO "I was assigned major number %d. To talk to\n", Major);
	printk(KERN_INFO "the driver, create a dev file with\n");
	printk(KERN_INFO "'mknod /dev/%s c %d 0'.\n", DEVICE_NAME, Major);
	printk(KERN_INFO "Try various minor numbers. Try to cat and echo to\n");
	printk(KERN_INFO "the device file.\n");
	printk(KERN_INFO "Remove the device file and module when done.\n");

	char_queue_init(&mqueue, CHAR_MSG_QUEUE_MAX);
	return SUCCESS;
}

/*
 * This function is called when the module is unloaded
 */
void cleanup_module(void)
{
	/*  Unregister the device */
	unregister_chrdev(Major, DEVICE_NAME);
	char_queue_free(&mqueue);

	return;
}

MODULE_LICENSE("GPL");

