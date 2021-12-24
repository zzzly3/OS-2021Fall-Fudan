#ifndef UPDATE_API
#define USE_LAGACY
#endif
#include <common/bitmap.h>
#include <common/string.h>
#include <core/arena.h>
#include <core/console.h>
#include <core/physical_memory.h>
#include <core/proc.h>
#include <fs/cache.h>
#include <ob/mem.h>
#include <ob/mutex.h>
#include <aarch64/intrinsic.h>

static const SuperBlock *sblock;
static const BlockDevice *device;

static int cache_cnt;
static SpinLock lock;     // protects block cache.
static Arena arena;       // memory pool for `Block` struct.
static ListNode head;     // the list of all allocated in-memory block.
static LogHeader header;  // in-memory copy of log header block.
static OpContext *current;
static int ctx_cnt;
#ifdef UPDATE_API
    static OBJECT_POOL BlockPool;
    static MUTEX atomic_lock;
#else
    static SleepLock atomic_lock;
#endif

// hint: you may need some other variables. Just add them here.

// read the content from disk.
static INLINE void device_read(Block *block) {
    device->read(block->block_no, block->data);
}

// write the content back to disk.
static INLINE void device_write(Block *block) {
    device->write(block->block_no, block->data);
}

// read log header from disk.
static INLINE void read_header() {
    device->read(sblock->log_start, (u8 *)&header);
}

// write log header back to disk.
static INLINE void write_header() {
    device->write(sblock->log_start, (u8 *)&header);
}

// initialize block cache.
void init_bcache(const SuperBlock *_sblock, const BlockDevice *_device) {
    sblock = _sblock;
    device = _device;

    // TODO
    #ifdef UPDATE_API
        BOOL te = arch_disable_trap();
        MmInitializeObjectPool(&BlockPool, sizeof(Block));
        if (te) arch_enable_trap();
    #else
        ArenaPageAllocator allocator = {.allocate = kalloc, .free = kfree};
        init_arena(&arena, sizeof(Block), allocator);
    #endif
    read_header();
    init_spinlock(&lock);
    init_list_node(&head);
    #ifdef UPDATE_API
        KeInitializeMutex(&atomic_lock);
    #else
        init_sleeplock(&atomic_lock, "ctx");
    #endif
    //     init_sleeplock(&ctx_lock, "ctx");
    for (int i = 0; i < header.num_blocks; i++)
    {
        Block b;
        b.block_no = sblock->log_start + i + 1;
        device_read(&b);
        b.block_no = header.block_no[i];
        device_write(&b);
    }
    header.num_blocks = 0;
    write_header();
}

// initialize a block struct.
static void init_block(Block *block) {
    block->block_no = 0;
    init_list_node(&block->node);
    block->acquired = false;
    block->pinned = false;

    init_sleeplock(&block->lock, "block");
    block->valid = false;
    memset(block->data, 0, sizeof(block->data));
}

// see `cache.h`.
static usize get_num_cached_blocks() {
    // TODO
    return cache_cnt;
}

// see `cache.h`.
USR_ONLY
static Block *cache_acquire(usize block_no) {
    // TODO
begin:
    acquire_spinlock(&lock);
    ListNode* p = head.next;
    Block* b = NULL;
    while (p != &head)
    {
        b = container_of(p, Block, node);
        if (b->block_no == block_no)
        {
            if (b->acquired)
            {
                release_spinlock(&lock);
                goto begin;
            }
            detach_from_list(p);
            merge_list(&head, p);
            cache_cnt--;
            goto acquire;
        }
        p = p->next;
    }
    #ifdef UPDATE_API
        BOOL te = arch_disable_trap();
        b = (Block*) MmAllocateObject(&BlockPool);
        if (te) arch_enable_trap();
    #else
        b = (Block*) alloc_object(&arena);
    #endif
    init_block(b);
    b->block_no = block_no;
    device_read(b);
    b->valid = true;
    merge_list(&head, &b->node);
acquire:
    b->acquired = true;
    #ifndef UPDATE_API
        acquire_sleeplock(&b->lock);
    #endif
    release_spinlock(&lock);
    ctx_cnt++;
    return b;
}

