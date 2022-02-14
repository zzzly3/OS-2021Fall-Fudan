#include <elf.h>

#include "trap.h"

#include <fs/file.h>
#include <fs/inode.h>

#include <aarch64/mmu.h>
#include <core/console.h>
#include <core/proc.h>
#include <core/sched.h>
#include <core/virtual_memory.h>

#include <elf.h>
#include <mod/syscall.h>

static uint64_t auxv[][2] = {{AT_PAGESZ, PAGE_SIZE}};

/* 
 * Step1: Load data from the file stored in `path`.
 * The first `sizeof(struct Elf64_Ehdr)` bytes is the ELF header part.
 * You should check the ELF magic number and get the `e_phoff` and `e_phnum`
 * which is the starting byte of program header.
 * 
 * Step2: Load program headers
 * Program headers are stored like:
 * struct Elf64_Phdr phdr[e_phnum];
 * e_phoff is the address of phdr[0].
 * For each program header, if the type is LOAD, you should:
 * (1) allocate memory, va region [vaddr, vaddr+memsz)
 * (2) copy [offset, offset + filesz) of file to va [vaddr, vaddr+filesz) of memory
 * 
 * Step3: Allocate and initialize user stack.
 * 
 * The va of the user stack is not required to be any fixed value. It can be randomized.
 * 
 * Push argument strings.
 *
 * The initial stack is like
 *
 *   +-------------+
 *   | auxv[o] = 0 | 
 *   +-------------+
 *   |    ....     |
 *   +-------------+
 *   |   auxv[0]   |
 *   +-------------+
 *   | envp[m] = 0 |
 *   +-------------+
 *   |    ....     |
 *   +-------------+
 *   |   envp[0]   |
 *   +-------------+
 *   | argv[n] = 0 |  n == argc
 *   +-------------+
 *   |    ....     |
 *   +-------------+
 *   |   argv[0]   |
 *   +-------------+
 *   |    argc     |
 *   +-------------+  <== sp
 *
 * where argv[i], envp[j] are 8-byte pointers and auxv[k] are
 * called auxiliary vectors, which are used to transfer certain
 * kernel level information to the user processes.
 *
 * ## Example 
 *
 * ```
 * sp -= 8; *(size_t *)sp = AT_NULL;
 * sp -= 8; *(size_t *)sp = PGSIZE;
 * sp -= 8; *(size_t *)sp = AT_PAGESZ;
 *
 * sp -= 8; *(size_t *)sp = 0;
 *
 * // envp here. Ignore it if your don't want to implement envp.
 *
 * sp -= 8; *(size_t *)sp = 0;
 *
 * // argv here.
 *
 * sp -= 8; *(size_t *)sp = argc;
 *
 * // Stack pointer must be aligned to 16B!
 *
 * thisproc()->tf->sp = sp;
 * ```
 * 
 * There are two important entry point addresses:
 * (1) Address of the first user-level instruction: that's stored in elf_header.entry
 * (2) Adresss of the main function: that's stored in memory (loaded in part2)
 *
 */
