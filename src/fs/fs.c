#include <fs/block_device.h>
#include <fs/cache.h>
#include <fs/defines.h>
#include <fs/fs.h>
#include <fs/inode.h>
#include <def.h>

void init_filesystem() {
    putstr("init_block_device\n");
    init_block_device();

    const SuperBlock *sblock = get_super_block();
    printf("SuperBlock: log_start=%d inode_start=%d bitmap_start=%d num_blocks=%d num_inodes=%d\n",
        sblock->log_start, sblock->inode_start, sblock->bitmap_start, sblock->num_blocks, sblock->num_inodes);
    putstr("init_bcache\n");
    init_bcache(sblock, &block_device);
    putstr("init_inodes\n");
    init_inodes(sblock, &bcache);
}
