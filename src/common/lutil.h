// Utils added by LY.
#ifndef __LUTIL__
#define __LUTIL__

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
typedef char CHAR;
typedef char* PCHAR;
typedef const char* CPCHAR;
typedef struct
{
	int len;
	CPCHAR str;
} KSTRING, *PKSTRING;

typedef enum
{
	STATUS_ACCESS_DENIED,
	STATUS_ACCESS_VIOLATION,
	STATUS_NO_ENOUGH_MEMORY,
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

static inline void itos(long long n, char* s, int base)
{
	unsigned long long nn = n;
	if (base == 10 && n < 0)
		*s++ = '-', nn = -n;
	char *t = s, c;
	do
	{
		*t++ = "0123456789abcdef"[nn % base];
		nn /= base;
	} while (nn);
	*t-- = '\0';
	while (s < t)
		c = *s, *s++ = *t, *t-- = c;
}

#define ObLockObject(obj) KeAcquireSpinLock(&(obj)->Lock)
#define ObTryToLockObject(obj) KeTryToAcquireSpinLock(&(obj)->Lock)
#define ObUnlockObject(obj) KeReleaseSpinLock(&(obj)->Lock)
#define ObReferenceObject(obj) ({ObLockObject(obj); \
	BOOL __mret = (obj)->ReferenceCount < OBJECT_MAX_REFERENCE; \
	(obj)->ReferenceCount += __mret ? 1 : 0; \
	ObUnlockObject(obj); \
	__mret;})
#define ObDereferenceObject(obj) (ObLockObject(obj),(obj)->ReferenceCount--,ObUnlockObject(obj))

#endif