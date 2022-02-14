#pragma once

#include <core/syscallno.h>
#include <core/trapframe.h>
#include <fs/inode.h>
#include <fs/cache.h>
#include <fs/file.h>
#include <mod/syscall.h>

void sys_myexecve(char *s);
NO_RETURN void sys_myexit();
void sys_myprint(int x);
u64 syscall_dispatch(Trapframe *frame);

int sys_yield();
usize sys_brk(usize);
int sys_clone(PTRAP_FRAME);
int sys_wait4(PTRAP_FRAME);
int sys_exit();
int sys_dup(int);
isize sys_read(int, void*, int);
isize sys_write(int, void*, int);
isize sys_writev(int, void*, int);
int sys_close(int);
int sys_fstat(int, struct stat *);
int sys_fstatat(PTRAP_FRAME);
Inode *create(char *path, short type, short major, short minor, OpContext *ctx);
int sys_openat(int, char*, int);
int sys_mkdirat(int, char*, int);
int sys_mknodat(PTRAP_FRAME);
int sys_chdir(char*);
int sys_exec(PTRAP_FRAME);
int argint(int n, int *ip);
int argu64(int n, u64 *ip);
int argptr(int n, char **pp, usize size);
int argstr(int n, char **pp);