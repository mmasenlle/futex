
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#ifndef KERNELv4
#	include <linux/cdev.h>
#endif
#include "box_futex.h"
#ifndef iminor
#	define iminor(inode) MINOR(inode->i_rdev)
#endif

#define BOXFUTEX_LOGID 		BOXFUTEX_MNAME ": "
#define BOXFUTEX_PROC_ENTRY 	BOXFUTEX_MNAME

#define BOXFUTEX_MAXUSERS 30

enum {
	BOXFUTEX_WAITING_N = 0,
	BOXFUTEX_WAITING_R,
	BOXFUTEX_WAITING_W
};

static struct proc_dir_entry *boxfutex_proc_entry = NULL;
static dev_t devbase = MKDEV(206, 0);
#ifndef KERNELv4
static struct cdev boxfutex_cdev[BOXFUTEX_MAXBFUTEX];
#endif

static DECLARE_MUTEX(boxfutex_mtx);

struct boxfutex_data_t {
	struct box_futex_ulock_t *ulock;
	spinlock_t spinlock;
	struct list_head list;
	int list_size;
	unsigned long users;
};
static unsigned long boxfutex_page = 0;
static struct boxfutex_data_t *boxfutexes[BOXFUTEX_MAXBFUTEX];

struct boxfutex_user_data_t {
	int id;
	struct boxfutex_data_t *bf_data;
	struct list_head list;
	struct task_struct *task;
	int waiting;
};

static int boxfutex_wait(struct boxfutex_user_data_t *data)
{
	unsigned long flags;

	spin_lock_irqsave(&data->bf_data->spinlock, flags);
	
	if(data->bf_data->ulock->status & BOXFUTEX_WAITMARK) {
		data->bf_data->list_size++;
		list_add_tail(&data->list, &data->bf_data->list);

		do {
			if (signal_pending_state(TASK_INTERRUPTIBLE, data->task)) {
				list_del(&data->list);
				data->bf_data->list_size--;
				spin_unlock_irqrestore(&data->bf_data->spinlock, flags);
				return -EINTR;
			}
			__set_task_state(data->task, TASK_INTERRUPTIBLE);
			spin_unlock_irq(&data->bf_data->spinlock);
			schedule_timeout(MAX_SCHEDULE_TIMEOUT);
			spin_lock_irq(&data->bf_data->spinlock);
		} while (data->waiting);
	}
	
	spin_unlock_irqrestore(&data->bf_data->spinlock, flags);
	return 0;
}

static void boxfutex_wake(struct boxfutex_user_data_t *data)
{
	unsigned long flags;
	struct boxfutex_user_data_t *waiter;
#define BF_WAKE_CONT_BASE 1000
	int cont = BF_WAKE_CONT_BASE;

	spin_lock_irqsave(&data->bf_data->spinlock, flags);
	
	while(!list_empty(&data->bf_data->list) && cont) {
		waiter = list_first_entry(&data->bf_data->list, struct boxfutex_user_data_t, list);
		if(cont != BF_WAKE_CONT_BASE && waiter->waiting == BOXFUTEX_WAITING_W)
			break;
		list_del(&waiter->list);
		data->bf_data->list_size--;
		if(waiter->waiting == BOXFUTEX_WAITING_W) {
			cont = 0;
		} else {
			cont--;
		}
		waiter->waiting = BOXFUTEX_WAITING_N;
		wake_up_process(waiter->task);
	}

	for (;;) {
		unsigned long status = data->bf_data->ulock->status;
		if(status == BOXFUTEX_WAITMARK) {
			unsigned long old_status = _bf_cmpxchg(&data->bf_data->ulock->status, status, 0);
			if(_bf_likely(status == old_status))
				break;
		} else {
			break;
		}
	}

	spin_unlock_irqrestore(&data->bf_data->spinlock, flags);
}


/*  /dev/box_futexN file operations */

