#pragma once
#include <ntifs.h>

#define DRIVER_NAME L"HKDrv"
#define DRIVER_VERSION L"1.0"

#define DEVICE_NAME L"\\Device\\HKDrv"
#define SYMBOLIC_LINK_NAME L"\\DosDevices\\HKDrv"

#define IOCTL_BASE 0x8000

#define IOCTL_READ_MEMORY       CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BASE + 0x01, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WRITE_MEMORY      CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BASE + 0x02, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_MODULE_BASE   CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BASE + 0x03, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SET_PROCESS       CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BASE + 0x04, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_START_HYPERVISOR  CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BASE + 0x10, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_STOP_HYPERVISOR   CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BASE + 0x11, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_HV_STATUS         CTL_CODE(FILE_DEVICE_UNKNOWN, IOCTL_BASE + 0x12, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _MEMORY_REQUEST {
    ULONG ProcessId;
    PVOID Address;
    PVOID Buffer;
    ULONG Size;
} MEMORY_REQUEST, *PMEMORY_REQUEST;

typedef struct _MODULE_REQUEST {
    ULONG ProcessId;
    WCHAR ModuleName[260];
    PVOID ModuleBase;
} MODULE_REQUEST, *PMODULE_REQUEST;

typedef struct _PROCESS_REQUEST {
    ULONG ProcessId;
} PROCESS_REQUEST, *PPROCESS_REQUEST;

typedef struct _PEB_LDR_DATA {
    ULONG Length;
    BOOLEAN Initialized;
    PVOID SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
    LIST_ENTRY InMemoryOrderModuleList;
    LIST_ENTRY InInitializationOrderModuleList;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

typedef struct _PEB {
    BOOLEAN InheritedAddressSpace;
    BOOLEAN ReadImageFileExecOptions;
    BOOLEAN BeingDebugged;
    BOOLEAN SpareBool;
    PVOID Mutant;
    PVOID ImageBaseAddress;
    PPEB_LDR_DATA Ldr;

} PEB, *PPEB;

typedef struct _LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    PVOID DllBase;
    PVOID EntryPoint;
    ULONG SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;

} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

extern ULONG g_TargetProcessId;
extern PEPROCESS g_TargetProcess;

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath);
VOID DriverUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS DeviceCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DeviceClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS DeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS ReadProcessMemory(ULONG ProcessId, PVOID Address, PVOID Buffer, ULONG Size);
NTSTATUS WriteProcessMemory(ULONG ProcessId, PVOID Address, PVOID Buffer, ULONG Size);
NTSTATUS GetModuleBase(ULONG ProcessId, PWCHAR ModuleName, PVOID* ModuleBase);
NTSTATUS SetTargetProcess(ULONG ProcessId);

NTSTATUS ReadVirtualMemory(PEPROCESS Process, PVOID Address, PVOID Buffer, SIZE_T Size);
NTSTATUS WriteVirtualMemory(PEPROCESS Process, PVOID Address, PVOID Buffer, SIZE_T Size);
PVOID GetProcessPeb(PEPROCESS Process);

NTKERNELAPI
NTSTATUS
NTAPI
MmCopyVirtualMemory(
    IN PEPROCESS FromProcess,
    IN PVOID FromAddress,
    IN PEPROCESS ToProcess,
    OUT PVOID ToAddress,
    IN SIZE_T BufferSize,
    IN KPROCESSOR_MODE PreviousMode,
    OUT PSIZE_T NumberOfBytesCopied
);

NTKERNELAPI
VOID
NTAPI
KeStackAttachProcess(
    IN PEPROCESS Process,
    OUT PKAPC_STATE ApcState
);

NTKERNELAPI
VOID
NTAPI
KeUnstackDetachProcess(
    IN PKAPC_STATE ApcState
);
