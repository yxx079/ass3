

int init_module(void);
void cleanup_module(void);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
static long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long);

#define SUCCESS 0
#define DEVICE_NAME "chardev"	


struct cdev *my_cdev;
       dev_t dev_num;

static int Major;		
static int Device_Open = 0;	

typedef struct message_t{
    char* msg;
    size_t length;
    struct message_t* next;
}message_t;

static message_t* messages=NULL;
static size_t messages_size=0;

static struct file_operations fops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.unlocked_ioctl = device_ioctl,
	.release = device_release
};