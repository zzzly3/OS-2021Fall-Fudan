#ifndef __MSG_H__
#define __MSG_H__

#include <common/lutil.h>
#include <ob/mutex.h>

#define MSG_TYPE_CHILDEXIT 1
#define MSG_TYPE_CHILDLEAVE 2

#define MSG_FLAG_FOWRAWD 1

typedef struct _MESSAGE
{
	int Type;
	int Flags;
	LIST_ENTRY MsgList;
	ULONG64 Data;
} MESSAGE, *PMESSAGE;

typedef struct _MESSAGE_QUEUE
{
	int ChildCount;
	SPINLOCK Lock;
	LIST_ENTRY MsgList;
	MUTEX MsgSignal;
} MESSAGE_QUEUE, *PMESSAGE_QUEUE;

void KeFreeMessage(PMESSAGE);
BOOL KeQueueMessage(PMESSAGE_QUEUE, int, ULONG64);
USR_ONLY PMESSAGE KeUserWaitMessage(PMESSAGE_QUEUE);
void KeInitializeMessageQueue(PMESSAGE_QUEUE);
void KeClearMessageQueue(PMESSAGE_QUEUE);
PMESSAGE KeGetMessage(PMESSAGE_QUEUE);
int KeUpdateQueueCounter(PMESSAGE_QUEUE, int);

#endif