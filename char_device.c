#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/kfifo.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include "chardev.h"

#define FIFO_SIZE 65535

#define PROC_LIMIT 10

int que_lock = 0;

struct kfifo_rec_ptr_2 que[PROC_LIMIT];
struct kfifo_rec_ptr_2 name_que[PROC_LIMIT];

int count = 0;
int used = 0;
spinlock_t mylock[PROC_LIMIT];
pid_t proc_id_map[PROC_LIMIT];
char *name_map[PROC_LIMIT];
char *broadcast_msg=NULL;

static int search_process_by_id(pid_t pid)
{
	int i=0;
	for(i=0;i<PROC_LIMIT;i++)
	{
		if(proc_id_map[i] == pid)
		{
			return i;
		}
	}
	return -1;
}

static long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
{
	int idx, len;
	char *temp;
	if (ioctl_num == IOCTL_SET_MSG)
	{	
		temp = (char *)kmalloc(NAME_LEN, GFP_USER);
		if(!temp) 
			return -1; //kmalloc failed
		len = copy_from_user(temp, (char *)ioctl_param, NAME_LEN);

		idx = search_process_by_id(get_current()->tgid);
		name_map[idx] = temp;
	}
	else if(ioctl_num == IOCTL_GET_MSG)
	{
		char *name;
		name = (char *)kmalloc(NAME_LEN, GFP_USER);
		if(!name)
			return -1;

		printk("ioctl get call\n");
		idx = search_process_by_id(get_current()->tgid);
		if(idx==-1)
		{
			printk("Something went wrong: couldn't get pid");
			return -1;
		}

		spin_lock_init(&mylock[idx]);
		len = kfifo_out_spinlocked(&name_que[idx], name, NAME_LEN, &mylock[idx]);
		name[len] = '\0';
		// temp = name;
		printk("Out: %s\n", name);
		len = copy_to_user((char *)ioctl_param, name, strlen(name)+1);
	}
	else if(ioctl_num == IOCTL_SET_BROAD_MSG)
	{
		temp = (char *)kmalloc(NAME_LEN, GFP_USER);
		if(!temp) 
			return -1; //kmalloc failed
		len = copy_from_user(temp, (char *)ioctl_param, NAME_LEN);
		broadcast_msg = temp;
	}
	else if(ioctl_num == IOCTL_GET_BROAD_MSG)
	{
		if(broadcast_msg!=NULL)
			len = copy_to_user((char *)ioctl_param, broadcast_msg, strlen(broadcast_msg));
	}
	return 0;
}

static int myopen(struct inode *inode, struct file *file)
{
	int ret;
	pid_t curr_id;

	printk("myopen called in module\n");
	curr_id = get_current()->tgid;
	printk("OPEN PID: %d\n", curr_id);

    ret = kfifo_alloc(&que[count], FIFO_SIZE, GFP_KERNEL);
	if (ret || used >= PROC_LIMIT) {
        printk(KERN_ERR "error kfifo_alloc\n");
        return -1;
    }
	ret = kfifo_alloc(&name_que[count], FIFO_SIZE, GFP_KERNEL);
    if (ret || used >= PROC_LIMIT) {
        printk(KERN_ERR "error kfifo_alloc\n");
        return -1;
    }
	
	proc_id_map[count] = curr_id;
	count++;
	used++;

	return 0;
}

static int myclose(struct inode *inodep, struct file *filp)
{
	int idx;
	printk("myclose called in module\n");
	printk("CLOSE PID: %d\n", get_current()->tgid);
	idx = search_process_by_id(get_current()->tgid);
	printk("IDX: %d\n", idx);

	swap(proc_id_map[idx], proc_id_map[used-1]);
	swap(que[idx], que[used-1]);
	swap(name_que[idx], name_que[used-1]);
	swap(name_map[idx], name_map[used-1]);

	if(idx!=-1)
		kfifo_free(&que[used-1]);

	used--;
	count--;

	if(used==0)
		kfree(broadcast_msg);

	return 0;
}

static ssize_t mywrite(struct file *file, const char __user *buf, size_t len, loff_t *ppos)
{
	char *module_buf;
	int idx, i;

	printk("mywrite function called in module\n");

	module_buf = (char *)kmalloc(len, GFP_USER);
	if(!module_buf) 
		return -1; //kmalloc failed

	len = copy_from_user(module_buf, buf, len);
	printk("Received this string from user: %s\n", module_buf);

	printk("WRITE PID: %d\n",get_current()->tgid);
	idx = search_process_by_id(get_current()->tgid);
	if(idx==-1)
	{
		printk("Something went wrong: couldn't get pid\n");
		return -1;
	}

	for(i=0; i<used; i++)
	{
		if(i!=idx)
		{
			printk("Written on: %d\n", i);
			spin_lock_init(&mylock[i]);
			kfifo_in_spinlocked(&que[i], module_buf, strlen(module_buf), &mylock[i]);
			kfifo_in_spinlocked(&name_que[i], name_map[idx], strlen(name_map[idx]), &mylock[i]);
		}
	}

	kfree(module_buf); // clean up
	
	return len; 

}

static ssize_t myread(struct file *file, char __user *buf, size_t len, loff_t *ppos)
{
	char *module_buf;
	int idx, ret;
	printk("myread function called in module\n");

	module_buf = (char *)kmalloc(len, GFP_USER);
	if(!module_buf) 
		return -1; //kmalloc failed

	printk("READ PID: %d\n",get_current()->tgid);
	idx = search_process_by_id(get_current()->tgid);
	if(idx==-1)
	{
		printk("Something went wrong: couldn't get pid");
		return -1;
	}

	spin_lock_init(&mylock[idx]);

	if(!kfifo_is_empty(&que[idx]))
	{
		printk("not empty\n");
		ret = kfifo_out_spinlocked(&que[idx], module_buf, len, &mylock[idx]);
		module_buf[ret] = '\0';
		printk("Out: %s\n", module_buf);
		len = copy_to_user(buf, module_buf, strlen(module_buf)+1);
	}

	kfree(module_buf);

	return len; 
}


static const struct file_operations myfops = {
    .owner	= THIS_MODULE,
    .read	= myread,
    .write	= mywrite,
    .open	= myopen,
    .release	= myclose,
    .llseek 	= no_llseek,
	.unlocked_ioctl = device_ioctl
};

struct miscdevice mydevice = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "chatroom",
    .fops = &myfops,
    .mode = S_IRUGO | S_IWUGO,
};

static int __init my_init(void)
{
	printk("my_init called\n");

	// register the character device
	if (misc_register(&mydevice) != 0) {
		printk("device registration failed\n");
		return -1;
	}

	printk("character device registered\n");
	return 0;
}

static void __exit my_exit(void)
{
	printk("my_exit called\n");
	misc_deregister(&mydevice);
}

module_init(my_init)
module_exit(my_exit)
MODULE_DESCRIPTION("Miscellaneous character device module\n");
MODULE_AUTHOR("Aditya Mohan");
MODULE_LICENSE("GPL");