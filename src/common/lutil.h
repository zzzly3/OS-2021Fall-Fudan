// Utils added by LY.
#ifndef __LUTIL__
#define __LUTIL__

#define whlie while

typedef unsigned char BOOL;
#define TRUE ((BOOL)1)
#define FALSE ((BOOL)0)

#define OBJECT_MAX_REFERENCE 32767

#define offsetof(TYPE,MEMBER)   ((size_t) &((TYPE *)0)->MEMBER)
#define container_of(PTR,TYPE,MEMBER)    ({  \
    const typeof(((TYPE *)0)->MEMBER) *__mptr=(PTR);  \
    (TYPE *) ((char *)__mptr - offsetof(TYPE,MEMBER)); })

typedef void* PVOID;
typedef unsigned short USHORT;
typedef unsigned long ULONG;
typedef unsigned long long ULONG64;
typedef unsigned char UCHAR;
typedef char CHAR; // In RaspberryPi, it seems that CHAR=UCHAR.
typedef char* PCHAR;
typedef const char* CPCHAR;
typedef struct
{
	int len;
	CPCHAR str;
} KSTRING, *PKSTRING;

typedef enum
{
	STATUS_ALERTED,
	STATUS_ACCESS_DENIED,
	STATUS_ACCESS_VIOLATION,
	STATUS_NO_ENOUGH_MEMORY,
	STATUS_ALREADY_MAPPED,
	STATUS_PAGE_NOT_FOUND,
	STATUS_INVALID_ARGUMENT,
	STATUS_FILE_NOT_FOUND,
	STATUS_DEVICE_BUSY,
	STATUS_UNSUPPORTED,
	STATUS_FAILURE,
	STATUS_SUCCESS,
	STATUS_COMPLETED,
	STATUS_PENDING,
	STATUS_REDIRECTED
} KSTATUS;
#define KSUCCESS(stat) ((stat)>=STATUS_SUCCESS)

typedef UCHAR EXECUTE_LEVEL;
#define EXECUTE_LEVEL_ISR 10
#define EXECUTE_LEVEL_RT 5
#define EXECUTE_LEVEL_APC 2
#define EXECUTE_LEVEL_USR 0

#define USR_ONLY // Indicates the API MUST be called in USR level.
#define APC_ONLY
#define RT_ONLY
#define LEQAPC_ONLY // EL <= APC
#define LEQRT_ONLY
#define UNSAFE

typedef struct _LIST_ENTRY
{
	struct _LIST_ENTRY* Forward;
	struct _LIST_ENTRY* Backward;
} LIST_ENTRY, *PLIST_ENTRY;

#define LibInsertListEntry(last_entry, new_entry) \
	((new_entry)->Backward = (last_entry)->Backward)->Forward = (new_entry), \
	((new_entry)->Forward = (last_entry))->Backward = (new_entry)
#define LibRemoveListEntry(entry) \
	((entry)->Forward->Backward = (entry)->Backward)->Forward = (entry)->Forward
#define LibPopSingleListEntry(head) ({typeof(head) __r = (head); \
	if ((head) != NULL) (head) = (head)->NextEntry; \
	__r;})

static inline BOOL LibInitializeKString(PKSTRING kstr, CPCHAR cstr, int len)
{
	for (int i = 0; i <= len; i++)
	{
		if (cstr[i] == '\0')
		{
			kstr->len = i;
			kstr->str = cstr;
			return TRUE;
		}
	}
	return FALSE;
}

static inline BOOL LibCompareKString(PKSTRING kstr1, PKSTRING kstr2)
{
	if (kstr1->len != kstr2->len)
		return FALSE;
	for (CPCHAR p1 = kstr1->str, p2 = kstr2->str; *p1; p1++, p2++)
		if (*p1 != *p2)
			return FALSE;
	return TRUE;
}

static inline void LibKStringToCString(PKSTRING kstr, PCHAR cstr, int len)
{
	int i = 0;
	for (int i = 0; i < len && i <= kstr->len; i++)
	{
		cstr[i] = kstr->str[i];
	}
	cstr[i - 1] = 0;
}

static inline int itos(long long n, char* s, int base)
{
	unsigned long long nn = n;
	int r = 0;
	if (base == 10 && n < 0)
		*s++ = '-', nn = -n, r++;
	char *t = s, c;
	do
	{
		*t++ = "0123456789abcdef"[nn % base];
		nn /= base;
	} while (nn);
	r += t - s;
	*t-- = '\0';
	while (s < t)
		c = *s, *s++ = *t, *t-- = c;
	return r;
}

#define ObTryToLockObjectFast(obj) KeTryToAcquireSpinLockFast(&(obj)->Lock)
#define ObLockObjectFast(obj) KeAcquireSpinLockFast(&(obj)->Lock)
#define ObUnlockObjectFast(obj) KeReleaseSpinLockFast(&(obj)->Lock)
#define ObReferenceObjectFast(obj) ({ObLockObjectFast(obj); \
	BOOL __mret = (obj)->ReferenceCount < OBJECT_MAX_REFERENCE; \
	(obj)->ReferenceCount += __mret ? 1 : 0; \
	ObUnlockObjectFast(obj); \
	__mret;})
#define ObDereferenceObjectFast(obj) (ObLockObjectFast(obj),(obj)->ReferenceCount--,ObUnlockObjectFast(obj))
#define ObLockObject(obj) KeAcquireSpinLock(&(obj)->Lock)
#define ObUnlockObject(obj) KeReleaseSpinLock(&(obj)->Lock)
#define ObReferenceObject(obj) ({ObLockObject(obj); \
	BOOL __mret = (obj)->ReferenceCount < OBJECT_MAX_REFERENCE; \
	(obj)->ReferenceCount += __mret ? 1 : 0; \
	ObUnlockObject(obj); \
	__mret;})
#define ObDereferenceObject(obj) (ObLockObject(obj),(obj)->ReferenceCount--,ObUnlockObject(obj))

#endif