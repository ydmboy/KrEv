#include <ntddk.h>

#include "KrEverythingShared.h"

#define KR_TAG 'rEkK'
#define KR_INITIAL_QUERY_SIZE (64 * 1024)

typedef struct _KR_SYSTEM_PROCESS_INFORMATION {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    LARGE_INTEGER WorkingSetPrivateSize;
    ULONG HardFaultCount;
    ULONG NumberOfThreadsHighWatermark;
    ULONGLONG CycleTime;
    LARGE_INTEGER CreateTime;
    LARGE_INTEGER UserTime;
    LARGE_INTEGER KernelTime;
    UNICODE_STRING ImageName;
    KPRIORITY BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    ULONG_PTR UniqueProcessKey;
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage;
    SIZE_T QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    LARGE_INTEGER ReadOperationCount;
    LARGE_INTEGER WriteOperationCount;
    LARGE_INTEGER OtherOperationCount;
    LARGE_INTEGER ReadTransferCount;
    LARGE_INTEGER WriteTransferCount;
    LARGE_INTEGER OtherTransferCount;
} KR_SYSTEM_PROCESS_INFORMATION;
typedef KR_SYSTEM_PROCESS_INFORMATION *PKR_SYSTEM_PROCESS_INFORMATION;

NTSYSAPI NTSTATUS NTAPI ZwQuerySystemInformation(
    _In_ ULONG SystemInformationClass,
    _Out_writes_bytes_opt_(SystemInformationLength) PVOID SystemInformation,
    _In_ ULONG SystemInformationLength,
    _Out_opt_ PULONG ReturnLength
);

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD KrEverythingUnload;
_Dispatch_type_(IRP_MJ_CREATE) DRIVER_DISPATCH KrEverythingCreateClose;
_Dispatch_type_(IRP_MJ_CLOSE) DRIVER_DISPATCH KrEverythingCreateClose;
_Dispatch_type_(IRP_MJ_CLEANUP) DRIVER_DISPATCH KrEverythingCreateClose;
_Dispatch_type_(IRP_MJ_DEVICE_CONTROL) DRIVER_DISPATCH KrEverythingDeviceControl;
DRIVER_DISPATCH KrEverythingUnsupported;

static VOID KrEverythingCompleteRequest(
    _Inout_ PIRP Irp,
    _In_ NTSTATUS Status,
    _In_ ULONG_PTR Information
)
{
    Irp->IoStatus.Status = Status;
    Irp->IoStatus.Information = Information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
}

static VOID KrEverythingFillImageName(
    _Out_writes_(KREVERYTHING_MAX_IMAGE_NAME) PWCHAR Destination,
    _In_opt_ PUNICODE_STRING Source,
    _In_ ULONGLONG ProcessId
)
{
    USHORT charsToCopy;

    RtlZeroMemory(Destination, sizeof(WCHAR) * KREVERYTHING_MAX_IMAGE_NAME);

    if (Source != NULL && Source->Buffer != NULL && Source->Length != 0) {
        charsToCopy = Source->Length / sizeof(WCHAR);
        if (charsToCopy >= KREVERYTHING_MAX_IMAGE_NAME) {
            charsToCopy = KREVERYTHING_MAX_IMAGE_NAME - 1;
        }

        RtlCopyMemory(Destination, Source->Buffer, charsToCopy * sizeof(WCHAR));
        Destination[charsToCopy] = L'\0';
        return;
    }

    if (ProcessId == 0) {
        RtlCopyMemory(Destination, L"System Idle Process", sizeof(L"System Idle Process"));
        return;
    }

    if (ProcessId == 4) {
        RtlCopyMemory(Destination, L"System", sizeof(L"System"));
        return;
    }

    RtlCopyMemory(Destination, L"<unknown>", sizeof(L"<unknown>"));
}

