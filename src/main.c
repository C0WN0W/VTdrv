#include "driver.h"
#include "hypervisor.h"

ULONG g_TargetProcessId = 0;
PEPROCESS g_TargetProcess = NULL;

NTSTATUS DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    NTSTATUS status;
    UNICODE_STRING deviceName;
    UNICODE_STRING symbolicLink;
    PDEVICE_OBJECT deviceObject = NULL;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint("[HKDrv] DriverEntry called\n");
    DbgPrint("[HKDrv] Driver Version: %ws\n", DRIVER_VERSION);

    RtlInitUnicodeString(&deviceName, DEVICE_NAME);
    RtlInitUnicodeString(&symbolicLink, SYMBOLIC_LINK_NAME);

    status = IoCreateDevice(
        DriverObject,
        0,
        &deviceName,
        FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN,
        FALSE,
        &deviceObject
    );

    if (!NT_SUCCESS(status)) {
        DbgPrint("[HKDrv] Failed to create device: 0x%X\n", status);
        return status;
    }

    DbgPrint("[HKDrv] Device created: %wZ\n", &deviceName);

    status = IoCreateSymbolicLink(&symbolicLink, &deviceName);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[HKDrv] Failed to create symbolic link: 0x%X\n", status);
        IoDeleteDevice(deviceObject);
        return status;
    }

    DbgPrint("[HKDrv] Symbolic link created: %wZ\n", &symbolicLink);

    deviceObject->Flags |= DO_BUFFERED_IO;
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    DriverObject->MajorFunction[IRP_MJ_CREATE] = DeviceCreate;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DeviceClose;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceControl;
    DriverObject->DriverUnload = DriverUnload;

    DbgPrint("[HKDrv] Driver loaded successfully\n");

    DbgPrint("[HKDrv] ========================================\n");
    DbgPrint("[HKDrv] Pre-flight check: Virtualization support\n");
    DbgPrint("[HKDrv] ========================================\n");

    if (!HvCheckVmxSupport()) {
        DbgPrint("[HKDrv] ✗ CRITICAL: Virtualization not supported!\n");
        DbgPrint("[HKDrv] ✗ Reason: CPU does not support VT-x or VT-x is disabled\n");
        DbgPrint("[HKDrv] ========================================\n");
        DbgPrint("[HKDrv] Possible causes:\n");
        DbgPrint("[HKDrv]   1. CPU does not support Intel VT-x\n");
        DbgPrint("[HKDrv]   2. VT-x is disabled in BIOS/UEFI\n");
        DbgPrint("[HKDrv]   3. Hyper-V is enabled (conflicts with our hypervisor)\n");
        DbgPrint("[HKDrv]   4. IA32_FEATURE_CONTROL MSR is locked incorrectly\n");
        DbgPrint("[HKDrv] ========================================\n");
        DbgPrint("[HKDrv] Solutions:\n");
        DbgPrint("[HKDrv]   1. Check CPU specifications for VT-x support\n");
        DbgPrint("[HKDrv]   2. Enable VT-x in BIOS/UEFI settings\n");
        DbgPrint("[HKDrv]   3. Disable Hyper-V: bcdedit /set hypervisorlaunchtype off\n");
        DbgPrint("[HKDrv]   4. Reboot and try again\n");
        DbgPrint("[HKDrv] ========================================\n");
        DbgPrint("[HKDrv] DRIVER LOAD ABORTED!\n");
        DbgPrint("[HKDrv] ========================================\n");

        IoDeleteSymbolicLink(&symbolicLink);
        IoDeleteDevice(deviceObject);

        return STATUS_NOT_SUPPORTED;
    }

    DbgPrint("[HKDrv] ✓ Virtualization support verified\n");
    DbgPrint("[HKDrv] ✓ CPU supports Intel VT-x\n");
    DbgPrint("[HKDrv] ✓ VT-x is enabled in BIOS/UEFI\n");
    DbgPrint("[HKDrv] ✓ IA32_FEATURE_CONTROL MSR is configured correctly\n");
    DbgPrint("[HKDrv] ========================================\n");

    DbgPrint("[HKDrv] ========================================\n");
    DbgPrint("[HKDrv] NOTE: Hypervisor NOT auto-started\n");
    DbgPrint("[HKDrv] Use IOCTL_START_HYPERVISOR to start manually\n");
    DbgPrint("[HKDrv] ========================================\n");

    // DISABLED: Auto-start causes immediate BSOD during driver load
    // Use IOCTL_START_HYPERVISOR to start hypervisor after driver is loaded
    /*
    NTSTATUS hvStatus = HvInitialize();
    if (NT_SUCCESS(hvStatus)) {
        DbgPrint("[HKDrv] ✓ Hypervisor started successfully!\n");
    } else {
        DbgPrint("[HKDrv] ✗ Failed to start Hypervisor: 0x%X\n", hvStatus);
        IoDeleteSymbolicLink(&symbolicLink);
        IoDeleteDevice(deviceObject);
        return hvStatus;
    }
    */

    return STATUS_SUCCESS;
}

