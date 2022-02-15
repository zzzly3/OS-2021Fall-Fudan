/* File descriptors */

#include "file.h"
#include "fs.h"
#include <common/defines.h>
#include <common/spinlock.h>
#include <core/console.h>
#include <core/sleeplock.h>
#include <fs/inode.h>
#include <fs/cache.h>
#include <ob/mem.h>

// struct devsw devsw[NDEV];
struct {
    SPINLOCK lock;
    struct file file[NFILE];
} ftable;

struct file* ifile(char id) {
    if (id > 0)
        return &ftable.file[id];
    return 0;
}

char filei(struct file* f) {
    if (f)
        return ((ULONG64)f - (ULONG64)&ftable.file) / sizeof (struct file);
    return 0;
}

/* Optional since BSS is zero-initialized. */
void fileinit() {
    // init_spinlock(&ftable.lock);
    KeInitializeSpinLock(&ftable.lock);
}

/* Allocate a file structure. */
struct file *filealloc() {
    /* TODO: Lab9 Shell */
    KeAcquireSpinLock(&ftable.lock);
    for (int i = 1; i < NFILE; i++)
    {
        if (ftable.file[i].ref == 0)
        {
            memset(&ftable.file[i], 0, sizeof(struct file));
            ftable.file[i].ref = 1;
            KeReleaseSpinLock(&ftable.lock);
            return &ftable.file[i];
        }
    }
    KeReleaseSpinLock(&ftable.lock);
    return 0;
}

/* Increment ref count for file f. */
struct file *filedup(struct file *f) {
    /* TODO: Lab9 Shell */
    KeAcquireSpinLock(&ftable.lock);
    ASSERT(f->ref > 0, BUG_BADREF);
    f->ref++;
    KeReleaseSpinLock(&ftable.lock);
    return f;
}

static OpContext SingleOpCtx;

/* Close file f. (Decrement ref count, close when reaches 0.) */
void fileclose(struct file *f) {
    /* TODO: Lab9 Shell */
    BOOL te = arch_disable_trap();
    Inode* close = NULL;
    KeAcquireSpinLockFast(&ftable.lock);
    ASSERT(f->ref > 0, BUG_BADREF);
    if (--f->ref == 0)
    {
        close = f->ip;
    }
    KeReleaseSpinLockFast(&ftable.lock);
    // ASSERT(KeGetCurrentExecuteLevel() == EXECUTE_LEVEL_USR, BUG_BADLEVEL);
    if (close == NULL)
        goto end;
    bcache.begin_op(&SingleOpCtx); // maybe switched out
    inodes.put(&SingleOpCtx, close);
    bcache.end_op(&SingleOpCtx);
end:
    if (te) arch_enable_trap();
}

/* Get metadata about file f. */
int filestat(struct file *f, struct stat *st) {
    /* TODO: Lab9 Shell */
    ASSERT(f->ref > 0, BUG_BADREF);
    switch (f->type)
    {
        case FD_INODE:
            KeAcquireSpinLock(&f->ip->lock);
            stati(f->ip, st);
            KeReleaseSpinLock(&f->ip->lock);
            break;
    }
}

/* Read from file f. */
isize fileread(struct file *f, char *addr, isize n) {
    /* TODO: Lab9 Shell */
    ASSERT(f->ref > 0, BUG_BADREF);
    usize sz;
    switch (f->type)
    {
        case FD_INODE:
            KeAcquireSpinLock(&f->ip->lock);
            sz = inodes.read(f->ip, addr, f->off, n);
            f->off += sz;
            KeReleaseSpinLock(&f->ip->lock);
            break;
    }
    return sz;
}

/* Write to file f. */
isize filewrite(struct file *f, char *addr, isize n) {
    /* TODO: Lab9 Shell */
    ASSERT(f->ref > 0, BUG_BADREF);
    usize sz;
    switch (f->type)
    {
        case FD_INODE:
            KeAcquireSpinLock(&f->ip->lock);
            sz = n;
            bcache.begin_op(&SingleOpCtx); // maybe switched out
            inodes.write(&SingleOpCtx, f->ip, addr, f->off, sz);
            f->off += sz;
            bcache.end_op(&SingleOpCtx);
            KeReleaseSpinLock(&f->ip->lock);
            break;
    }
    return sz;
}