static NTSTATUS KrEverythingQueryProcessSnapshot(
    _Outptr_result_bytebuffer_(*BufferSize) PVOID *Buffer,
    _Out_ PULONG BufferSize
)
{
    NTSTATUS status;
    PVOID buffer;
    ULONG querySize;
    ULONG returnLength;

    *Buffer = NULL;
    *BufferSize = 0;
    querySize = KR_INITIAL_QUERY_SIZE;

    for (;;) {
        buffer = ExAllocatePool2(POOL_FLAG_PAGED, querySize, KR_TAG);
        if (buffer == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        returnLength = 0;
        status = ZwQuerySystemInformation(5, buffer, querySize, &returnLength);
        if (status == STATUS_INFO_LENGTH_MISMATCH) {
            ExFreePoolWithTag(buffer, KR_TAG);

            if (returnLength > querySize) {
                querySize = returnLength + 4096;
            } else {
                querySize *= 2;
            }

            if (querySize > 16 * 1024 * 1024) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            continue;
        }

        if (!NT_SUCCESS(status)) {
            ExFreePoolWithTag(buffer, KR_TAG);
            return status;
        }

        *Buffer = buffer;
        *BufferSize = querySize;
        return STATUS_SUCCESS;
    }
}

static NTSTATUS KrEverythingHandleGetProcessList(
    _Inout_ PIRP Irp,
    _In_ ULONG OutputBufferLength
)
{
    NTSTATUS status;
    PVOID snapshotBuffer;
    ULONG snapshotBufferSize;
    PKR_SYSTEM_PROCESS_INFORMATION processInfo;
    KREVERYTHING_PROCESS_LIST_HEADER *header;
    KREVERYTHING_PROCESS_ENTRY *entries;
    ULONG entryCount;
    ULONG bytesRequired;
    ULONG bytesAvailable;
    ULONG copiedCount;

    PAGED_CODE();

    if (OutputBufferLength < sizeof(KREVERYTHING_PROCESS_LIST_HEADER)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    status = KrEverythingQueryProcessSnapshot(&snapshotBuffer, &snapshotBufferSize);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    UNREFERENCED_PARAMETER(snapshotBufferSize);

    entryCount = 0;
    processInfo = (PKR_SYSTEM_PROCESS_INFORMATION)snapshotBuffer;

    for (;;) {
        entryCount++;
        if (processInfo->NextEntryOffset == 0) {
            break;
        }

        processInfo = (PKR_SYSTEM_PROCESS_INFORMATION)
            ((PUCHAR)processInfo + processInfo->NextEntryOffset);
    }

    bytesRequired = sizeof(KREVERYTHING_PROCESS_LIST_HEADER) +
        (entryCount * sizeof(KREVERYTHING_PROCESS_ENTRY));

    header = (KREVERYTHING_PROCESS_LIST_HEADER *)Irp->AssociatedIrp.SystemBuffer;
    header->Version = KREVERYTHING_PROTOCOL_VERSION;
    header->EntryCount = 0;
    header->BytesRequired = bytesRequired;
    header->Reserved = 0;

    if (OutputBufferLength < bytesRequired) {
        ExFreePoolWithTag(snapshotBuffer, KR_TAG);
        return STATUS_BUFFER_TOO_SMALL;
    }

    bytesAvailable = OutputBufferLength - sizeof(KREVERYTHING_PROCESS_LIST_HEADER);
    entries = (KREVERYTHING_PROCESS_ENTRY *)(header + 1);
    copiedCount = 0;
    processInfo = (PKR_SYSTEM_PROCESS_INFORMATION)snapshotBuffer;

    for (;;) {
        KREVERYTHING_PROCESS_ENTRY *entry;
        ULONGLONG processId;

        if (bytesAvailable < sizeof(KREVERYTHING_PROCESS_ENTRY)) {
            break;
        }

        entry = &entries[copiedCount];
        RtlZeroMemory(entry, sizeof(*entry));

        processId = (ULONGLONG)(ULONG_PTR)processInfo->UniqueProcessId;
        entry->ProcessId = processId;
        entry->ParentProcessId =
            (ULONGLONG)(ULONG_PTR)processInfo->InheritedFromUniqueProcessId;
        entry->ThreadCount = processInfo->NumberOfThreads;
        entry->HandleCount = processInfo->HandleCount;
        entry->SessionId = processInfo->SessionId;
        entry->BasePriority = processInfo->BasePriority;
        entry->PageFaultCount = processInfo->PageFaultCount;
        entry->CreateTime = (ULONGLONG)processInfo->CreateTime.QuadPart;
        entry->UserTime = (ULONGLONG)processInfo->UserTime.QuadPart;
        entry->KernelTime = (ULONGLONG)processInfo->KernelTime.QuadPart;
        entry->PeakWorkingSetSize = (ULONGLONG)processInfo->PeakWorkingSetSize;
        entry->WorkingSetSize = (ULONGLONG)processInfo->WorkingSetSize;
        entry->PeakVirtualSize = (ULONGLONG)processInfo->PeakVirtualSize;
        entry->VirtualSize = (ULONGLONG)processInfo->VirtualSize;
        entry->PrivatePageCount = (ULONGLONG)processInfo->PrivatePageCount;
        KrEverythingFillImageName(entry->ImageName, &processInfo->ImageName, processId);

        copiedCount++;
        bytesAvailable -= sizeof(KREVERYTHING_PROCESS_ENTRY);

        if (processInfo->NextEntryOffset == 0) {
            break;
        }

        processInfo = (PKR_SYSTEM_PROCESS_INFORMATION)
            ((PUCHAR)processInfo + processInfo->NextEntryOffset);
    }

    header->EntryCount = copiedCount;

    ExFreePoolWithTag(snapshotBuffer, KR_TAG);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS KrEverythingCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PAGED_CODE();
    UNREFERENCED_PARAMETER(DeviceObject);

    KrEverythingCompleteRequest(Irp, STATUS_SUCCESS, 0);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS KrEverythingUnsupported(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    PAGED_CODE();
    UNREFERENCED_PARAMETER(DeviceObject);

    KrEverythingCompleteRequest(Irp, STATUS_INVALID_DEVICE_REQUEST, 0);
    return STATUS_INVALID_DEVICE_REQUEST;
}

_Use_decl_annotations_
NTSTATUS KrEverythingDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
    NTSTATUS status;
    ULONG_PTR information;
    PIO_STACK_LOCATION stackLocation;

    PAGED_CODE();
    UNREFERENCED_PARAMETER(DeviceObject);

    information = 0;
    stackLocation = IoGetCurrentIrpStackLocation(Irp);

    switch (stackLocation->Parameters.DeviceIoControl.IoControlCode) {
    case IOCTL_KREVERYTHING_GET_PROCESS_LIST:
        status = KrEverythingHandleGetProcessList(
            Irp,
            stackLocation->Parameters.DeviceIoControl.OutputBufferLength);

        if (status == STATUS_SUCCESS) {
            information =
                ((KREVERYTHING_PROCESS_LIST_HEADER *)Irp->AssociatedIrp.SystemBuffer)->BytesRequired;
        }
        break;

    default:
        status = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    KrEverythingCompleteRequest(Irp, status, information);
    return status;
}

_Use_decl_annotations_
VOID KrEverythingUnload(PDRIVER_OBJECT DriverObject)
{
    UNICODE_STRING symbolicLinkName;

    PAGED_CODE();

    RtlInitUnicodeString(&symbolicLinkName, KREVERYTHING_DOS_DEVICE_NAME);
    IoDeleteSymbolicLink(&symbolicLinkName);

    if (DriverObject->DeviceObject != NULL) {
        IoDeleteDevice(DriverObject->DeviceObject);
    }

    DbgPrintEx(
        DPFLTR_IHVDRIVER_ID,
        DPFLTR_INFO_LEVEL,
        "KrEverything: unload\r\n");
}

_Use_decl_annotations_
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    NTSTATUS status;
    PDEVICE_OBJECT deviceObject;
    UNICODE_STRING deviceName;
    UNICODE_STRING symbolicLinkName;
    ULONG index;

    UNREFERENCED_PARAMETER(RegistryPath);

    RtlInitUnicodeString(&deviceName, KREVERYTHING_KERNEL_DEVICE_NAME);
    RtlInitUnicodeString(&symbolicLinkName, KREVERYTHING_DOS_DEVICE_NAME);

    status = IoCreateDevice(
        DriverObject,
        0,
        &deviceName,
        KREVERYTHING_DEVICE_TYPE,
        0,
        FALSE,
        &deviceObject);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    deviceObject->Flags |= DO_BUFFERED_IO;

    DriverObject->DriverUnload = KrEverythingUnload;
    for (index = 0; index <= IRP_MJ_MAXIMUM_FUNCTION; index++) {
        DriverObject->MajorFunction[index] = KrEverythingUnsupported;
    }

    DriverObject->MajorFunction[IRP_MJ_CREATE] = KrEverythingCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = KrEverythingCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLEANUP] = KrEverythingCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = KrEverythingDeviceControl;

    status = IoCreateSymbolicLink(&symbolicLinkName, &deviceName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(deviceObject);
        return status;
    }

    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    DbgPrintEx(
        DPFLTR_IHVDRIVER_ID,
        DPFLTR_INFO_LEVEL,
        "KrEverything: Hello from kernel mode.\r\n");

    return STATUS_SUCCESS;
}
