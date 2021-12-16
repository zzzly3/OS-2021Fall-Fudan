#ifndef UPDATE_API
#define USE_LAGACY
#endif
#include <common/string.h>
#include <fs/inode.h>
#include <core/arena.h>
#include <common/lutil.h>
#include <mod/scheduler.h>
#include <core/console.h>
// TODO: Replace API after satisfying TAs.
#ifdef UPDATE_API
#include <ob/mem.h>
#include <mod/bug.h>
#else
#include <core/physical_memory.h>
#endif

// this lock mainly prevents concurrent access to inode list `head`, reference
// count increment and decrement.
static SpinLock lock;
static ListNode head;

static const SuperBlock *sblock;
static const BlockCache *cache;
#ifdef UPDATE_API
    static OBJECT_POOL InodePool;
#else
    static Arena arena;
#endif

// return which block `inode_no` lives on.
static INLINE usize to_block_no(usize inode_no) {
    return sblock->inode_start + (inode_no / (INODE_PER_BLOCK));
}

// return the pointer to on-disk inode.
static INLINE InodeEntry *get_entry(Block *block, usize inode_no) {
    return ((InodeEntry *)block->data) + (inode_no % INODE_PER_BLOCK);
}

// return address array in indirect block.
static INLINE u32 *get_addrs(Block *block) {
    return ((IndirectBlock *)block->data)->addrs;
}

// initialize inode tree.
void init_inodes(const SuperBlock *_sblock, const BlockCache *_cache) {
    #ifdef UPDATE_API
        MmInitializeObjectPool(&InodePool, sizeof(Inode));
    #else
        ArenaPageAllocator allocator = {.allocate = kalloc, .free = kfree};
        init_arena(&arena, sizeof(Inode), allocator);
    #endif
    init_spinlock(&lock);
    init_list_node(&head);
    sblock = _sblock;
    cache = _cache;
    inodes.root = inodes.get(ROOT_INODE_NO);
}

// initialize in-memory inode.
static void init_inode(Inode *inode) {
    init_spinlock(&inode->lock);
    init_rc(&inode->rc);
    init_list_node(&inode->node);
    inode->inode_no = 0;
    inode->valid = false;
}

// see `inode.h`.
static usize inode_alloc(OpContext *ctx, InodeType type) {
    assert(type != INODE_INVALID);
    usize step = 0, count = 0;
    for (usize i = 1; i < sblock->num_inodes; i++)
    {
        usize bno = to_block_no(i);
        Block* b = cache->acquire(bno);
        InodeEntry* ie = get_entry(b, i);
        if (ie->type == INODE_INVALID)
        {
            memset(ie, 0, sizeof(InodeEntry));
            ie->type = type;
            cache->sync(ctx, b);
            cache->release(b);
            //printf("???%d\n", ie->addrs[0]);
            return i;
        }
        cache->release(b);
    }
    PANIC("failed to allocate inode on disk");
}

// see `inode.h`.
static void inode_lock(Inode *inode) {
    assert(inode->rc.count > 0);
    acquire_spinlock(&inode->lock);
}

// see `inode.h`.
static void inode_unlock(Inode *inode) {
    assert(holding_spinlock(&inode->lock));
    assert(inode->rc.count > 0);
    release_spinlock(&inode->lock);
}

// see `inode.h`.
static void inode_sync(OpContext *ctx, Inode *inode, bool do_write) {
    if (inode->valid)
    {
        // store
        if (do_write)
        {
            Block* b = cache->acquire(to_block_no(inode->inode_no));
            memcpy(get_entry(b, inode->inode_no), &inode->entry, sizeof(InodeEntry));
            cache->sync(ctx, b);
            cache->release(b);
        }
    }
    else
    {
        // load
        Block* b = cache->acquire(to_block_no(inode->inode_no));
        memcpy(&inode->entry, get_entry(b, inode->inode_no), sizeof(InodeEntry));
        cache->release(b);
        inode->valid = true;
    }
}