Inode *namex(const char *path, int nameiparent, char *name, OpContext *ctx);
int execve(PTRAP_FRAME tf, const char *path, char *const argv[], char *const envp[]) {
	/* TODO: Lab9 Shell */
    int ret = -1;
    // copy string
    static char str[2048];
    short _argv[16] = {0}, _envp[16] = {0}, pstr = 0, argc = 0, envc = 0;
    memset(str, 0, 2048);
    for (int i = 0; i < 16; i++)
    {
        if (!MmProbeRead(&argv[i]))
            return -1;
        if (argv[i] == NULL || i == 15)
        {
            _argv[i] = -1;
            argc = i;
            break;
        }
        int len = KiValidateString(argv[i]);
        if (len == 0 || pstr + len >= 2048)
            return -1;
        memcpy(&str[pstr], argv[i], len);
        _argv[i] = pstr;
        pstr += len;
    }
    for (int i = 0; i < 16; i++)
    {
        if (!MmProbeRead(&envp[i]))
            return -1;
        if (envp[i] == NULL || i == 15)
        {
            _envp[i] = -1;
            envc = i;
            break;
        }
        int len = KiValidateString(envp[i]);
        if (len == 0 || pstr + len >= 2048)
            return -1;
        memcpy(&str[pstr], envp[i], len);
        _envp[i] = pstr;
        pstr += len;
    }
    // read ehdr
    char name[16] = {0};
    static OpContext ctx;
    static Elf64_Ehdr ehdr;
    BOOL te = arch_disable_trap();
    bcache.begin_op(&ctx);
	Inode* ip = namex(path, 0, name, &ctx);
    if (ip == NULL)
        goto end1;
    inodes.lock(ip);
    inodes.read(ip, (u8*)&ehdr, 0, sizeof(ehdr));
    // check magic
    if (ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr.e_ident[EI_MAG3] != ELFMAG3 ||
        ehdr.e_phoff >= 0x4000000)
        goto end2;
    // build new mem
    PMEMORY_SPACE mem = MmCreateMemorySpace();
    if (mem == NULL)
        goto end2;
    PMEMORY_SPACE old_mem = KeSwitchMemorySpace(mem);
    static Elf64_Phdr phdr;
    static char buffer[65536];
    for (int p = ehdr.e_phoff, i = 0; p < 0x4000000 && i < ehdr.e_phnum; p += sizeof(phdr), i += 1)
    {
        inodes.read(ip, (u8*)&phdr, p, sizeof(phdr));
        if (phdr.p_type == PT_LOAD)
        {
            // load section
            if (phdr.p_offset >= 0x4000000 ||
                phdr.p_filesz >= 65536 ||
                phdr.p_vaddr >= USPACE_TOP - 0x8000000 ||
                phdr.p_memsz >= 0x4000000 ||
                phdr.p_memsz < phdr.p_filesz)
                goto end3;
            if (phdr.p_memsz == 0)
                continue;
            inodes.read(ip, buffer, phdr.p_offset, phdr.p_filesz);
            if (!KSUCCESS(MmCreateUserPagesEx(mem, (PVOID)phdr.p_vaddr, phdr.p_memsz, TRUE)))
                goto end3;
            memcpy((PVOID)phdr.p_vaddr, buffer, phdr.p_filesz);
        }
    }
    // setup stack
    ULONG64 sp = USPACE_TOP - 0x4000000 + PAGE_SIZE;
    if (!KSUCCESS(MmCreateUserPageEx(mem, (PVOID)sp)))
        goto end3;
    sp += PAGE_SIZE;
    sp -= pstr;
    memcpy((PVOID)sp, str, pstr);
    ULONG64 str_base = sp;
    sp &= ~7ull; // 8-aligned
    // auxv * 3
    // envp * envc+1
    // argv * argc+1
    // argc * 1
    if (((sp >> 3) - (envc + argc)) & 1)
        sp -= 8; // 16-aligned
    #define PUSH(x) sp -= 8; *(ULONG64*)sp = (x);
    PUSH(AT_NULL)
    PUSH(PAGE_SIZE)
    PUSH(AT_PAGESZ)
    PUSH(0)
    for (int i = envc - 1; i >= 0; i--)
    {
        PUSH(str_base + _envp[i])
    }
    PUSH(0)
    for (int i = argc - 1; i >= 0; i--)
    {
        PUSH(str_base + _argv[i])
    }
    PUSH(argc)
    #undef PUSH
    // update process
    PKPROCESS cur = PsGetCurrentProcess();
    cur->UserDataBegin = USPACE_TOP - 0x4000000 + PAGE_SIZE;
    cur->UserDataEnd = USPACE_TOP - 0x4000000 + PAGE_SIZE * 2;
    arch_set_usp(sp);
    strncpy(cur->DebugName, name, 16);
    tf->elr = ehdr.e_entry;
    MmDestroyMemorySpace(old_mem);
    goto end2;
end3:
    KeSwitchMemorySpace(old_mem);
    MmDestroyMemorySpace(mem);
end2:
    inodes.unlock(ip);
    inodes.put(&ctx, ip);
end1:
    bcache.end_op(&ctx);
    if (te) arch_enable_trap();
    return ret;
}
