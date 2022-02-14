//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include <fcntl.h>

#include <aarch64/mmu.h>
#include <common/defines.h>
#include <common/spinlock.h>
#include <core/console.h>
#include <core/proc.h>
#include <core/sched.h>
#include <core/sleeplock.h>
#include <fs/file.h>
#include <fs/fs.h>

#include "syscall.h"

struct iovec {
    void *iov_base; /* Starting address. */
    usize iov_len;  /* Number of bytes to transfer. */
};

/*
 * Fetch the nth word-sized system call argument as a file descriptor
 * and return both the descriptor and the corresponding struct file.
 */
static int argfd(int n, i64 *pfd, struct file **pf) {
    // TODO
    // i32 fd;
    // struct file *f;

    // if (argint(n, &fd) < 0)
    //     return -1;
    // if (fd < 0 || fd >= NOFILE || (f = thiscpu()->proc->ofile[fd]) == 0)
    //     return -1;
    // if (pfd)
    //     *pfd = fd;
    // if (pf)
    //     *pf = f;
    // return 0;
}

/*
 * Allocate a file descriptor for the given file.
 * Takes over file reference from caller on success.
 */
static int fdalloc(struct file *f) {
    /* TODO: Lab9 Shell */
    PKPROCESS cur = PsGetCurrentProcess();
    for (int i = 0; i < 16; i++)
    {
        if (cur->FileDescriptors[i] == 0)
        {
            cur->FileDescriptors[i] = filei(f);
            return i;
        }
    }
    return -1;
}

/* 
 * Get the parameters and call filedup.
 */
int sys_dup(int fd) {
    /* TODO: Lab9 Shell. */
    struct file* f = KiValidateFileDescriptor(fd);
    if (f)
    {
        int fd = fdalloc(f);
        if (fd == -1)
            return -1;
        filedup(f);
        return fd;
    }
    return -1;
}

/* 
 * Get the parameters and call fileread.
 */
isize sys_read(int fd, void* buf, int count) {
    /* TODO: Lab9 Shell */
    struct file* f = KiValidateFileDescriptor(fd);
    if (f == NULL || !KiValidateBuffer(buf, count))
        return -1;
    return fileread(f, buf, count);
}

/* 
 * Get the parameters and call filewrite.
 */
isize sys_write(int fd, void* buf, int count) {
    /* TODO: Lab9 Shell */
    struct file* f = KiValidateFileDescriptor(fd);
    if (f == NULL || !KiValidateBuffer(buf, count))
        return -1;
    return filewrite(f, buf, count);
    return -1;
}

isize sys_writev(int fd, void* _iov, int iovcnt) {
    /* TODO: Lab9 Shell */

    struct file *f = KiValidateFileDescriptor(fd);
    struct iovec *iov = (struct iovec*)_iov, *p;
    // if (argfd(0, &fd, &f) < 0 ||
    //     argint(2, &iovcnt) < 0 ||
    //     argptr(1, &iov, iovcnt * sizeof(struct iovec)) < 0) {
    //     return -1;
    // }
    if (iovcnt < 0 || iovcnt >= 0x1000000)
        return -1;
    if (f == NULL)
        return -1;
    if (!KiValidateBuffer(iov, sizeof(struct iovec) * iovcnt))
        return -1;
    
    usize tot = 0;
    for (p = iov; p < iov + iovcnt; p++) {
        if (!KiValidateBuffer(p->iov_base, p->iov_len))
            return -1;
        tot += filewrite(f, p->iov_base, p->iov_len);
    }
    return tot;
}

/* 
 * Get the parameters and call fileclose.
 * Clear this fd of this process.
 */
int sys_close(int fd) {
    /* TODO: Lab9 Shell */
    struct file *f = KiValidateFileDescriptor(fd);
    if (f == NULL)
        return -1;
    PsGetCurrentProcess()->FileDescriptors[fd] = 0;
    fileclose(f);
    return 0;
}

/* 
 * Get the parameters and call filestat.
 */
int sys_fstat(int fd, struct stat *st) {
    /* TODO: Lab9 Shell */
    struct file* f = KiValidateFileDescriptor(fd);
    if (f == NULL || !KiValidateBuffer(st, sizeof(struct stat)))
        return -1;
    filestat(f, st);
    return 0;
}

