#include <ob/ioreq.h>

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