static int boxfutex_open(struct inode *inode, struct file *file)
{
	int i, nbftx = (iminor(inode) - MINOR(devbase));
	struct boxfutex_user_data_t *user_data = NULL;

	if(nbftx >= BOXFUTEX_MAXBFUTEX)
		return -ENODEV;

	if(down_interruptible(&boxfutex_mtx))
		return -ERESTARTSYS;

	if(!boxfutex_page) {
		boxfutex_page = __get_free_page(GFP_KERNEL);
		if(!boxfutex_page) {
			up(&boxfutex_mtx);
			return -ENOMEM;
		}
	}
	if(!boxfutexes[nbftx]) {
		boxfutexes[nbftx] = (struct boxfutex_data_t*)kmalloc(sizeof(struct boxfutex_data_t), GFP_KERNEL);
		if(!boxfutexes[nbftx]) {
			up(&boxfutex_mtx);
			return -ENOMEM;
		}
		boxfutexes[nbftx]->ulock = BOXFUTEX_ULOCK_ADDR(boxfutex_page, nbftx);
		boxfutexes[nbftx]->ulock->status = 0;
		spin_lock_init(&boxfutexes[nbftx]->spinlock);
		INIT_LIST_HEAD(&boxfutexes[nbftx]->list);
		boxfutexes[nbftx]->list_size = 0;
		boxfutexes[nbftx]->users = 0;
	}
	for(i = 0; i < BOXFUTEX_MAXUSERS; i++) {
		if(!(boxfutexes[nbftx]->users & BOXFUTEX_IDMARK(i))) {
			boxfutexes[nbftx]->users |= BOXFUTEX_IDMARK(i);
			break;
		}
	}
	if(i == BOXFUTEX_MAXUSERS) {
		up(&boxfutex_mtx);
		return -EBUSY;
	}
	if(!(user_data = kmalloc(sizeof(struct boxfutex_user_data_t), GFP_KERNEL))) {
		up(&boxfutex_mtx);
		return -ENOMEM;
	}
	user_data->bf_data = boxfutexes[nbftx];
	INIT_LIST_HEAD(&user_data->list);
	user_data->task = current;
	user_data->waiting = BOXFUTEX_WAITING_N;
	user_data->id = i;
	file->private_data = user_data;

	up(&boxfutex_mtx);
	return 0;
}

static int boxfutex_release(struct inode *inode, struct file *file)
{
	struct boxfutex_user_data_t *data = (struct boxfutex_user_data_t *)file->private_data;

	for (;;) {
		unsigned long status = data->bf_data->ulock->status;
		if(status & BOXFUTEX_IDMARK(data->id)) {
			unsigned long old_status = _bf_cmpxchg(&data->bf_data->ulock->status,
									status, status & ~(BOXFUTEX_WRITING | BOXFUTEX_IDMARK(data->id)));
			if(_bf_likely(status == old_status)) {
				if((status & ~(BOXFUTEX_WRITING | BOXFUTEX_IDMARK(data->id))) == BOXFUTEX_WAITMARK)
					boxfutex_wake(data);
				break;
			}
		} else {
			break;
		}
	}

	data->bf_data->users &= ~BOXFUTEX_IDMARK(data->id);
	kfree(data);

	return 0;
}

static ssize_t boxfutex_read(struct file *file, char *buf, size_t len, loff_t *lff) 
{
	return ((struct boxfutex_user_data_t *)file->private_data)->id;
}

static ssize_t boxfutex_write(struct file *file, const char *buf, size_t len, loff_t *lff)
{
	struct boxfutex_user_data_t *data = (struct boxfutex_user_data_t *)file->private_data;

	switch(len) {
	case BOXFUTEX_REQ_READ:
		data->waiting = BOXFUTEX_WAITING_R;
		if(boxfutex_wait(data))
			return -ERESTARTSYS; 
		return 0;
	case BOXFUTEX_REQ_WRITE:
		data->waiting = BOXFUTEX_WAITING_W;
		if(boxfutex_wait(data))
			return -ERESTARTSYS; 
		return 0;
	case BOXFUTEX_REQ_LEAVE:
		boxfutex_wake(data);
		return 0;
	}

	return -EINVAL;
}

static int boxfutex_mmap(struct file *filp, struct vm_area_struct *vma)
{
	if ((vma->vm_end - vma->vm_start) > PAGE_SIZE || (vma->vm_pgoff != 0))
		return -EINVAL;
	
#ifndef KERNELv4
	if(remap_pfn_range(vma, vma->vm_start, __pa(boxfutex_page) >> PAGE_SHIFT, PAGE_SIZE, vma->vm_page_prot))
		return -EAGAIN;
#else
	if(remap_page_range(vma->vm_start, __pa(boxfutex_page), PAGE_SIZE, vma->vm_page_prot))
		return -EAGAIN;
#endif

	return 0;
}