VOID DriverUnload(
    _In_ PDRIVER_OBJECT DriverObject
)
{
    UNICODE_STRING symbolicLink;

    DbgPrint("[HKDrv] DriverUnload called\n");

    if (HvIsActive()) {
        DbgPrint("[HKDrv] Auto-stopping Hypervisor...\n");
        DbgPrint("[HKDrv] ========================================\n");

        HvTerminate();

        DbgPrint("[HKDrv] ✓ Hypervisor stopped\n");
        DbgPrint("[HKDrv] ✓ System returned to normal mode\n");
        DbgPrint("[HKDrv] ✓ All CPU cores devirtualized\n");
        DbgPrint("[HKDrv] ========================================\n");
    } else {
        DbgPrint("[HKDrv] Hypervisor was not active\n");
    }

    RtlInitUnicodeString(&symbolicLink, SYMBOLIC_LINK_NAME);
    IoDeleteSymbolicLink(&symbolicLink);
    DbgPrint("[HKDrv] Symbolic link deleted\n");

    if (DriverObject->DeviceObject) {
        IoDeleteDevice(DriverObject->DeviceObject);
        DbgPrint("[HKDrv] Device deleted\n");
    }

    if (g_TargetProcess) {
        ObDereferenceObject(g_TargetProcess);
        g_TargetProcess = NULL;
        g_TargetProcessId = 0;
        DbgPrint("[HKDrv] Target process released\n");
    }

    DbgPrint("[HKDrv] Driver unloaded\n");
}

