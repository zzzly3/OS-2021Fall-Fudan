#ifndef __DEV_H__
#define __DEV_H__

#include <common/spinlock.h>
#include <common/string.h>
#include <common/lutil.h>

struct _MANAGER_OBJECT;
struct _IOREQ_OBJECT;
struct _DEVICE_OBJECT;

typedef enum
{
	IOREQ_TYPE_NULL,
	IOREQ_TYPE_INSTALL,
	IOREQ_TYPE_UNINSTALL,
	IOREQ_TYPE_CREATE,
	IOREQ_TYPE_READ,
	IOREQ_TYPE_WRITE,
	IOREQ_TYPE_CONTROL,
	IOREQ_TYPE_CANCEL
} IOREQ_TYPE;
#define IOREQ_FLAG_NONBLOCK 1
#define IOREQ_FLAG_ASYNC 2
typedef BOOL(*PIOREQ_CALLBACK)(struct _DEVICE_OBJECT*, struct _IOREQ_OBJECT*);
typedef struct _IOREQ_OBJECT
{
	IOREQ_TYPE Type;
	KSTATUS Status;
	ULONG Flags;
	ULONG RequestCode;
	ULONG Size;
	PVOID Buffer;
	union {
		PKSTRING Name;
		ULONG Id;
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
typedef BOOL(*PDEVICE_DISPATCH)(struct _DEVICE_OBJECT*, struct _IOREQ_OBJECT*);
typedef struct _DEVICE_OBJECT
{
	ULONG Flags;
	PKSTRING DeviceName;
	PDEVICE_DISPATCH IOHandler;
	PVOID DeviceStorage; // Implementation-dependent
	PSPINLOCK Lock; // TODO: Replaced with scheduler-related lock
} DEVICE_OBJECT, *PDEVICE_OBJECT;

BOOL IoUpdateRequest(PDEVICE_OBJECT, PIOREQ_OBJECT);
void IoInitializeRequest(PIOREQ_OBJECT);
KSTATUS IoLockDevice(PDEVICE_OBJECT);
KSTATUS IoTryToLockDevice(PDEVICE_OBJECT);
KSTATUS IoUnlockDevice(PDEVICE_OBJECT);
KSTATUS IoCallDevice(PDEVICE_OBJECT, PIOREQ_OBJECT);

#endif