// see `inode.h`.
static Inode *inode_get(usize inode_no) {
    assert(inode_no > 0);
    assert(inode_no < sblock->num_inodes);
    Inode* q = NULL;
    acquire_spinlock(&lock);
    for (ListNode* p = head.next; p != &head; p = p->next)
    {
        if (container_of(p, Inode, node)->inode_no == inode_no)
        {
            increment_rc(&(q = container_of(p, Inode, node))->rc);
            break;
        }
    }
    release_spinlock(&lock);
    if (q != NULL)
        return q;
    #ifdef UPDATE_API
        Inode* in = MmAllocateObject(&InodePool);
        ASSERT(in != NULL, BUG_FSFAULT);
    #else
        Inode* in = alloc_object(&arena);
    #endif
    init_inode(in);
    in->inode_no = inode_no;
    inode_sync(NULL, in, false);
    //printf("!!!%d\n", in->entry.addrs[0]);
    increment_rc(&in->rc);
    acquire_spinlock(&lock);
    merge_list(&head, &in->node);
    release_spinlock(&lock);
    return in;
}

// see `inode.h`.
static void inode_clear(OpContext *ctx, Inode *inode) {
    InodeEntry *entry = &inode->entry;
    //printf("clearrrrr\n");
    for (int i = 0; i < INODE_NUM_DIRECT; i++)
    {
        //printf("%d %d\n", i, entry->addrs[i]);
        if (entry->addrs[i] > 0)
        {
            cache->free(ctx, entry->addrs[i]);
            entry->addrs[i] = 0;
        }
    }
    //printf("%d \n", entry->indirect);
    if (entry->indirect > 0)
    {
        Block* b = cache->acquire(entry->indirect);
        for (int i = 0; i < INODE_NUM_INDIRECT; i++)
        {
            if (get_addrs(b)[i] > 0)
            {
                cache->free(ctx, get_addrs(b)[i]);
                get_addrs(b)[i] = 0;
            }
        }
        cache->release(b);
        cache->free(ctx, entry->indirect);
        entry->indirect = 0;
    }
    entry->num_bytes = 0;
    inode_sync(ctx, inode, true);
}

// see `inode.h`.
static Inode *inode_share(Inode *inode) {
    acquire_spinlock(&lock);
    increment_rc(&inode->rc);
    release_spinlock(&lock);
    return inode;
}

// see `inode.h`.
static void inode_put(OpContext *ctx, Inode *inode) {
    //printf("putttt\n");
    assert(inode->valid);
    acquire_spinlock(&lock);
    if (decrement_rc(&inode->rc))
    {
        //printf("freeeeee %d\n", inode->entry.num_links);
        detach_from_list(&inode->node);
        release_spinlock(&lock);
        if (inode->entry.num_links == 0)
        {
            inode_clear(ctx, inode);
            inode->entry.type = INODE_INVALID;
            inode_sync(ctx, inode, true);
        }
        //printf("clearrrrrr\n");
        #ifdef UPDATE_API
            MmFreeObject(&InodePool, inode);
        #else
            free_object(inode);
        #endif
    }
    else
        release_spinlock(&lock);
}

// this function is private to inode layer, because it can allocate block
// at arbitrary offset, which breaks the usual file abstraction.
//
// retrieve the block in `inode` where offset lives. If the block is not
// allocated, `inode_map` will allocate a new block and update `inode`, at
// which time, `*modified` will be set to true.
// the block number is returned.
//
// NOTE: caller must hold the lock of `inode`.
static usize inode_map(OpContext *ctx, Inode *inode, usize offset, bool *modified) {
    assert(offset < INODE_MAX_BYTES);
    InodeEntry *entry = &inode->entry;
    int bi = offset / BLOCK_SIZE;
    *modified = false;
    #define get(t) if (t == 0 && ctx != NULL) {usize a = cache->alloc(ctx); assert(a > 0); t = a; *modified = true;}
    if (bi < INODE_NUM_DIRECT)
    {
        // direct
        get(inode->entry.addrs[bi])
        return inode->entry.addrs[bi];
    }
    else
    {
        // indirect
        if (ctx == NULL && inode->entry.indirect == 0)
            return 0;
        get(inode->entry.indirect)
        Block* b = cache->acquire(inode->entry.indirect);
        get(get_addrs(b)[bi - INODE_NUM_DIRECT])
        usize r = get_addrs(b)[bi - INODE_NUM_DIRECT];
        cache->release(b);
        return r;
    }
    #undef get
    return 0;
}

