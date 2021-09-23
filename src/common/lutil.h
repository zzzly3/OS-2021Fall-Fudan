// Utils added by LY.
#ifndef __LUTIL__
#define __LUTIL__

typedef unsigned char BOOL;
#define TRUE ((BOOL)1)
#define FALSE ((BOOL)0)

typedef void* PVOID;
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
	STATUS_INVALID_ARGUMENT,
	STATUS_DEVICE_BUSY,
	STATUS_UNSUPPORTED,
	STATUS_FAILURE,
	STATUS_SUCCESS,
	STATUS_COMPLETED,
	STATUS_PENDING
} KSTATUS;
#define KSUCCESS(stat) ((stat)>=STATUS_SUCCESS)

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

static inline void itos(int n, char* s, int base)
{
	static const char ch[] = {"0123456789abcdef"};
	if (base == 10 && n < 0)
		*s++ = '-';
	char *t = s, c;
	do
	{
		*t++ = ch[n % base];
		n /= base;
	} while (n);
	*t-- = '\0';
	while (s < t)
		c = *s, *s++ = *t, *t-- = c;
}

#endif