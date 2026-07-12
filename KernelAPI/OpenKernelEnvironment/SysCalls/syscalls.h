#ifndef SYSCALLS_H
#define SYSCALLS_H

extern int syscall(int id, void* arg);
extern int syscall_handler(int id, void* arg);

#endif