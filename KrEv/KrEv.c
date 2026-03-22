#include <ntddk.h>
#include "..\shared\KrEvShared.h"

DRIVER_UNLOAD KrEvUnload;
DRIVER_DISPATCH KrEvCreateClose;
DRIVER_DISPATCH KrEvDeviceControl;

static NTSTATUS
KrEvWriteMessage(
    _Out_writes_bytes_(OutputBufferLength) KREV_MESSAGE_RESPONSE* Response,
    _In_ ULONG OutputBufferLength,
    _Out_ ULONG_PTR* Information
    )
{
    static const WCHAR kMessage[] = L"Hello from the KrEv kernel driver.";
    SIZE_T bytesToCopy;

    if (OutputBufferLength < sizeof(KREV_MESSAGE_RESPONSE)) {
        return STATUS_BUFFER_TOO_SMALL;
    }

    RtlZeroMemory(Response, sizeof(KREV_MESSAGE_RESPONSE));
    Response->Version = 1;
    Response->MessageLength = (ULONG)(RTL_NUMBER_OF(kMessage) - 1);

    bytesToCopy = sizeof(kMessage);
    RtlCopyMemory(Response->Message, kMessage, bytesToCopy);

    *Information = sizeof(KREV_MESSAGE_RESPONSE);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
KrEvCreateClose(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

_Use_decl_annotations_
NTSTATUS
KrEvDeviceControl(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
    )
{
    PIO_STACK_LOCATION stack;
    NTSTATUS status;
    ULONG_PTR information;

    UNREFERENCED_PARAMETER(DeviceObject);

    stack = IoGetCurrentIrpStackLocation(Irp);
    status = STATUS_INVALID_DEVICE_REQUEST;
    information = 0;

    if (stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_KREV_GET_MESSAGE) {
        status = KrEvWriteMessage(
            (KREV_MESSAGE_RESPONSE*)Irp->AssociatedIrp.SystemBuffer,
            stack->Parameters.DeviceIoControl.OutputBufferLength,
            &information);
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

_Use_decl_annotations_
VOID
KrEvUnload(
    PDRIVER_OBJECT DriverObject
    )
{
    UNICODE_STRING dosDeviceName;

    RtlInitUnicodeString(&dosDeviceName, KREV_DOS_DEVICE_NAME);
    IoDeleteSymbolicLink(&dosDeviceName);

    if (DriverObject->DeviceObject != NULL) {
        IoDeleteDevice(DriverObject->DeviceObject);
    }

    KdPrint(("KrEv: driver unloaded.\n"));
}

_Use_decl_annotations_
NTSTATUS
DriverEntry(
    PDRIVER_OBJECT DriverObject,
    PUNICODE_STRING RegistryPath
    )
{
    UNICODE_STRING deviceName;
    UNICODE_STRING dosDeviceName;
    PDEVICE_OBJECT deviceObject;
    NTSTATUS status;

    UNREFERENCED_PARAMETER(RegistryPath);

    RtlInitUnicodeString(&deviceName, KREV_DEVICE_NAME);
    RtlInitUnicodeString(&dosDeviceName, KREV_DOS_DEVICE_NAME);
    deviceObject = NULL;

    status = IoCreateDevice(
        DriverObject,
        0,
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject);
    if (!NT_SUCCESS(status)) {
        KdPrint(("KrEv: IoCreateDevice failed (0x%08X).\n", status));
        return status;
    }

    deviceObject->Flags |= DO_BUFFERED_IO;

    status = IoCreateSymbolicLink(&dosDeviceName, &deviceName);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(deviceObject);
        KdPrint(("KrEv: IoCreateSymbolicLink failed (0x%08X).\n", status));
        return status;
    }

    DriverObject->DriverUnload = KrEvUnload;
    DriverObject->MajorFunction[IRP_MJ_CREATE] = KrEvCreateClose;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = KrEvCreateClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = KrEvDeviceControl;

    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    KdPrint(("KrEv: device ready for user-mode communication.\n"));
    return STATUS_SUCCESS;
}
