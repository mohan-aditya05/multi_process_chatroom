#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include "chardev.h"

#define BUFLEN 100
char buf[BUFLEN];
int fd;
bool exit_flag = false;
char name[NAME_LEN];

void *read_from_user(void *arg);
void *write_to_user(void *arg);
void ioctl_set_name(int file_desc, char *name);
void ioctl_set_broadcast_msg(int file_desc, char *msg);
char * ioctl_get_name(int file_desc);
char * ioctl_get_broadcast_msg(int file_desc);

void *read_from_user(void *arg)
{
    char *prev_msg=NULL;
    while(!exit_flag)
    {
        char buf_read[BUFLEN];
        char *proc_name, *broadcast_msg;
        sleep(5);
        if(read(fd, buf_read, BUFLEN) < 0)
        {
            perror("Read failed: ");
            exit(1);
        }
        // printf("Length: %d\n", strlen(buf_read));
        broadcast_msg = ioctl_get_broadcast_msg(fd);
        if(strlen(buf_read)>0)
        {
            proc_name = ioctl_get_name(fd);
            if(strlen(proc_name)>0)
            {
                printf("\n%s : %s\n> ", proc_name, buf_read);
                printf("\n%s > ", name);
                fflush(stdout);
            }
        }
        else if(broadcast_msg != NULL && strlen(broadcast_msg)>0)
        {
            if(prev_msg!=NULL)
            if(strcmp(prev_msg, broadcast_msg)==0)
                continue;

            if(strlen(broadcast_msg)>0)
            {
                printf("\n%s\n", broadcast_msg);
                printf("\n%s > ", name);
                fflush(stdout);
            }
            prev_msg = broadcast_msg;
        }

        memset(buf_read, 0, strlen(buf_read));
    }
    pthread_exit(0);
}

void *write_to_user(void *arg)
{
    while(!exit_flag)
    {
        printf("\n%s > ", name);
        fgets(buf, BUFLEN, stdin);
        buf[strcspn(buf, "\n")] = 0;
        // printf("strcmp: %d %c", strcmp(buf, "Bye!"), buf[4]);
        if (strcmp(buf, "Bye!") == 0)
        {
            printf("Exiting... \n");
            exit_flag = true;
            sprintf(buf, "%s left", name);
            ioctl_set_broadcast_msg(fd, buf);
            memset(buf, 0, BUFLEN);
            pthread_exit(0);
        }

        if(write(fd, buf, strlen(buf)+1) < 0)
        {
            perror("write failed: ");
            exit(1);
        }
        memset(buf, 0, BUFLEN);
    }
    pthread_exit(0);
}

void ioctl_set_name(int file_desc, char *name)
{
    long ret;

    ret = ioctl(file_desc, IOCTL_SET_MSG, name);

    if(ret<0)
    {
        printf("IOCTL_SET_MSG failed!\n");
        exit(-1);
    }
}

char *ioctl_get_name(int file_desc)
{
    long ret;
    char *name = (char *)malloc(sizeof('a')*NAME_LEN);

    ret = ioctl(file_desc, IOCTL_GET_MSG, name);

    if(ret<0)
    {
        printf("IOCTL_GET_MSG failed!\n");
        exit(-1);
    }
    // printf("Name: %s\n", name);
    return name;
}

void ioctl_set_broadcast_msg(int file_desc, char *msg)
{
    long ret;

    ret = ioctl(file_desc, IOCTL_SET_BROAD_MSG, msg);

    if(ret<0)
    {
        printf("IOCTL_SET_BROAD_MSG failed!\n");
        exit(-1);
    }
}

char * ioctl_get_broadcast_msg(int file_desc)
{
    long ret;
    char *msg = (char *)malloc(sizeof('a')*NAME_LEN);

    ret = ioctl(file_desc, IOCTL_GET_BROAD_MSG, msg);

    if(ret<0)
    {
        printf("IOCTL_GET_BROAD_MSG failed!\n");
        exit(-1);
    }
    // printf("Name: %s\n", name);
    if(strlen(msg)>0)
        return msg;
    else
        return NULL;
}

int main(int argc, char *argv[])
{
    strcpy(name, argv[1]);

    printf("PID: %d\n", getpid());
    fd = open("/dev/chatroom", O_RDWR);
    if (fd < 0)
    {
        perror("Open failed: ");
        exit(1);
    }

    char *msg = (char *)malloc(sizeof('a')*NAME_LEN);
    sprintf(msg, "%s has joined the chat!", name);

    ioctl_set_name(fd, name);
    ioctl_set_broadcast_msg(fd, msg);

    pthread_t thread_arr[2];
    int ret_value = pthread_create(&thread_arr[0],
                                    NULL,
                                    read_from_user,
                                    NULL);

    if(ret_value < 0)
    {
        perror("Pthread create error: read");
        exit(1);
    }

    ret_value = pthread_create(&thread_arr[1],
                                NULL,
                                write_to_user,
                                NULL);

    if(ret_value < 0)
    {
        perror("Pthread create error: write");
        exit(1);
    }

    while(!exit_flag)
    {
        /*do nothing. Wait for user to end the write thread by typing "Bye!" */
    }

    pthread_join(thread_arr[1], 0);
    pthread_join(thread_arr[0], 0);

}