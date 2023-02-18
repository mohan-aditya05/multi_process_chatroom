# multi_process_chatroom
## Chatroom implementation using character devices

***The work done here may only be used as reference material. The work here is not to be submitted as your own, with or without edits.***
<br> 
<br>

How to compile the kernel module and add the module to the kernel

```
$ make
$ sudo insmod char_device.ko
```

Compiling the user space program
```
$ gcc -o join user_process.c -lpthread
```

Joining the chatroom
```
$ ./join <name of the process or user>
```

Leaving the chatroom
```
Either type "Bye!" or Ctrl+C.
```
