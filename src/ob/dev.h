#ifndef __DEV_H__
#define __DEV_H__

#include <common/spinlock.h>
#include <common/string.h>
#include <common/lutil.h>
#include <driver/uart.h>

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
typedef BOOL(*PIOREQ_CALLBACK)(struct _DEVICE_OBJECT*, struct _IOREQ_OBJECT*);
typedef struct _IOREQ_OBJECT
{
	USHORT Type;
	USHORT Size;
	USHORT Flags;
	KSTATUS Status;
	ULONG RequestCode;
	PVOID Buffer;
	union {
		PKSTRING Name;
		ULONG Id;
		PVOID Pointer;
	} ObjectAttribute;
	PIOREQ_CALLBACK UpdateCallback;
} IOREQ_OBJECT, *PIOREQ_OBJECT;

typedef KSTATUS(*PMANAGER_DISPATCH)(struct _MANAGER_OBJECT*, struct _IOREQ_OBJECT*);
typedef struct _MANAGER_OBJECT
{
	PKSTRING ManagerName;
	PMANAGER_DISPATCH IOHandler;
} MANAGER_OBJECT, *PMANAGER_OBJECT;

#define DEVICE_FLAG_NOLOCK 1
#define DEVICE_FLAG_READONLY 2
#define DEVICE_FLAG_DYNAMIC 4
typedef BOOL(*PDEVICE_DISPATCH)(struct _DEVICE_OBJECT*, struct _IOREQ_OBJECT*);
typedef struct _DEVICE_OBJECT
{
	USHORT Flags;
	USHORT ReferenceCount;
	PKSTRING DeviceName;
	PDEVICE_DISPATCH IOHandler;
	PVOID DeviceStorage; // Implementation-dependent
	SPINLOCK Lock; // TODO: Replaced with scheduler-related lock
	SPINLOCK IOLock;
	LIST_ENTRY DeviceList;
} DEVICE_OBJECT, *PDEVICE_OBJECT;

BOOL IoUpdateRequest(PDEVICE_OBJECT, PIOREQ_OBJECT);
void IoInitializeRequest(PIOREQ_OBJECT);
KSTATUS IoCallDevice(PDEVICE_OBJECT, PIOREQ_OBJECT);
void IoInitializeDevice(PDEVICE_OBJECT);
KSTATUS IoRegisterDevice(PDEVICE_OBJECT);
KSTATUS ObInitializeDeviceManager();
BOOL IoTryToLockDevice(PDEVICE_OBJECT);
KSTATUS IoUnloadDevice(PDEVICE_OBJECT);
#define IoUnlockDevice ObUnlockObject
//PDEVICE_OBJECT IouLookupDevice(PKSTRING);

#endif