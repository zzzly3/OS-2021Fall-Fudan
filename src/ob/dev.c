#include <ob/dev.h>

DEVICE_OBJECT RootDeviceX;
static SPINLOCK DeviceListLock;

#define next_device(dev) container_of((dev)->DeviceList.Backward, DEVICE_OBJECT, DeviceList)
#define last_device(dev) container_of((dev)->DeviceList.Forward, DEVICE_OBJECT, DeviceList)

BOOL IoUpdateRequest(PDEVICE_OBJECT DeviceObject, PIOREQ_OBJECT IOReq)
{
	if (IOReq->Flags & IOREQ_FLAG_ASYNC)
		return IOReq->UpdateCallback(DeviceObject, IOReq);
	else
		return TRUE;
}

void IoInitializeRequest(PIOREQ_OBJECT IOReq)
{
	memset(IOReq, 0, sizeof(IOREQ_OBJECT));
	IOReq->Status = STATUS_PENDING;
}

KSTATUS IoCallDevice(PDEVICE_OBJECT DeviceObject, PIOREQ_OBJECT IOReq)
{
	if (DeviceObject->IOHandler == NULL)
		return STATUS_UNSUPPORTED;
	if ((DeviceObject->Flags & DEVICE_FLAG_READONLY) && 
			(IOReq->Type == IOREQ_TYPE_WRITE || IOReq->Type == IOREQ_TYPE_CREATE))
		return STATUS_UNSUPPORTED;
	if (!(DeviceObject->Flags & DEVICE_FLAG_DYNAMIC) && 
			(IOReq->Type == IOREQ_TYPE_INSTALL || IOReq->Type == IOREQ_TYPE_UNINSTALL))
		return STATUS_UNSUPPORTED;
	if (IOReq->Flags & IOREQ_FLAG_ASYNC)
	{
		// TODO: Async
		return STATUS_UNSUPPORTED;
	}
	else
	{
		// TODO: Scheduler-related logic?
		BOOL ret = FALSE;
		if (DeviceObject->Flags & DEVICE_FLAG_NOLOCK)
		{
			ret = DeviceObject->IOHandler(DeviceObject, IOReq);
		}
		else
		{
			if (IOReq->Flags & IOREQ_FLAG_NONBLOCK)
			{
				return STATUS_UNSUPPORTED;
			}
			else
			{
				KeAcquireSpinLock(&DeviceObject->IOLock);
			}
			ret = DeviceObject->IOHandler(DeviceObject, IOReq);
			KeReleaseSpinLock(&DeviceObject->IOLock);
		}
		if (ret)
			return IOReq->Status;
		else
			return STATUS_FAILURE;
	}
	return STATUS_FAILURE;
}

// BOOL IoTryToLockDevice(PDEVICE_OBJECT DeviceObject)
// {
// 	if (!ObTryToLockObjectFast(DeviceObject))
// 		return FALSE;
// 	if (DeviceObject->ReferenceCount <= 1)
// 	{
// 		//TODO: eq 0 SHOULD trigger BUGCHECK! (ACCESS_WITHOUT_REFERENCE)
// 		return TRUE;
// 	}
// 	else
// 	{
// 		ObUnlockObject(DeviceObject);
// 		return FALSE;
// 	}
// }

void IoInitializeDevice(PDEVICE_OBJECT DeviceObject)
{
	memset(DeviceObject, 0, sizeof(DEVICE_OBJECT));
	KeInitializeSpinLock(&DeviceObject->Lock);
	KeInitializeSpinLock(&DeviceObject->IOLock);
	init_rc(&DeviceObject->ReferenceCount);
}

RT_ONLY KSTATUS IoRegisterDevice(PDEVICE_OBJECT DeviceObject)
{
	ObLockObjectFast(DeviceObject);
	KeAcquireSpinLockFast(&DeviceListLock);
	LibInsertListEntry(&RootDeviceX.DeviceList, &DeviceObject->DeviceList);
	KeReleaseSpinLockFast(&DeviceListLock);
	if (DeviceObject->Flags & DEVICE_FLAG_DYNAMIC)
	{
		IOREQ_OBJECT install_req;
		IoInitializeRequest(&install_req);
		install_req.Type = IOREQ_TYPE_INSTALL;
		KSTATUS ret = IoCallDevice(DeviceObject, &install_req);
		if (!KSUCCESS(ret))
		{
			KeAcquireSpinLockFast(&DeviceListLock);
			LibRemoveListEntry(&DeviceObject->DeviceList);
			KeReleaseSpinLockFast(&DeviceListLock);
		}
		ObUnlockObjectFast(DeviceObject);
		return ret;
	}
	else
	{
		ObUnlockObjectFast(DeviceObject);
		return STATUS_SUCCESS;
	}
}

// KSTATUS IoUnloadDevice(PDEVICE_OBJECT DeviceObject)
// {
// 	if (!(DeviceObject->Flags & DEVICE_FLAG_DYNAMIC))
// 		return STATUS_UNSUPPORTED;
// 	if (!IoTryToLockDevice(DeviceObject))
// 		return STATUS_DEVICE_BUSY;
// 	IOREQ_OBJECT uninstall_req;
// 	IoInitializeRequest(&uninstall_req);
// 	uninstall_req.Type = IOREQ_TYPE_UNINSTALL;
// 	KSTATUS ret = IoCallDevice(DeviceObject, &uninstall_req);
// 	if (KSUCCESS(ret))
// 	{
// 		KeAcquireSpinLockSafe(&DeviceListLock);
// 		LibRemoveListEntry(&DeviceObject->DeviceList);
// 		KeReleaseSpinLockSafe(&DeviceListLock);
// 		// If success, return without unlock the device.
// 		// Cleanup? (associated with the driver?)
// 	}
// 	else
// 	{
// 		IoUnlockDevice(DeviceObject);
// 	}
// 	return ret;
// }

PDEVICE_OBJECT IoiLookupDevice(PKSTRING DeviceName)
{
	PDEVICE_OBJECT p0 = &RootDeviceX, p = next_device(p0);
	while (p != p0)
	{
		if (p->DeviceName)
		{
			if (LibCompareKString(p->DeviceName, DeviceName))
			{
				return p;
			}
		}
		p = next_device(p);
	}
	return NULL;
}

KSTATUS HalInitializeDeviceManager()
{
	KeInitializeSpinLock(&DeviceListLock);
	IoInitializeDevice(&RootDeviceX);
	RootDeviceX.Flags |= DEVICE_FLAG_NOLOCK;
	RootDeviceX.DeviceList.Forward = RootDeviceX.DeviceList.Backward = &RootDeviceX.DeviceList;
	init_interrupt();
	return STATUS_SUCCESS;
}
