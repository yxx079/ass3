/* Global definition for the example character device driver */
#define MESSAGE_LENGTH 4*1024
#define MESSAGE_SIZE 1000
#define SUCCESS 0
#define DEVICE_NAME "chardev"	/* Dev name as it appears in /proc/devices   */
#define BUF_LEN 80		/* Max length of the message from the device */

int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
static long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long);

static int Major;		
static int Device_Open = 0;
static char msg[BUF_LEN];	
static size_t msg_num=0;

typedef struct message{
    char* data;
    size_t length;
    struct list_head list;
}message_t;
struct cdev *my_cdev;
       dev_t dev_num;

static struct file_operations fops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.unlocked_ioctl = device_ioctl,
	.release = device_release
};