int sys_fstatat(PTRAP_FRAME tf) {
    // i32 dirfd, flags;
    char *path = (char*)tf->x1;
    struct stat *st = (struct stat*)tf->x2;

    int len = KiValidateString(path);
    if (len == 0 || len > 1024)
        return -1;
    if (!KiValidateBuffer(st, sizeof(struct stat)))
        return -1;

    // if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0 || argptr(2, (void *)&st, sizeof(*st)) < 0 ||
    //     argint(3, &flags) < 0)
    //     return -1;

    if (tf->x0 != AT_FDCWD) {
        printf("sys_fstatat: dirfd unimplemented\n");
        return -1;
    }
    if (tf->x3 != 0) {
        printf("sys_fstatat: flags unimplemented\n");
        return -1;
    }

    Inode *ip;
    static OpContext ctx;
    bcache.begin_op(&ctx);
    if ((ip = namei(path, &ctx)) == 0) {
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.lock(ip);
    stati(ip, st);
    inodes.unlock(ip);
    inodes.put(&ctx, ip);
    bcache.end_op(&ctx);

    return 0;
}

/* 
 * Create an inode.
 *
 * Example:
 * Path is "/foo/bar/bar1", type is normal file.
 * You should get the inode of "/foo/bar", and
 * create an inode named "bar1" in this directory.
 * 
 * If type is directory, you should additionally handle "." and "..".
 */
Inode *create(char *path, short type, short major, short minor, OpContext *ctx) {
    /* TODO: Lab9 Shell */
    char name[FILE_NAME_MAX_LENGTH] = {0};
    Inode* ip = nameiparent(path, name, ctx);
    if (ip == NULL)
        return NULL;
    Inode* id = inodes.get(inodes.alloc(ctx, type));
    inodes.lock(ip);
    ip->entry.num_links++;
    inodes.insert(ctx, ip, name, id->inode_no);
    inodes.sync(ctx, ip, 1);
    inodes.unlock(ip);
    inodes.put(ip);
    inodes.lock(id);
    id->entry.num_links = 1;
    id->entry.major = major;
    id->entry.minor = minor;
    if (type == INODE_DIRECTORY)
    {
        id->entry.num_links = 2;
        inodes.insert(ctx, id, ".", id->inode_no);
        inodes.insert(ctx, id, "..", ip->inode_no);
    }
    inodes.sync(ctx, id, 1);
    return id;
}

int sys_openat(int dirfd, char* path, int omode) {
    int fd;
    struct file *f;
    Inode *ip;

    int len = KiValidateString(path);
    if (len == 0 || len > 1024)
        return -1;

    // if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0 || argint(2, &omode) < 0)
    //     return -1;

    // printf("%d, %s, %lld\n", dirfd, path, omode);
    if (dirfd != AT_FDCWD) {
        printf("sys_openat: dirfd unimplemented\n");
        return -1;
    }
    // if ((omode & O_LARGEFILE) == 0) {
    //     printf("sys_openat: expect O_LARGEFILE in open flags\n");
    //     return -1;
    // }

    static OpContext ctx;
    bcache.begin_op(&ctx);
    if (omode & O_CREAT) {
        // FIXME: Support acl mode.
        ip = create(path, INODE_REGULAR, 0, 0, &ctx);
        if (ip == 0) {
            bcache.end_op(&ctx);
            return -1;
        }
    } else {
        if ((ip = namei(path, &ctx)) == 0) {
            bcache.end_op(&ctx);
            return -1;
        }
        inodes.lock(ip);
        // if (ip->entry.type == INODE_DIRECTORY && omode != (O_RDONLY | O_LARGEFILE)) {
        //     inodes.unlock(ip);
        //     inodes.put(&ctx, ip);
        //     bcache.end_op(&ctx);
        //     return -1;
        // }
    }

    if ((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0) {
        if (f)
            fileclose(f);
        inodes.unlock(ip);
        inodes.put(&ctx, ip);
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.unlock(ip);
    bcache.end_op(&ctx);

    f->type = FD_INODE;
    f->ip = ip;
    f->off = 0;
    f->readable = !(omode & O_WRONLY);
    f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
    return fd;
}

int sys_mkdirat(int dirfd, char* path, int mode) {
    Inode *ip;
    int len = KiValidateString(path);
    if (len == 0 || len > 1024)
        return -1;

    // if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0 || argint(2, &mode) < 0)
    //     return -1;
    if (dirfd != AT_FDCWD) {
        printf("sys_mkdirat: dirfd unimplemented\n");
        return -1;
    }
    if (mode != 0) {
        printf("sys_mkdirat: mode unimplemented\n");
        return -1;
    }
    static OpContext ctx;
    bcache.begin_op(&ctx);
    if ((ip = create(path, INODE_DIRECTORY, 0, 0, &ctx)) == 0) {
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.unlock(ip);
    inodes.put(&ctx, ip);
    bcache.end_op(&ctx);
    return 0;
}

int sys_mknodat(PTRAP_FRAME tf) {
    Inode *ip;
    char *path = (char*)tf->x1;
    int len = KiValidateString(path);
    if (len == 0 || len > 1024)
        return -1;
    i32 dirfd = tf->x0, major = tf->x2, minor = tf->x3;

    // if (argint(0, &dirfd) < 0 || argstr(1, &path) < 0 || argint(2, &major) < 0 || argint(3, &minor))
    //     return -1;

    if (dirfd != AT_FDCWD) {
        printf("sys_mknodat: dirfd unimplemented\n");
        return -1;
    }
    printf("mknodat: path '%s', major:minor %d:%d\n", path, major, minor);

    static OpContext ctx;
    bcache.begin_op(&ctx);
    if ((ip = create(path, INODE_DEVICE, major, minor, &ctx)) == 0) {
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.unlock(ip);
    inodes.put(&ctx, ip);
    bcache.end_op(&ctx);
    return 0;
}

int sys_chdir(char* path) {
    Inode *ip;
    unsigned len = KiValidateString(path);
    if (len == 0 || len > 1024)
        return -1;
    PKPROCESS cur = PsGetCurrentProcess();
    static OpContext ctx;
    bcache.begin_op(&ctx);
    if ((ip = namei(path, &ctx)) == 0) {
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.lock(ip);
    if (ip->entry.type != INODE_DIRECTORY) {
        inodes.unlock(ip);
        inodes.put(&ctx, ip);
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.unlock(ip);
    if (cur->Cwd)
        inodes.put(&ctx, cur->Cwd);
    bcache.end_op(&ctx);
    cur->Cwd = ip;
    return 0;
}
int execve(PTRAP_FRAME, const char *path, char *const argv[], char *const envp[]);

/* 
 * Get the parameters and call execve.
 */
int sys_exec(PTRAP_FRAME tf) {
    /* TODO: Lab9 Shell */
    int len = KiValidateString((PVOID)tf->x0);
    if (len == 0 || len >= 1024)
        return -1;
    return execve(tf, (const char *)tf->x0, (char *const*)tf->x1, (char *const*)tf->x2);
}