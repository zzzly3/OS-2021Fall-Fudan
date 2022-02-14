#include <mod/syscall.h>
#include <fs/file.h>

extern BOOL KeBugFaultFlag;
extern u64 TrapContext[CPU_NUM][16];
u64 SyscallContext[CPU_NUM][16];

extern const char* syscall_table_str[NR_SYSCALL];

BOOL KiValidateBuffer(PVOID Address, unsigned Size)
{
	if (Size == 0)
		return TRUE;
	if (Size >= 0x7fffffff)
		return FALSE;
	if ((ULONG64)Address + Size >= USPACE_TOP)
		return FALSE;
	return MmProbeReadEx(Address, Size);
}

unsigned KiValidateString(PVOID Address) // return size with '\0'
{
	for (int i = 0; ; i++)
	{
		if (i >= 0x7fffffff)
			return 0;
		PVOID p = (PVOID)((ULONG64)Address + i);
		if ((ULONG64)p >= USPACE_TOP)
			return 0;
		if (!MmProbeRead(p))
			return 0;
		if (*(char*)p == 0)
			return i + 1;
	}
}

struct file* KiValidateFileDescriptor(int FileDescriptor) // return file object
{
	if (FileDescriptor < 0 || FileDescriptor >= 16)
		return NULL;
	return ifile(PsGetCurrentProcess()->FileDescriptors[FileDescriptor]);
}

void KiSystemCallEntry(PTRAP_FRAME TrapFrame)
{
	if (KeBugFaultFlag) for(arch_disable_trap();;);
	if (TrapFrame->elr >> 63) 
	{
		printf("Invoke syscall in kernel mode");
		KeBugFault(BUG_BADLEVEL);
	}
	memcpy(SyscallContext[cpuid()], TrapContext[cpuid()], 16*8);
	arch_enable_trap();
	ULONG64 callno = TrapFrame->x8;
	ULONG64 arg1 = TrapFrame->x0;
	ULONG64 arg2 = TrapFrame->x1;
	ULONG64 arg3 = TrapFrame->x2;
	ULONG64 ret = 0;
	if (callno < 400) printf("SYSCALL %d %s\n", callno, syscall_table_str[callno]);
	switch (callno)
	{
		case SYS_set_tid_address: ret = sys_gettid(); break;
		case SYS_ioctl: ret = sys_ioctl(); break;
		case SYS_gettid: ret = sys_gettid(); break;
		case SYS_rt_sigprocmask: ret = sys_sigprocmask(); break;
		case SYS_brk: ret = sys_brk(arg1); break;
		case SYS_execve: ret = sys_exec(TrapFrame); break;
		case SYS_sched_yield: ret = sys_yield(); break;
		case SYS_clone: ret = sys_clone(TrapFrame); break;
		case SYS_wait4: ret = sys_wait4(TrapFrame); break;
		case SYS_exit_group: ret = sys_exit(); break;
		case SYS_exit: ret = sys_exit(); break;
		case SYS_dup: ret = sys_dup(arg1); break;
		case SYS_chdir: ret = sys_chdir((char*)arg1); break;
		case SYS_fstat: ret = sys_fstat(arg1, (struct file*)arg2); break;
		case SYS_newfstatat: ret = sys_fstatat(TrapFrame); break;
		case SYS_mkdirat: ret = sys_mkdirat(arg1, (char*)arg2, arg3); break;
		case SYS_mknodat: ret = sys_mknodat(TrapFrame); break;
		case SYS_openat: ret = sys_openat(arg1, (char*)arg2, arg3); break;
		case SYS_writev: ret = sys_writev(arg1, (void*)arg2, arg3); break;
		case SYS_read: ret = sys_read(arg1, (void*)arg2, arg3); break;
		case SYS_write: ret = sys_write(arg1, (void*)arg2, arg3); break;
		case SYS_close: ret = sys_close(arg1); break;
		case SYS_myyield: ret = sys_yield(); break;
		default: ret = -1;
	}
	TrapFrame->x0 = ret;
	// Fault if EL mismatching
	KeRaiseExecuteLevel(EXECUTE_LEVEL_APC);
	KeLowerExecuteLevel(EXECUTE_LEVEL_USR);
}