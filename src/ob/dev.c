#include <ob/dev.h>

KSTATUS IoLockDevice(PDEVICE_OBJECT DeviceObject)
{
	if (DeviceObject->Flags & DEVICE_FLAG_NOLOCK)
		return STATUS_UNSUPPORTED;
	KeAcquireSpinLock(DeviceObject->Lock);
	return STATUS_SUCCESS;
}

KSTATUS IoTryToLockDevice(PDEVICE_OBJECT DeviceObject)
{
	if (DeviceObject->Flags & DEVICE_FLAG_NOLOCK)
		return STATUS_UNSUPPORTED;
	if (KeTryToAcquireSpinLock(DeviceObject->Lock))
		return STATUS_SUCCESS;
	else
		return STATUS_DEVICE_BUSY;
}

KSTATUS IoUnlockDevice(PDEVICE_OBJECT DeviceObject)
{
	if (DeviceObject->Flags & DEVICE_FLAG_NOLOCK)
		return STATUS_UNSUPPORTED;
	KeReleaseSpinLock(DeviceObject->Lock);
	return STATUS_SUCCESS;
}

KSTATUS IoCallDevice(PDEVICE_OBJECT DeviceObject, PIOREQ_OBJECT IOReq)
{
	if ((DeviceObject->Flags & DEVICE_FLAG_READONLY) && 
			(IOReq->Type == IOREQ_TYPE_WRITE || IOReq->Type == IOREQ_TYPE_CREATE))
		return STATUS_UNSUPPORTED;
	if (IOReq->Flags & IOREQ_FLAG_ASYNC)
	{
		// TODO: Async
		return STATUS_UNSUPPORTED;
	}
	else
	{
		// TODO: Scheduler-related logic?
		if (DeviceObject->IOHandler(IOReq))
			return IOReq->Status;
		else
			return STATUS_FAILURE;
	}
}
