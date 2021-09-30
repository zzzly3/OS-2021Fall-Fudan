#include <ob/dev.h>

static DEVICE_OBJECT RootDeviceX;
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
		if (IOReq->Flags & IOREQ_FLAG_NONBLOCK)
		{
			if (!KeTryToAcquireSpinLock(&DeviceObject->IOLock))
				return STATUS_DEVICE_BUSY;
		}
		else
		{
			KeAcquireSpinLock(&DeviceObject->IOLock);
		}
		BOOL ret = DeviceObject->IOHandler(DeviceObject, IOReq);
		KeReleaseSpinLock(&DeviceObject->IOLock);
		if (ret)
			return IOReq->Status;
		else
			return STATUS_FAILURE;
	}
	return STATUS_FAILURE;
}

BOOL IoTryToLockDevice(PDEVICE_OBJECT DeviceObject)
{
	if (!ObTryToLockObject(DeviceObject))
		return FALSE;
	if (DeviceObject->ReferenceCount <= 1)
	{
		//TODO: eq 0 SHOULD trigger BUGCHECK! (ACCESS_WITHOUT_REFERENCE)
		return TRUE;
	}
	else
	{
		ObUnlockObject(DeviceObject);
		return FALSE;
	}
}

void IoInitializeDevice(PDEVICE_OBJECT DeviceObject)
{
	memset(DeviceObject, 0, sizeof(PDEVICE_OBJECT));
	KeInitializeSpinLock(&DeviceObject->Lock);
	KeInitializeSpinLock(&DeviceObject->IOLock);
}

KSTATUS IoRegisterDevice(PDEVICE_OBJECT DeviceObject)
{
	ObLockObject(DeviceObject);
	KeAcquireSpinLock(&DeviceListLock);
	LibInsertListEntry(&RootDeviceX.DeviceList, &DeviceObject->DeviceList);
	KeReleaseSpinLock(&DeviceListLock);
	if (DeviceObject->Flags & DEVICE_FLAG_DYNAMIC)
	{
		IOREQ_OBJECT install_req;
		IoInitializeRequest(&install_req);
		install_req.Type = IOREQ_TYPE_INSTALL;
		KSTATUS ret = IoCallDevice(DeviceObject, &install_req);
		if (!KSUCCESS(ret))
		{
			KeAcquireSpinLock(&DeviceListLock);
			LibRemoveListEntry(&DeviceObject->DeviceList);
			KeReleaseSpinLock(&DeviceListLock);
		}
		ObUnlockObject(DeviceObject);
		return ret;
	}
	else
	{
		ObUnlockObject(DeviceObject);
		return STATUS_SUCCESS;
	}
}

KSTATUS IoUnloadDevice(PDEVICE_OBJECT DeviceObject)
{
	if (!(DeviceObject->Flags & DEVICE_FLAG_DYNAMIC))
		return STATUS_UNSUPPORTED;
	if (!IoTryToLockDevice(DeviceObject))
		return STATUS_DEVICE_BUSY;
	IOREQ_OBJECT uninstall_req;
	IoInitializeRequest(&uninstall_req);
	uninstall_req.Type = IOREQ_TYPE_UNINSTALL;
	KSTATUS ret = IoCallDevice(DeviceObject, &uninstall_req);
	if (KSUCCESS(ret))
	{
		KeAcquireSpinLock(&DeviceListLock);
		LibRemoveListEntry(&DeviceObject->DeviceList);
		KeReleaseSpinLock(&DeviceListLock);
		// If success, return without unlock.
	}
	else
	{
		IoUnlockDevice(DeviceObject);
	}
	return ret;
}

PDEVICE_OBJECT IouLookupDevice(PKSTRING DeviceName)
{
	PDEVICE_OBJECT p0 = &RootDeviceX, p = next_device(p0);
	while (p != p0)
	{
		uart_put_char(((ULONG64)p&0xf)+'a');
		uart_put_char('\n');
		if (p->DeviceName && LibCompareKString(p->DeviceName, DeviceName))
		{
			return p;
		}
		uart_put_char('.');
		p = next_device(p);
		uart_put_char('.');
	}
	return NULL;
}

KSTATUS ObInitializeDeviceManager()
{
	KeInitializeSpinLock(&DeviceListLock);
	IoInitializeDevice(&RootDeviceX);
	RootDeviceX.Flags |= DEVICE_FLAG_NOLOCK;
	RootDeviceX.DeviceList.Forward = RootDeviceX.DeviceList.Backward = &RootDeviceX.DeviceList;
	return STATUS_SUCCESS;
}
