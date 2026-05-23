#include "driver.h"

NTSTATUS SetTargetProcess(ULONG ProcessId)
{
    NTSTATUS status;
    PEPROCESS process = NULL;

    if (g_TargetProcess) {
        ObDereferenceObject(g_TargetProcess);
        g_TargetProcess = NULL;
        g_TargetProcessId = 0;
    }

    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[HKDrv] Failed to lookup process %d: 0x%X\n", ProcessId, status);
        return status;
    }

    g_TargetProcess = process;
    g_TargetProcessId = ProcessId;

    DbgPrint("[HKDrv] Target process set to PID %d\n", ProcessId);

    return STATUS_SUCCESS;
}

NTSTATUS ReadProcessMemory(
    ULONG ProcessId,
    PVOID Address,
    PVOID Buffer,
    ULONG Size
)
{
    NTSTATUS status;
    PEPROCESS process = NULL;

    if (!Address || !Buffer || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (g_TargetProcessId == ProcessId && g_TargetProcess) {
        process = g_TargetProcess;
        ObReferenceObject(process);
    } else {
        status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &process);
        if (!NT_SUCCESS(status)) {
            return status;
        }
    }

    status = ReadVirtualMemory(process, Address, Buffer, Size);

    ObDereferenceObject(process);

    return status;
}

NTSTATUS WriteProcessMemory(
    ULONG ProcessId,
    PVOID Address,
    PVOID Buffer,
    ULONG Size
)
{
    NTSTATUS status;
    PEPROCESS process = NULL;

    if (!Address || !Buffer || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    if (g_TargetProcessId == ProcessId && g_TargetProcess) {
        process = g_TargetProcess;
        ObReferenceObject(process);
    } else {
        status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &process);
        if (!NT_SUCCESS(status)) {
            return status;
        }
    }

    status = WriteVirtualMemory(process, Address, Buffer, Size);

    ObDereferenceObject(process);

    return status;
}

NTSTATUS ReadVirtualMemory(
    PEPROCESS Process,
    PVOID Address,
    PVOID Buffer,
    SIZE_T Size
)
{
    NTSTATUS status = STATUS_SUCCESS;
    SIZE_T bytesRead = 0;

    if (!Process || !Address || !Buffer || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    status = MmCopyVirtualMemory(
        Process,
        Address,
        PsGetCurrentProcess(),
        Buffer,
        Size,
        KernelMode,
        &bytesRead
    );

    return status;
}

NTSTATUS WriteVirtualMemory(
    PEPROCESS Process,
    PVOID Address,
    PVOID Buffer,
    SIZE_T Size
)
{
    NTSTATUS status = STATUS_SUCCESS;
    SIZE_T bytesWritten = 0;

    if (!Process || !Address || !Buffer || Size == 0) {
        return STATUS_INVALID_PARAMETER;
    }

    status = MmCopyVirtualMemory(
        PsGetCurrentProcess(),
        Buffer,
        Process,
        Address,
        Size,
        KernelMode,
        &bytesWritten
    );

    return status;
}

PVOID GetProcessPeb(PEPROCESS Process)
{
    return *(PVOID*)((ULONG_PTR)Process + 0x550);
}

NTSTATUS GetModuleBase(
    ULONG ProcessId,
    PWCHAR ModuleName,
    PVOID* ModuleBase
)
{
    NTSTATUS status;
    PEPROCESS process = NULL;
    KAPC_STATE apcState;
    PPEB peb = NULL;
    PPEB_LDR_DATA ldr = NULL;
    PLIST_ENTRY listHead = NULL;
    PLIST_ENTRY listEntry = NULL;
    PLDR_DATA_TABLE_ENTRY ldrEntry = NULL;
    UNICODE_STRING targetModule;
    BOOLEAN found = FALSE;

    if (!ModuleName || !ModuleBase) {
        return STATUS_INVALID_PARAMETER;
    }

    *ModuleBase = NULL;

    RtlInitUnicodeString(&targetModule, ModuleName);

    status = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)ProcessId, &process);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    __try {
        KeStackAttachProcess(process, &apcState);

        peb = (PPEB)GetProcessPeb(process);
        if (!peb || !MmIsAddressValid(peb)) {
            status = STATUS_UNSUCCESSFUL;
            __leave;
        }

        ldr = (PPEB_LDR_DATA)peb->Ldr;
        if (!ldr || !MmIsAddressValid(ldr)) {
            status = STATUS_UNSUCCESSFUL;
            __leave;
        }

        listHead = &ldr->InLoadOrderModuleList;
        listEntry = listHead->Flink;

        while (listEntry != listHead && MmIsAddressValid(listEntry)) {
            ldrEntry = CONTAINING_RECORD(listEntry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

            if (MmIsAddressValid(&ldrEntry->BaseDllName) &&
                MmIsAddressValid(ldrEntry->BaseDllName.Buffer)) {

                if (RtlEqualUnicodeString(&ldrEntry->BaseDllName, &targetModule, TRUE)) {
                    *ModuleBase = ldrEntry->DllBase;
                    found = TRUE;
                    status = STATUS_SUCCESS;
                    break;
                }
            }

            listEntry = listEntry->Flink;
        }

        if (!found) {
            status = STATUS_NOT_FOUND;
        }

        KeUnstackDetachProcess(&apcState);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (apcState.Process != NULL) {
            KeUnstackDetachProcess(&apcState);
        }
        status = GetExceptionCode();
    }

    ObDereferenceObject(process);

    return status;
}