// see `cache.h`.
USR_ONLY
static void cache_release(Block *block) {
    // TODO
    #ifndef UPDATE_API
        release_sleeplock(&block->lock);
    #endif
begin:
    acquire_spinlock(&lock);
    if (cache_cnt >= EVICTION_THRESHOLD)
    {
        ListNode* p = head.prev;
        while (p != &head)
        {
            Block* b = container_of(p, Block, node);
            if (!b->pinned && !b->acquired)
            {
                detach_from_list(&b->node);
                #ifdef UPDATE_API
                    BOOL te = arch_disable_trap();
                    MmFreeObject(&BlockPool, b);
                    if (te) arch_enable_trap();
                #else
                    free_object(b);
                #endif
                cache_cnt--;
                break;
            }
            p = p->prev;
        }
    }
    if (cache_cnt >= EVICTION_THRESHOLD)
    {
        if (!block->pinned)
        {
            block->acquired = false;
            detach_from_list(&block->node);
            #ifdef UPDATE_API
                BOOL te = arch_disable_trap();
                MmFreeObject(&BlockPool, block);
                if (te) arch_enable_trap();
            #else
                free_object(block);
            #endif
        }
        else
        {
            release_spinlock(&lock);
            goto begin;
        }
    }
    else
    {
        block->acquired = false;
        cache_cnt++;
    }
    release_spinlock(&lock);
    if (block->pinned && ctx_cnt == 3)
        cache_end_op(current);
}

// see `cache.h`.
USR_ONLY
static void cache_begin_op(OpContext *ctx) {
    // TODO
    static int funny = 0;
    #ifdef UPDATE_API
        KeUserWaitForMutexSignaled(&atomic_lock, FALSE);
    #else
        acquire_sleeplock(&atomic_lock);
    #endif
    ctx->ts = ++funny;
    ctx->log.num_blocks = 0;
    ctx_cnt = 0;
    current = ctx;
    init_spinlock(&ctx->lock);
    release_spinlock(&lock);
}

// see `cache.h`.
static void cache_sync(OpContext *ctx, Block *block) {
    if (ctx) {
        // TODO
        acquire_spinlock(&ctx->lock);
        for (int i = 0; i < ctx->log.num_blocks; i++)
        {
            if (ctx->log.block_no[i] == block->block_no)
                goto ret;
        }
        acquire_spinlock(&lock);
        block->pinned = true;
        release_spinlock(&lock);
        if (ctx->log.num_blocks >= OP_MAX_NUM_BLOCKS)
            PANIC("Log limit exceeded (LLE).");
        ctx->log.block_no[ctx->log.num_blocks++] = block->block_no;
    ret:
        release_spinlock(&ctx->lock);
    } else
        device_write(block);
}

// see `cache.h`.
USR_ONLY
static void cache_end_op(OpContext *ctx) {
    // TODO
    if (ctx->ts == 0)
        return;
    acquire_spinlock(&ctx->lock);
    Block* b[OP_MAX_NUM_BLOCKS];
    for (int i = 0; i < ctx->log.num_blocks; i++)
        b[i] = cache_acquire(ctx->log.block_no[i]);
    acquire_spinlock(&lock);
    for (int i = 0; i < ctx->log.num_blocks; i++)
        device->write(sblock->log_start + i + 1, b[i]->data);
    memcpy(&header, &ctx->log, sizeof(LogHeader));
    write_header();
    for (int i = 0; i < header.num_blocks; i++)
    {
        device_write(b[i]);
        b[i]->pinned = false;
    }
    header.num_blocks = 0;
    write_header();
    release_spinlock(&lock);
    for (int i = 0; i < ctx->log.num_blocks; i++)
        cache_release(b[i]);
    ctx->ts = 0;
    release_spinlock(&ctx->lock);
    if (current == ctx)
    {
        current = NULL;
        #ifdef UPDATE_API
            KeSetMutexSignaled(&atomic_lock);
        #else
            release_sleeplock(&atomic_lock);
        #endif
    }
}

// see `cache.h`.
// hint: you can use `cache_acquire`/`cache_sync` to read/write blocks.
static usize cache_alloc(OpContext *ctx) {
    // TODO
    Block* b = cache_acquire(sblock->bitmap_start);
    int p = 0;
    for (int i = 0; i < BLOCK_SIZE; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            int mask = 1 << j;
            if ((b->data[i] & mask) == 0)
            {
                b->data[i] |= mask;
                p = i * 8 + j;
                goto alloc;
            }
        }
    }
panic:
    PANIC("cache_alloc: no free block");
alloc:
    if (p >= sblock->num_blocks)
        goto panic;
    cache_sync(ctx, b);
    cache_release(b);
    b = cache_acquire(p);
    memset(b->data, 0, BLOCK_SIZE);
    cache_sync(ctx, b);
    cache_release(b);
    return p;
}

// see `cache.h`.
// hint: you can use `cache_acquire`/`cache_sync` to read/write blocks.
static void cache_free(OpContext *ctx, usize block_no) {
    // TODO
    Block* b = cache_acquire(sblock->bitmap_start);
    b->data[block_no / 8] &= ~(1 << (block_no % 8));
    cache_sync(ctx, b);
    cache_release(b);
}

BlockCache bcache = {
    .get_num_cached_blocks = get_num_cached_blocks,
    .acquire = cache_acquire,
    .release = cache_release,
    .begin_op = cache_begin_op,
    .sync = cache_sync,
    .end_op = cache_end_op,
    .alloc = cache_alloc,
    .free = cache_free,
};
