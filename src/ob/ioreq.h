#ifndef __IOREQ_H__
#define __IOREQ_H__

#include <common/spinlock.h>
#include <common/string.h>
#include <common/lutil.h>
#include <ob/dev.h>

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

BOOL IoUpdateRequest(struct _DEVICE_OBJECT*, struct _IOREQ_OBJECT*);
void IoInitializeRequest(struct _IOREQ_OBJECT*);

#endif