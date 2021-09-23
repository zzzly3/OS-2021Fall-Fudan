#ifndef __DEV_H__
#define __DEV_H__

#include <common/spinlock.h>
#include <common/string.h>
#include <common/lutil.h>
#include <ob/ioreq.h>

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

KSTATUS IoLockDevice(PDEVICE_OBJECT DeviceObject);
KSTATUS IoTryToLockDevice(PDEVICE_OBJECT DeviceObject);
KSTATUS IoUnlockDevice(PDEVICE_OBJECT DeviceObject);
KSTATUS IoCallDevice(PDEVICE_OBJECT DeviceObject, PIOREQ_OBJECT IOReq);

#endif