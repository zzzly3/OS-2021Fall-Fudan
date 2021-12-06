#include <ob/dev.h>
#include <mod/scheduler.h>
#include <mod/bug.h>

DEVICE_OBJECT RootDeviceX;
static SPINLOCK DeviceListLock;

#define next_device(dev) container_of((dev)->DeviceList.Backward, DEVICE_OBJECT, DeviceList)
#define last_device(dev) container_of((dev)->DeviceList.Forward, DEVICE_OBJECT, DeviceList)

void IoiDispatchRequests(PDEVICE_OBJECT DeviceObject);

// PENDING -> [SUBMITTED ->] <RESULT>
void IoUpdateRequest(PDEVICE_OBJECT DeviceObject, PIOREQ_OBJECT IOReq, KSTATUS Status)
{
	ASSERT(Status != STATUS_PENDING, BUG_BADIO);
	ASSERT(IOReq->Status != Status, BUG_BADIO);
	if (!(DeviceObject->Flags & DEVICE_FLAG_NOQUEUE) &&
		!(IOReq->Flags & IOREQ_FLAG_NONBLOCK) &&
		IOReq->Status == STATUS_PENDING)
	{
		// Do next while using queue
		KeAcquireSpinLock(&DeviceObject->IORequestLock);
		LibRemoveListEntry(&IOReq->RequestList);
		if (!LibTestListEmpty(&DeviceObject->IORequest))
		{
			// Do next request
			ASSERT(
				KeQueueWorkerApc((PAPC_ROUTINE)IoiDispatchRequests, (ULONG64)DeviceObject),
				BUG_BADIO);
		}
		KeReleaseSpinLock(&DeviceObject->IORequestLock);
	}
	IOReq->Status = Status;
	if (IOReq->UpdateCallback)
		IOReq->UpdateCallback(DeviceObject, IOReq);
	if (Status != STATUS_SUBMITTED)
		KeSetMutexSignaled(&IOReq->Event);
}

void IoInitializeRequest(PIOREQ_OBJECT IOReq)
{
	memset(IOReq, 0, sizeof(IOREQ_OBJECT));
	KeInitializeMutex(&IOReq->Event);
	KeTestMutexSignaled(&IOReq->Event, FALSE);
	IOReq->Status = STATUS_PENDING;
}

KSTATUS IoCallDevice(PDEVICE_OBJECT DeviceObject, PIOREQ_OBJECT IOReq)
{
	ASSERT(IOReq->Status == STATUS_PENDING, BUG_BADIO);
	if (DeviceObject->IOHandler == NULL)
		return STATUS_UNSUPPORTED;
	if ((DeviceObject->Flags & DEVICE_FLAG_READONLY) && 
			(IOReq->Type == IOREQ_TYPE_WRITE || IOReq->Type == IOREQ_TYPE_CREATE))
		return STATUS_UNSUPPORTED;
	if (!(DeviceObject->Flags & DEVICE_FLAG_DYNAMIC) && 
			(IOReq->Type == IOREQ_TYPE_INSTALL || IOReq->Type == IOREQ_TYPE_UNINSTALL))
		return STATUS_UNSUPPORTED;
	// Queue-free device
	if (DeviceObject->Flags & DEVICE_FLAG_NOQUEUE)
	{
		DeviceObject->IOHandler(DeviceObject, IOReq);
		return IOReq->Status;
	}
	if (IOReq->Flags & IOREQ_FLAG_NONBLOCK)
	{
		// Non-block request
		if (!KeTryToAcquireSpinLock(&DeviceObject->IORequestLock))
			return STATUS_DEVICE_BUSY;
		KSTATUS ret = STATUS_DEVICE_BUSY;
		if (LibTestListEmpty(&DeviceObject->IORequest))
		{
			DeviceObject->IOHandler(DeviceObject, IOReq);
			ret = IOReq->Status;
		}
		KeReleaseSpinLock(&DeviceObject->IORequestLock);
		return ret;
	}
	else
	{
		// Queued request
		EXECUTE_LEVEL el = KeGetCurrentExecuteLevel();
		ASSERT((IOReq->Flags & IOREQ_FLAG_ASYNC) || el <= EXECUTE_LEVEL_APC, BUG_BADLEVEL);
		KeAcquireSpinLock(&DeviceObject->IORequestLock);
		LibInsertListEntry(DeviceObject->IORequest.Forward, &IOReq->RequestList);
		if (DeviceObject->IORequest.Forward == DeviceObject->IORequest.Backward)
		{
			// Start do requests
			ASSERT(
				KeQueueWorkerApc((PAPC_ROUTINE)IoiDispatchRequests, (ULONG64)DeviceObject), 
				BUG_BADIO);
		}
		KeReleaseSpinLock(&DeviceObject->IORequestLock);
		if (IOReq->Flags & IOREQ_FLAG_ASYNC)
		{
			// Async request
			return STATUS_PENDING;
		}
		else
		{
			// Sync request
			KeRaiseExecuteLevel(EXECUTE_LEVEL_APC);
			KSTATUS ret = KeWaitForMutexSignaled(&IOReq->Event, FALSE);
			if (KSUCCESS(ret))
				ret = IOReq->Status;
			KeLowerExecuteLevel(el);
			return ret;
		}
	}
}

void IoInitializeDevice(PDEVICE_OBJECT DeviceObject)
{
	memset(DeviceObject, 0, sizeof(DEVICE_OBJECT));
	KeInitializeMutex(&DeviceObject->Lock);
	KeInitializeSpinLock(&DeviceObject->IORequestLock);
	DeviceObject->IORequest.Forward = DeviceObject->IORequest.Backward = &DeviceObject->IORequest;
	init_rc(&DeviceObject->ReferenceCount);
}

RT_ONLY KSTATUS IoRegisterDevice(PDEVICE_OBJECT DeviceObject)
{
	IoTryToLockDevice(DeviceObject);
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
		IoUnlockDevice(DeviceObject);
		return ret;
	}
	else
	{
		IoUnlockDevice(DeviceObject);
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

void IoiDispatchRequests(PDEVICE_OBJECT DeviceObject)
{
	PIOREQ_OBJECT req = container_of(DeviceObject->IORequest.Backward, IOREQ_OBJECT, RequestList);
	DeviceObject->IOHandler(DeviceObject, req);
}

KSTATUS HalInitializeDeviceManager()
{
	KeInitializeSpinLock(&DeviceListLock);
	IoInitializeDevice(&RootDeviceX);
	RootDeviceX.Flags |= DEVICE_FLAG_NOQUEUE;
	RootDeviceX.DeviceList.Forward = RootDeviceX.DeviceList.Backward = &RootDeviceX.DeviceList;
	init_interrupt();
	return STATUS_SUCCESS;
}