static struct file_operations boxfutex_fops = {
	.owner = THIS_MODULE,
	.open  = boxfutex_open,
	.release = boxfutex_release,
	.read  = boxfutex_read,
	.write = boxfutex_write,
	.mmap = boxfutex_mmap,
};

/*  /proc/box_futex file operations */

static ssize_t boxfutex_proc_read(struct file *file, char *buf, size_t len, loff_t *lff)
{
	ssize_t ret = 0, n, i;
	char buffer[128];
	
	if(file->private_data) {
		file->private_data = (void*)0;
		return 0;
	}
	file->private_data = (void*)1;

	for(i = 0; i < BOXFUTEX_MAXBFUTEX; i++) {
		if(boxfutexes[i]) {
			n = snprintf(buffer, sizeof(buffer), "Futex %d) Status: %08lx  Waiting: % 2d Users: %08lx\n",
					i, boxfutexes[i]->ulock->status, boxfutexes[i]->list_size, boxfutexes[i]->users);
			if((len < (ret + n)) || copy_to_user(buf + ret, buffer, n))
				return -EFAULT;
			ret += n;
		}
	}

	return ret;
}

static struct file_operations boxfutex_proc_ops = {
	.owner = THIS_MODULE,
	.read = boxfutex_proc_read,
};

static int __init boxfutex_init(void)
{
	int i, result;
	
	boxfutex_page = 0;
	memset(boxfutexes, 0, sizeof(boxfutexes));
	
#ifndef KERNELv4
	if((result = register_chrdev_region(devbase, BOXFUTEX_MAXBFUTEX, BOXFUTEX_MNAME)) < 0) {
		printk(KERN_WARNING BOXFUTEX_LOGID "register_chrdev_region %d (%d)\n", MAJOR(devbase), BOXFUTEX_MAXBFUTEX);
		return result;
	}
	for(i = 0; i < BOXFUTEX_MAXBFUTEX; i++) {
		dev_t devno = MKDEV(MAJOR(devbase), MINOR(devbase) + i);
		cdev_init(&boxfutex_cdev[i], &boxfutex_fops);
		boxfutex_cdev[i].owner = THIS_MODULE;
		boxfutex_cdev[i].ops = &boxfutex_fops;
		if((result = cdev_add(&boxfutex_cdev[i], devno, 1)))
			printk(KERN_NOTICE BOXFUTEX_LOGID "Error %d adding box_futex%d", result, i);
	}
#else
	if((result = register_chrdev(MAJOR(devbase), BOXFUTEX_MNAME, &boxfutex_fops)) < 0) {
		printk(KERN_WARNING BOXFUTEX_LOGID "register_chrdev %d\n", MAJOR(devbase));
		return result;
	}
#endif
	boxfutex_proc_entry = create_proc_entry(BOXFUTEX_PROC_ENTRY, S_IFREG | 0666, NULL);
	if(!boxfutex_proc_entry) {
		printk(KERN_WARNING BOXFUTEX_LOGID "error creating proc entry '" BOXFUTEX_PROC_ENTRY "'\n");
	} else {
		boxfutex_proc_entry->proc_fops = &boxfutex_proc_ops;
	}

	printk(KERN_INFO BOXFUTEX_LOGID "Started OK (v0.1.0 - " __DATE__ ")\n");
	return 0;
}

static void __exit boxfutex_exit(void)
{
	int i;
	printk(KERN_INFO BOXFUTEX_LOGID "Exiting ...\n");
	if(boxfutex_proc_entry) remove_proc_entry(BOXFUTEX_PROC_ENTRY, NULL);
#ifndef KERNELv4
	for(i = 0; i < BOXFUTEX_MAXBFUTEX; i++)	cdev_del(&boxfutex_cdev[i]);
	unregister_chrdev_region(devbase, BOXFUTEX_MAXBFUTEX);
#else
	if(unregister_chrdev(MAJOR(devbase), BOXFUTEX_MNAME) < 0)
		printk(KERN_WARNING BOXFUTEX_LOGID "unregister_chrdev %d\n", MAJOR(devbase));
#endif
	for(i = 0; i < BOXFUTEX_MAXBFUTEX; i++) {
		if(boxfutexes[i]) {
			kfree(boxfutexes[i]);
			boxfutexes[i] = NULL;
		}
	}
	if(boxfutex_page) free_page(boxfutex_page);
}

module_init(boxfutex_init);
module_exit(boxfutex_exit);
MODULE_LICENSE("Dual BSD/GPL");

