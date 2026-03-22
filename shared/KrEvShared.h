#ifndef KREV_SHARED_H
#define KREV_SHARED_H

#define KREV_DEVICE_NAME      L"\\Device\\KrEv"
#define KREV_DOS_DEVICE_NAME  L"\\DosDevices\\KrEv"
#define KREV_WIN32_DEVICE_NAME L"\\\\.\\KrEv"

#define KREV_MESSAGE_MAX_CHARS 256

#define IOCTL_KREV_GET_MESSAGE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_DATA)

typedef struct _KREV_MESSAGE_RESPONSE {
    unsigned long Version;
    unsigned long MessageLength;
    wchar_t Message[KREV_MESSAGE_MAX_CHARS];
} KREV_MESSAGE_RESPONSE;

#endif