// see `inode.h`.
static void inode_read(Inode *inode, u8 *dest, usize offset, usize count) {
    InodeEntry *entry = &inode->entry;
    usize end = offset + count;
    assert(offset <= entry->num_bytes);
    assert(end <= entry->num_bytes);
    assert(offset <= end);
    for (usize o = offset; o < end; o++, dest++)
    {
        bool x;
        usize bn = inode_map(NULL, inode, o, &x);
        //printf("%d in %d\n", o, bn);
        assert(x == false);
        if (bn == 0)
            *dest = 0;
        else
        {
            Block* b = cache->acquire(bn);
            *dest = ((u8*)b)[o % BLOCK_SIZE];
            cache->release(b);
        }
    }
}

// see `inode.h`.
static void inode_write(OpContext *ctx, Inode *inode, u8 *src, usize offset, usize count) {
    InodeEntry *entry = &inode->entry;
    usize end = offset + count;
    assert(offset <= entry->num_bytes);
    assert(end <= INODE_MAX_BYTES);
    assert(offset <= end);
    for (usize o = offset; o < end; o++, src++)
    {
        bool x;
        usize bn = inode_map(ctx, inode, o, &x);
        assert(bn > 0);
        Block* b = cache->acquire(bn);
        ((u8*)b)[o % BLOCK_SIZE] = *src;
        cache->release(b);
    }
    entry->num_bytes = MAX(entry->num_bytes, end);
    inode_sync(ctx, inode, true);
}

// see `inode.h`.
static usize inode_lookup(Inode *inode, const char *name, usize *index) {
    InodeEntry *entry = &inode->entry;
    assert(entry->type == INODE_DIRECTORY);
    DirEntry de;
    usize o;
    usize i;
    if (index == NULL)
        index = &i;
    for (o = 0; o < entry->num_bytes; o += sizeof(DirEntry))
    {
        // printf("l %d\n", o);
        inode_read(inode, (u8*)&de, o, sizeof(DirEntry));
        if (name == NULL)
        {
            if (de.inode_no == 0)
            {
                *index = o;
                return 0;
            }
        }
        else if (strncmp(de.name, name, FILE_NAME_MAX_LENGTH) == 0)
        {
            *index = o / sizeof(DirEntry);
            return de.inode_no;
        }
    }
    // printf("l %d\n", o);
    if (name == NULL)
        *index = o;
    return 0;
}

// see `inode.h`.
static usize inode_insert(OpContext *ctx, Inode *inode, const char *name, usize inode_no) {
    InodeEntry *entry = &inode->entry;
    assert(entry->type == INODE_DIRECTORY);
    DirEntry de;
    de.inode_no = inode_no;
    strncpy(de.name, name, FILE_NAME_MAX_LENGTH);
    usize i;
    //printf("i %s = %d\n", name, inode_no);
    inode_lookup(inode, NULL, &i);
    //printf("i to %d\n", i);
    inode_write(ctx, inode, (u8*)&de, i, sizeof(DirEntry));
    return 0;
}

// see `inode.h`.
static void inode_remove(OpContext *ctx, Inode *inode, usize index) {
    InodeEntry *entry = &inode->entry;
    assert(entry->type == INODE_DIRECTORY);
    DirEntry de;
    memset(&de, 0, sizeof(DirEntry));
    inode_write(ctx, inode, (u8*)&de, index * sizeof(DirEntry), sizeof(DirEntry));
}

InodeTree inodes = {
    .alloc = inode_alloc,
    .lock = inode_lock,
    .unlock = inode_unlock,
    .sync = inode_sync,
    .get = inode_get,
    .clear = inode_clear,
    .share = inode_share,
    .put = inode_put,
    .read = inode_read,
    .write = inode_write,
    .lookup = inode_lookup,
    .insert = inode_insert,
    .remove = inode_remove,
};
