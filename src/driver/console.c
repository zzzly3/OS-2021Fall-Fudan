#include <driver/console.h>
#include <ob/mutex.h>

static DEVICE_OBJECT ConsoleDevice;
static KSTRING ConsoleDeviceName;
static SPINLOCK ConsoleIOLock;

const PDEVICE_OBJECT HalConsoleDevice = &ConsoleDevice;

#define CONSOLE 1

#define INPUT_BUF 128
struct {
	SPINLOCK lock;
	MUTEX read;
    char buf[INPUT_BUF];
    usize r;  // Read index
    usize w;  // Write index
    usize e;  // Edit index
} input;
#define C(x)      ((x) - '@')  // Control-x
#define BACKSPACE 0x100

static void consputc(int c) {
    if (c == BACKSPACE) {
        uart_put_char('\b');
        uart_put_char(' ');
        uart_put_char('\b');
    } else
        uart_put_char(c);
}

USR_ONLY
isize console_read(char *dst, isize n) {
    usize target = n;
    acquire_spinlock(&input.lock);
    while (n > 0) {
        while (input.r == input.w) {
        	release_spinlock(&input.lock);
            KeUserWaitForMutexSignaled(&input.read, FALSE);
            acquire_spinlock(&input.lock);
        }
        int c = input.buf[input.r++ % INPUT_BUF];
        if (c == C('D')) {  // EOF
            if (n < target) {
                // Save ^D for next time, to make sure
                // caller gets a 0-byte result.
                input.r--;
            }
            break;
        }
        *dst++ = c;
        --n;
        if (c == '\n')
            break;
    }
    if (input.r != input.w)
    	KeSetMutexSignaled(&input.read);
    release_spinlock(&input.lock);
    return target - n;
}

void console_intr(char (*getc)()) {
    int c, doprocdump = 0;

    acquire_spinlock(&input.lock);

    while ((c = getc()) != 0xff) {
        switch (c) {
            case C('P'):  // Process listing.
                // procdump() locks cons.lock indirectly; invoke later
                doprocdump = 1;
                break;
            case C('U'):  // Kill line.
                while (input.e != input.w && input.buf[(input.e - 1) % INPUT_BUF] != '\n') {
                    input.e--;
                    consputc(BACKSPACE);
                }
                break;
            case C('H'):
            case '\x7f':  // Backspace
                if (input.e != input.w) {
                    input.e--;
                    consputc(BACKSPACE);
                }
                break;
            default:
                if (c != 0 && input.e - input.r < INPUT_BUF) {
                    c = (c == '\r') ? '\n' : c;
                    input.buf[input.e++ % INPUT_BUF] = c;
                    consputc(c);
                    if (c == '\n' || c == C('D') || input.e == input.r + INPUT_BUF) {
                        input.w = input.e;
                        KeSetMutexSignaled(&input.read);
                    }
                }
                break;
        }
    }
    release_spinlock(&input.lock);
}

static void ConsoleHandler(PDEVICE_OBJECT dev, PIOREQ_OBJECT req)
{
	// TODO: Upgrade the lock to a scheduler-related one
	PSPINLOCK lock = (PSPINLOCK)(dev->DeviceStorage);
	if (req->Flags & IOREQ_FLAG_NONBLOCK)
	{
		IoUpdateRequest(dev, req, STATUS_UNSUPPORTED);
		return;
	}
	else
	{
		KeAcquireSpinLock(lock);
	}
	// TODO: Upgrade the READ to an interruption-driven one
	char* buf = (char*)(req->Buffer);
	KSTATUS ret = STATUS_SUCCESS;
	switch (req->Type)
	{
	case IOREQ_TYPE_READ:
		if (req->Flags & IOREQ_FLAG_NONBLOCK)
		{
			ret = STATUS_UNSUPPORTED;
		}
		else
		{
			ASSERT(KeGetCurrentExecuteLevel() == EXECUTE_LEVEL_USR, BUG_BADLEVEL);
			req->Size = console_read(buf, req->Size); // may block (switch out)
			ret = STATUS_COMPLETED;
		}
		break;
	case IOREQ_TYPE_WRITE:
		for (int i = 0; i < req->Size; i++)
		{
			uart_put_char(buf[i]);
		}
		ret = STATUS_COMPLETED;
		break;
	default:
		ret = STATUS_UNSUPPORTED;
		break;
	}
	KeReleaseSpinLock(lock);
	return IoUpdateRequest(dev, req, ret);
}

KSTATUS HalInitializeConsole()
{
	KeInitializeSpinLock(&input.lock);
	KeInitializeMutex(&input.read);
	IoInitializeDevice(&ConsoleDevice);
	LibInitializeKString(&ConsoleDeviceName, "console", 8);
	ConsoleDevice.Flags = DEVICE_FLAG_NOQUEUE;
	ConsoleDevice.DeviceName = &ConsoleDeviceName;
	ConsoleDevice.IOHandler = ConsoleHandler;
	ConsoleDevice.DeviceStorage = (PVOID)&ConsoleIOLock;
	init_uart();
	return IoRegisterDevice(&ConsoleDevice);
}

KSTATUS HalWriteConsoleString(PKSTRING outString)
{
	IOREQ_OBJECT req;
	IoInitializeRequest(&req);
	req.Type = IOREQ_TYPE_WRITE;
	req.Size = outString->len;
	req.Buffer = (PVOID)outString->str;
	return IoCallDevice(HalConsoleDevice, &req);
}

KSTATUS HalWriteConsoleChar(CHAR outChar)
{
	IOREQ_OBJECT req;
	CHAR ch = outChar;
	IoInitializeRequest(&req);
	req.Type = IOREQ_TYPE_WRITE;
	req.Size = 1;
	req.Buffer = (PVOID)&ch;
	return IoCallDevice(HalConsoleDevice, &req);
}

KSTATUS HalReadConsoleChar(PCHAR inChar)
{
	IOREQ_OBJECT req;
	IoInitializeRequest(&req);
	req.Type = IOREQ_TYPE_READ;
	req.Size = 1;
	req.Buffer = (PVOID)inChar;
	return IoCallDevice(HalConsoleDevice, &req);
}