NTSTATUS DeviceCreate(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    DbgPrint("[HKDrv] Device opened\n");

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS DeviceClose(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    UNREFERENCED_PARAMETER(DeviceObject);

    DbgPrint("[HKDrv] Device closed\n");

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

NTSTATUS DeviceControl(
    _In_ PDEVICE_OBJECT DeviceObject,
    _In_ PIRP Irp
)
{
    NTSTATUS status = STATUS_SUCCESS;
    PIO_STACK_LOCATION irpStack;
    ULONG controlCode;
    PVOID inputBuffer;
    PVOID outputBuffer;
    ULONG inputBufferLength;
    ULONG outputBufferLength;
    ULONG_PTR information = 0;

    UNREFERENCED_PARAMETER(DeviceObject);

    irpStack = IoGetCurrentIrpStackLocation(Irp);
    controlCode = irpStack->Parameters.DeviceIoControl.IoControlCode;
    inputBuffer = Irp->AssociatedIrp.SystemBuffer;
    outputBuffer = Irp->AssociatedIrp.SystemBuffer;
    inputBufferLength = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    outputBufferLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    DbgPrint("[HKDrv] DeviceControl: IOCTL=0x%X\n", controlCode);

    switch (controlCode) {
        case IOCTL_READ_MEMORY: {
            DbgPrint("[HKDrv] IOCTL_READ_MEMORY\n");

            if (inputBufferLength >= sizeof(MEMORY_REQUEST) &&
                outputBufferLength >= sizeof(MEMORY_REQUEST)) {

                PMEMORY_REQUEST request = (PMEMORY_REQUEST)inputBuffer;

                DbgPrint("[HKDrv] Read: PID=%d, Addr=0x%p, Size=%d\n",
                    request->ProcessId, request->Address, request->Size);

                PVOID tempBuffer = ExAllocatePoolWithTag(NonPagedPool, request->Size, 'HKDR');
                if (tempBuffer) {
                    status = ReadProcessMemory(
                        request->ProcessId,
                        request->Address,
                        tempBuffer,
                        request->Size
                    );

                    if (NT_SUCCESS(status)) {

                        RtlCopyMemory(request->Buffer, tempBuffer, request->Size);
                        information = sizeof(MEMORY_REQUEST);
                        DbgPrint("[HKDrv] Read successful\n");
                    } else {
                        DbgPrint("[HKDrv] Read failed: 0x%X\n", status);
                    }

                    ExFreePoolWithTag(tempBuffer, 'HKDR');
                } else {
                    status = STATUS_INSUFFICIENT_RESOURCES;
                    DbgPrint("[HKDrv] Failed to allocate buffer\n");
                }
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
                DbgPrint("[HKDrv] Buffer too small\n");
            }
            break;
        }

        case IOCTL_WRITE_MEMORY: {
            DbgPrint("[HKDrv] IOCTL_WRITE_MEMORY\n");

            if (inputBufferLength >= sizeof(MEMORY_REQUEST)) {
                PMEMORY_REQUEST request = (PMEMORY_REQUEST)inputBuffer;

                DbgPrint("[HKDrv] Write: PID=%d, Addr=0x%p, Size=%d\n",
                    request->ProcessId, request->Address, request->Size);

                status = WriteProcessMemory(
                    request->ProcessId,
                    request->Address,
                    request->Buffer,
                    request->Size
                );

                if (NT_SUCCESS(status)) {
                    information = request->Size;
                    DbgPrint("[HKDrv] Write successful\n");
                } else {
                    DbgPrint("[HKDrv] Write failed: 0x%X\n", status);
                }
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
                DbgPrint("[HKDrv] Buffer too small\n");
            }
            break;
        }

        case IOCTL_GET_MODULE_BASE: {
            DbgPrint("[HKDrv] IOCTL_GET_MODULE_BASE\n");

            if (inputBufferLength >= sizeof(MODULE_REQUEST) &&
                outputBufferLength >= sizeof(MODULE_REQUEST)) {

                PMODULE_REQUEST request = (PMODULE_REQUEST)inputBuffer;
                PMODULE_REQUEST response = (PMODULE_REQUEST)outputBuffer;

                DbgPrint("[HKDrv] GetModule: PID=%d, Module=%ws\n",
                    request->ProcessId, request->ModuleName);

                status = GetModuleBase(
                    request->ProcessId,
                    request->ModuleName,
                    &response->ModuleBase
                );

                if (NT_SUCCESS(status)) {
                    information = sizeof(MODULE_REQUEST);
                    DbgPrint("[HKDrv] Module base: 0x%p\n", response->ModuleBase);
                } else {
                    DbgPrint("[HKDrv] GetModule failed: 0x%X\n", status);
                }
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
                DbgPrint("[HKDrv] Buffer too small\n");
            }
            break;
        }

        case IOCTL_SET_PROCESS: {
            DbgPrint("[HKDrv] IOCTL_SET_PROCESS\n");

            if (inputBufferLength >= sizeof(PROCESS_REQUEST)) {
                PPROCESS_REQUEST request = (PPROCESS_REQUEST)inputBuffer;

                DbgPrint("[HKDrv] SetProcess: PID=%d\n", request->ProcessId);

                status = SetTargetProcess(request->ProcessId);

                if (NT_SUCCESS(status)) {
                    DbgPrint("[HKDrv] Process set successfully\n");
                } else {
                    DbgPrint("[HKDrv] SetProcess failed: 0x%X\n", status);
                }
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
                DbgPrint("[HKDrv] Buffer too small\n");
            }
            break;
        }

        case IOCTL_START_HYPERVISOR: {
            DbgPrint("[HKDrv] IOCTL_START_HYPERVISOR\n");

            if (HvIsActive()) {
                DbgPrint("[HKDrv] Hypervisor already active\n");
                status = STATUS_ALREADY_INITIALIZED;
            } else {
                status = HvInitialize();
                if (NT_SUCCESS(status)) {
                    DbgPrint("[HKDrv] Hypervisor started successfully\n");
                } else {
                    DbgPrint("[HKDrv] Failed to start hypervisor: 0x%X\n", status);
                }
            }
            break;
        }

        case IOCTL_STOP_HYPERVISOR: {
            DbgPrint("[HKDrv] IOCTL_STOP_HYPERVISOR\n");

            if (!HvIsActive()) {
                DbgPrint("[HKDrv] Hypervisor not active\n");
                status = STATUS_NOT_FOUND;
            } else {
                HvTerminate();
                DbgPrint("[HKDrv] Hypervisor stopped\n");
                status = STATUS_SUCCESS;
            }
            break;
        }

        case IOCTL_HV_STATUS: {
            DbgPrint("[HKDrv] IOCTL_HV_STATUS\n");

            if (outputBufferLength >= sizeof(BOOLEAN)) {
                PBOOLEAN hvStatus = (PBOOLEAN)outputBuffer;
                *hvStatus = HvIsActive();
                information = sizeof(BOOLEAN);
                status = STATUS_SUCCESS;
                DbgPrint("[HKDrv] Hypervisor status: %s\n", *hvStatus ? "Active" : "Inactive");
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }

        default:
            status = STATUS_INVALID_DEVICE_REQUEST;
            DbgPrint("[HKDrv] Invalid IOCTL: 0x%X\n", controlCode);
            break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = information;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;
}
