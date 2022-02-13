#include <ob/msg.h>
#include <ob/mem.h>
#include <mod/bug.h>

static OBJECT_POOL MessagePool;

void ObInitializeMessages()
{
	MmInitializeObjectPool(&MessagePool, sizeof(MESSAGE));
}

void KeInitializeMessageQueue(PMESSAGE_QUEUE MessageQueue)
{
	MessageQueue->MsgList.Forward = MessageQueue->MsgList.Backward = &MessageQueue->MsgList;
	KeInitializeMutex(&MessageQueue->MsgSignal);
	KeTestMutexSignaled(&MessageQueue->MsgSignal, FALSE);
	KeInitializeSpinLock(&MessageQueue->Lock);
}

void KeFreeMessage(PMESSAGE Message)
{
	BOOL te = arch_disable_trap();
	MmFreeObject(&MessagePool, Message);
	if (te) arch_enable_trap();
}

void KeClearMessageQueue(PMESSAGE_QUEUE MessageQueue)
{
	ObLockObject(MessageQueue);
	PMESSAGE p = container_of(MessageQueue->MsgList.Backward, MESSAGE, MsgList);
	while (&p->MsgList != &MessageQueue->MsgList)
	{
		PMESSAGE x = p;
		p = container_of(p->MsgList.Backward, MESSAGE, MsgList);
		MmFreeObject(&MessagePool, p);
	}
	ObUnlockObject(MessageQueue);
}

BOOL KeQueueMessage(PMESSAGE_QUEUE MessageQueue, int Type, ULONG64 Data)
{
	BOOL te = arch_disable_trap();
	PMESSAGE msg = MmAllocateObject(&MessagePool);
	if (msg == NULL)
	{
		if (te) arch_enable_trap();
		return FALSE;
	}
	msg->Type = Type;
	msg->Data = Data;
	ObLockObjectFast(MessageQueue);
	if (LibTestListEmpty(&MessageQueue->MsgList))
		KeSetMutexSignaled(&MessageQueue->MsgSignal);
	LibInsertListEntry(MessageQueue->MsgList.Forward, &msg->MsgList);
	ObUnlockObjectFast(MessageQueue);
	if (te) arch_enable_trap();
	return TRUE;
}

USR_ONLY PMESSAGE KeUserWaitMessage(PMESSAGE_QUEUE MessageQueue)
{
	if (!KSUCCESS(KeUserWaitForMutexSignaled(&MessageQueue->MsgSignal, FALSE)))
		return NULL; // ???
	ObLockObject(MessageQueue);
	ASSERT(!LibTestListEmpty(&MessageQueue->MsgList), BUG_BADLOCK);
	PMESSAGE msg = container_of(MessageQueue->MsgList.Backward, MESSAGE, MsgList);
	LibRemoveListEntry(&msg->MsgList);
	if (!LibTestListEmpty(&MessageQueue->MsgList))
		KeSetMutexSignaled(&MessageQueue->MsgSignal);
	ObUnlockObject(MessageQueue);
	return msg;
}

PMESSAGE KeGetMessage(PMESSAGE_QUEUE MessageQueue)
{
	if (!KeTestMutexSignaled(&MessageQueue->MsgSignal, FALSE))
		return NULL;
	ObLockObject(MessageQueue);
	ASSERT(!LibTestListEmpty(&MessageQueue->MsgList), BUG_BADLOCK);
	PMESSAGE msg = container_of(MessageQueue->MsgList.Backward, MESSAGE, MsgList);
	LibRemoveListEntry(&msg->MsgList);
	if (!LibTestListEmpty(&MessageQueue->MsgList))
		KeSetMutexSignaled(&MessageQueue->MsgSignal);
	ObUnlockObject(MessageQueue);
	return msg;
}