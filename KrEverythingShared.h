#ifndef KREVERYTHING_SHARED_H
#define KREVERYTHING_SHARED_H

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <winioctl.h>
#endif


#define KREVERYTHING_PROTOCOL_VERSION 1UL
#define KREVERYTHING_DEVICE_TYPE 0x8000

#define KREVERYTHING_KERNEL_DEVICE_NAME L"\\Device\\KrEverything"
#define KREVERYTHING_DOS_DEVICE_NAME L"\\DosDevices\\KrEverything"
#define KREVERYTHING_WIN32_DEVICE_PATH L"\\\\.\\KrEverything"

#define KREVERYTHING_MAX_IMAGE_NAME 260

#define IOCTL_KREVERYTHING_GET_PROCESS_LIST \
    CTL_CODE(KREVERYTHING_DEVICE_TYPE, 0x800, METHOD_BUFFERED, FILE_READ_DATA)

typedef struct _KREVERYTHING_PROCESS_ENTRY {
    ULONGLONG ProcessId;
    ULONGLONG ParentProcessId;
    ULONG ThreadCount;
    ULONG HandleCount;
    ULONG SessionId;
    LONG BasePriority;
    ULONG PageFaultCount;
    ULONGLONG CreateTime;
    ULONGLONG UserTime;
    ULONGLONG KernelTime;
    ULONGLONG PeakWorkingSetSize;
    ULONGLONG WorkingSetSize;
    ULONGLONG PeakVirtualSize;
    ULONGLONG VirtualSize;
    ULONGLONG PrivatePageCount;
    WCHAR ImageName[KREVERYTHING_MAX_IMAGE_NAME];
} KREVERYTHING_PROCESS_ENTRY;

typedef struct _KREVERYTHING_PROCESS_LIST_HEADER {
    ULONG Version;
    ULONG EntryCount;
    ULONG BytesRequired;
    ULONG Reserved;
} KREVERYTHING_PROCESS_LIST_HEADER;

#endif

