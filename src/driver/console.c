#include <driver/console.h>

static DEVICE_OBJECT ConsoleDevice;
static KSTRING ConsoleDeviceName;
static SPINLOCK ConsoleIOLock;

const PDEVICE_OBJECT HalConsoleDevice = &ConsoleDevice;

static void ConsoleHandler(PDEVICE_OBJECT dev, PIOREQ_OBJECT req)
{
	// TODO: Upgrade the lock to a scheduler-related one
	uart_put_char('a');
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
			for (int i = 0; i < req->Size; i++)
			{
				char ch = uart_get_char();
				if (uart_valid_char(ch))
					buf[i] = ch;
				else
					i--;
			}
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
