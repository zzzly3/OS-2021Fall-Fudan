#ifndef __DEV_H__
#define __DEV_H__

#include <common/spinlock.h>
#include <common/string.h>
#include <common/lutil.h>
#include <common/rc.h>
#include <ob/mutex.h>
#include <ob/proc.h>
#include <driver/uart.h>
#include <driver/interrupt.h>

struct _MANAGER_OBJECT;
struct _IOREQ_OBJECT;
struct _DEVICE_OBJECT;

#define IOREQ_TYPE_NULL 1
#define IOREQ_TYPE_INSTALL 2
#define IOREQ_TYPE_UNINSTALL 3
#define IOREQ_TYPE_CREATE 4
#define IOREQ_TYPE_READ 5
#define IOREQ_TYPE_WRITE 6
#define IOREQ_TYPE_CONTROL 7
#define IOREQ_TYPE_CANCEL 8
#define IOREQ_FLAG_NONBLOCK 1
#define IOREQ_FLAG_ASYNC 2
typedef void(*PIOREQ_CALLBACK)(struct _DEVICE_OBJECT*, struct _IOREQ_OBJECT*);
typedef struct _IOREQ_OBJECT
{
	USHORT Type;
	USHORT Size;
	USHORT Flags;
	//struct _DEVICE_OBJECT* Device;
	KSTATUS Status;
	ULONG RequestCode;
	PVOID Buffer;
	union {
		PKSTRING Name;
		ULONG Id;
		PVOID Pointer;
	} ObjectAttribute;
	struct _MUTEX Event;
	LIST_ENTRY RequestList;
	PIOREQ_CALLBACK UpdateCallback;
} IOREQ_OBJECT, *PIOREQ_OBJECT;

typedef KSTATUS(*PMANAGER_DISPATCH)(struct _MANAGER_OBJECT*, struct _IOREQ_OBJECT*);
typedef struct _MANAGER_OBJECT
{
	PKSTRING ManagerName;
	PMANAGER_DISPATCH IOHandler;
} MANAGER_OBJECT, *PMANAGER_OBJECT;

#define DEVICE_FLAG_NOQUEUE 1
#define DEVICE_FLAG_READONLY 2
#define DEVICE_FLAG_DYNAMIC 4
#define DEVICE_FLAG_BINDCPU0 8
typedef void(*PDEVICE_DISPATCH)(struct _DEVICE_OBJECT*, struct _IOREQ_OBJECT*);
typedef struct _DEVICE_OBJECT
{
	USHORT Flags;
	REF_COUNT ReferenceCount;
	PKSTRING DeviceName;
	PDEVICE_DISPATCH IOHandler;
	PVOID DeviceStorage; // Implementation-dependent
	struct _MUTEX Lock; // Proposed. Like Linux's flock().
	SPINLOCK IORequestLock;
	LIST_ENTRY IORequest;
	LIST_ENTRY DeviceList;
} DEVICE_OBJECT, *PDEVICE_OBJECT;

void IoUpdateRequest(PDEVICE_OBJECT, PIOREQ_OBJECT, KSTATUS);
void IoInitializeRequest(PIOREQ_OBJECT);
KSTATUS IoCallDevice(PDEVICE_OBJECT, PIOREQ_OBJECT);
void IoInitializeDevice(PDEVICE_OBJECT);
USR_ONLY KSTATUS IoRegisterDevice(PDEVICE_OBJECT); // NOTE: USR_ONLY for dynamic devices
KSTATUS HalInitializeDeviceManager();
#define IoTryToLockDevice(Device) KeTestMutexSignaled(&Device->Lock, FALSE)
#define IoUnlockDevice(Device) KeSetMutexSignaled(&Device->Lock)
//KSTATUS IoUnloadDevice(PDEVICE_OBJECT);
//PDEVICE_OBJECT IouLookupDevice(PKSTRING);
extern OBJECT_POOL IORequestPool;
#define IoAllocateRequest() ({BOOL __t = arch_disable_trap(); \
	PIOREQ_OBJECT __p = (PIOREQ_OBJECT)MmAllocateObject(&IORequestPool); \
	if (__p) IoInitializeRequest(__p); \
	if (__t) arch_enable_trap(); __p;})
#define IoFreeRequest(Request) ({BOOL __t = arch_disable_trap(); \
	MmFreeObject(&IORequestPool, (PVOID)Request); \
	if (__t) arch_enable_trap();})

#endif