#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/ioctl.h>

/* The major device number. We can't rely on dynamic 
 * registration any more, because ioctls need to know 
 * it. */
#define MAJOR_NUM 100

#define NAME_LEN 100

/* Set the message of the device driver */
#define IOCTL_SET_MSG _IOR(MAJOR_NUM, 0, char *)

/* Get the message of the device driver */
#define IOCTL_GET_MSG _IOR(MAJOR_NUM, 1, char *)

#define IOCTL_SET_BROAD_MSG _IOR(MAJOR_NUM, 2, char *)

#define IOCTL_GET_BROAD_MSG _IOR(MAJOR_NUM, 3, char *)

/* The name of the device file */
#define DEVICE_FILE_NAME "chatroom"

#endif