#include "hypervisor.h"
#include "ept.h"

VOID __fastcall HvVmExitHandler(PGUEST_REGISTERS guestRegisters)
{

    if (g_HvShutdownRequested) {
        ULONG currentCpu = KeGetCurrentProcessorNumber();
        if (currentCpu < g_HypervisorData.ProcessorCount) {
            PVCPU_DATA vcpu = &g_HypervisorData.VcpuData[currentCpu];
            vcpu->ShutdownRequested = TRUE;
            vcpu->IsVirtualized = FALSE;

            __vmx_off();

            ULONG_PTR cr4 = __readcr4();
            cr4 &= ~CR4_VMXE;
            __writecr4(cr4);

            DbgPrint("[HV] CPU %d devirtualized on shutdown\n", currentCpu);

            RtlRestoreContext(&vcpu->LaunchContext, NULL);
        }
    }

    ULONG_PTR exitReason = 0;
    __vmx_vmread(VM_EXIT_REASON, &exitReason);

    exitReason &= 0xFFFF;

    DbgPrint("[HV] VM-Exit: Reason=%d, RIP=0x%llx\n", exitReason, HvGetGuestRip());

    switch (exitReason) {
        case EXIT_REASON_CPUID:
            HvHandleCpuid(guestRegisters);
            break;

        case EXIT_REASON_VMCALL:
            HvHandleVmcall(guestRegisters);
            break;

        case EXIT_REASON_MSR_READ:
            HvHandleMsrRead(guestRegisters);
            break;

        case EXIT_REASON_MSR_WRITE:
            HvHandleMsrWrite(guestRegisters);
            break;

        case EXIT_REASON_EPT_VIOLATION:
            HvHandleEptViolation(guestRegisters);
            break;

        case EXIT_REASON_EXCEPTION_NMI:
            DbgPrint("[HV] Exception/NMI\n");
            break;

        case EXIT_REASON_EXTERNAL_INTERRUPT:
            DbgPrint("[HV] External interrupt\n");

            break;

        case EXIT_REASON_TRIPLE_FAULT:
            DbgPrint("[HV] Triple fault!\n");

            break;

        case EXIT_REASON_INIT:
            DbgPrint("[HV] INIT signal\n");
            break;

        case EXIT_REASON_SIPI:
            DbgPrint("[HV] SIPI signal\n");
            break;

        case EXIT_REASON_HLT:
            DbgPrint("[HV] HLT instruction\n");
            HvAdvanceGuestRip();
            break;

        case EXIT_REASON_INVD:
            DbgPrint("[HV] INVD instruction\n");
            __wbinvd();
            HvAdvanceGuestRip();
            break;

        case EXIT_REASON_INVLPG:
            DbgPrint("[HV] INVLPG instruction\n");
            HvAdvanceGuestRip();
            break;

        case EXIT_REASON_RDTSC:
        {
            DbgPrint("[HV] RDTSC instruction\n");

            ULONG64 tsc = __rdtsc();
            guestRegisters->Rax = (ULONG)(tsc & 0xFFFFFFFF);
            guestRegisters->Rdx = (ULONG)(tsc >> 32);
            HvAdvanceGuestRip();
            break;
        }

        case EXIT_REASON_RDTSCP:
        {
            DbgPrint("[HV] RDTSCP instruction\n");

            ULONG aux;
            ULONG64 tsc = __rdtscp(&aux);

            guestRegisters->Rax = (ULONG)(tsc & 0xFFFFFFFF);
            guestRegisters->Rdx = (ULONG)(tsc >> 32);
            guestRegisters->Rcx = aux;

            HvAdvanceGuestRip();
            break;
        }

        case EXIT_REASON_CR_ACCESS:
            HvHandleCrAccess(guestRegisters);
            break;

        case EXIT_REASON_DR_ACCESS:
            HvHandleDrAccess(guestRegisters);
            break;

        case EXIT_REASON_IO_INSTRUCTION:
            HvHandleIoInstruction(guestRegisters);
            break;

        case EXIT_REASON_VMXOFF:
            DbgPrint("[HV] VMXOFF instruction - devirtualizing\n");

            HvAdvanceGuestRip();

            break;

        default:
            DbgPrint("[HV] Unhandled VM-Exit: %d\n", exitReason);
            HvAdvanceGuestRip();
            break;
    }

}

VOID HvHandleCpuid(PGUEST_REGISTERS guestRegisters)
{
    int cpuInfo[4];
    int function = (int)guestRegisters->Rax;
    int subFunction = (int)guestRegisters->Rcx;

    __cpuidex(cpuInfo, function, subFunction);

    if (function == 1) {

        cpuInfo[2] &= ~(1 << 31);
        DbgPrint("[HV] CPUID.1 - Hiding hypervisor bit\n");
    }
    else if (function == 0x40000000) {

        cpuInfo[0] = 0;
        cpuInfo[1] = 0;
        cpuInfo[2] = 0;
        cpuInfo[3] = 0;
        DbgPrint("[HV] CPUID.0x40000000 - Hiding hypervisor info\n");
    }

    guestRegisters->Rax = cpuInfo[0];
    guestRegisters->Rbx = cpuInfo[1];
    guestRegisters->Rcx = cpuInfo[2];
    guestRegisters->Rdx = cpuInfo[3];

    HvAdvanceGuestRip();
}

VOID HvHandleVmcall(PGUEST_REGISTERS guestRegisters)
{
    ULONG_PTR hypercallNumber = guestRegisters->Rcx;

    DbgPrint("[HV] VMCALL: Hypercall=%lld\n", hypercallNumber);

    switch (hypercallNumber) {
        case 0x1000:
        {
            PVOID address = (PVOID)guestRegisters->Rdx;
            ULONG size = (ULONG)guestRegisters->R8;
            PVOID buffer = (PVOID)guestRegisters->R9;

            DbgPrint("[HV] Hypercall: Read memory at 0x%p, size=%d\n", address, size);

            if (size == 0 || size > 0x100000) {
                guestRegisters->Rax = (ULONG_PTR)STATUS_INVALID_PARAMETER;
                break;
            }

            SIZE_T bytesCopied;
            NTSTATUS status;

            __try {
                status = MmCopyVirtualMemory(
                    PsGetCurrentProcess(),
                    address,
                    PsGetCurrentProcess(),
                    buffer,
                    size,
                    KernelMode,
                    &bytesCopied
                );
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                status = STATUS_ACCESS_VIOLATION;
            }

            guestRegisters->Rax = (ULONG_PTR)status;
            break;
        }

        case 0x1001:
        {
            PVOID address = (PVOID)guestRegisters->Rdx;
            ULONG size = (ULONG)guestRegisters->R8;
            PVOID buffer = (PVOID)guestRegisters->R9;

            DbgPrint("[HV] Hypercall: Write memory at 0x%p, size=%d\n", address, size);

            if (size == 0 || size > 0x100000) {
                guestRegisters->Rax = (ULONG_PTR)STATUS_INVALID_PARAMETER;
                break;
            }

            SIZE_T bytesCopied;
            NTSTATUS status;

            __try {
                status = MmCopyVirtualMemory(
                    PsGetCurrentProcess(),
                    buffer,
                    PsGetCurrentProcess(),
                    address,
                    size,
                    KernelMode,
                    &bytesCopied
                );
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                status = STATUS_ACCESS_VIOLATION;
            }

            guestRegisters->Rax = (ULONG_PTR)status;
            break;
        }

        case 0x1002:
        {
            ULONG processId = (ULONG)guestRegisters->Rdx;

            DbgPrint("[HV] Hypercall: Hide process PID=%d\n", processId);

            guestRegisters->Rax = 0;
            break;
        }

        case 0xFFFF:
        {
            DbgPrint("[HV] Hypercall: Devirtualize request\n");

            InterlockedExchange(&g_HvShutdownRequested, 1);

            ULONG currentCpu = KeGetCurrentProcessorNumber();
            if (currentCpu < g_HypervisorData.ProcessorCount) {
                PVCPU_DATA vcpu = &g_HypervisorData.VcpuData[currentCpu];
                vcpu->ShutdownRequested = TRUE;
                vcpu->IsVirtualized = FALSE;

                __vmx_off();

                ULONG_PTR cr4 = __readcr4();
                cr4 &= ~CR4_VMXE;
                __writecr4(cr4);

                DbgPrint("[HV] CPU %d devirtualized via VMCALL\n", currentCpu);

                RtlRestoreContext(&vcpu->LaunchContext, NULL);
            }

            break;
        }

        default:
            DbgPrint("[HV] Unknown hypercall: 0x%llx\n", hypercallNumber);
            guestRegisters->Rax = (ULONG_PTR)-1;
            break;
    }

    HvAdvanceGuestRip();
}

VOID HvHandleMsrRead(PGUEST_REGISTERS guestRegisters)
{
    ULONG msr = (ULONG)guestRegisters->Rcx;

    if (msr == IA32_FEATURE_CONTROL) {
        guestRegisters->Rax = 1;
        guestRegisters->Rdx = 0;
        HvAdvanceGuestRip();
        return;
    }

    if (msr >= IA32_VMX_BASIC && msr <= IA32_VMX_EPT_VPID_CAP) {
        guestRegisters->Rax = 0;
        guestRegisters->Rdx = 0;
        HvAdvanceGuestRip();
        return;
    }

    ULONG64 value = __readmsr(msr);

    DbgPrint("[HV] MSR Read: 0x%X = 0x%llx\n", msr, value);

    guestRegisters->Rax = (ULONG)(value & 0xFFFFFFFF);
    guestRegisters->Rdx = (ULONG)(value >> 32);

    HvAdvanceGuestRip();
}

VOID HvHandleMsrWrite(PGUEST_REGISTERS guestRegisters)
{
    ULONG msr = (ULONG)guestRegisters->Rcx;
    ULONG64 value = ((ULONG64)guestRegisters->Rdx << 32) | (ULONG)guestRegisters->Rax;

    if (msr == IA32_FEATURE_CONTROL ||
        (msr >= IA32_VMX_BASIC && msr <= IA32_VMX_EPT_VPID_CAP)) {
        DbgPrint("[HV] MSR Write blocked (stealth): 0x%X\n", msr);
        HvAdvanceGuestRip();
        return;
    }

    DbgPrint("[HV] MSR Write: 0x%X = 0x%llx\n", msr, value);

    __writemsr(msr, value);

    HvAdvanceGuestRip();
}

VOID HvHandleEptViolation(PGUEST_REGISTERS guestRegisters)
{
    ULONG_PTR guestPhysicalAddress = 0;
    ULONG_PTR exitQualification = 0;

    __vmx_vmread(GUEST_PHYSICAL_ADDRESS, &guestPhysicalAddress);
    __vmx_vmread(EXIT_QUALIFICATION, &exitQualification);

    BOOLEAN isRead = (exitQualification & 0x01) != 0;
    BOOLEAN isWrite = (exitQualification & 0x02) != 0;
    BOOLEAN isExecute = (exitQualification & 0x04) != 0;

    DbgPrint("[HV] EPT Violation at GPA: 0x%llx\n", guestPhysicalAddress);
    DbgPrint("     Read=%d, Write=%d, Execute=%d\n", isRead, isWrite, isExecute);

    if (guestPhysicalAddress == 0x12345000 && isRead) {
        DbgPrint("[HV] Intercepting read from protected address\n");

    }

    if (guestPhysicalAddress == 0x67890000 && isWrite) {
        DbgPrint("[HV] Blocking write to protected address\n");

        HvAdvanceGuestRip();
        return;
    }

    DbgPrint("[HV] EPT Violation unhandled, GPA=0x%llx, guest RIP=0x%llx - restoring access\n",
        guestPhysicalAddress, HvGetGuestRip());

    NTSTATUS permStatus = EptSetPagePermissions(g_EptTables, guestPhysicalAddress, TRUE, TRUE, TRUE);
    if (!NT_SUCCESS(permStatus)) {
        DbgPrint("[HV] WARNING: Failed to restore EPT permissions: 0x%X\n", permStatus);
    }
}

ULONG_PTR HvGetGuestRip()
{
    ULONG_PTR guestRip = 0;
    __vmx_vmread(GUEST_RIP, &guestRip);
    return guestRip;
}

VOID HvSetGuestRip(ULONG_PTR rip)
{
    __vmx_vmwrite(GUEST_RIP, rip);
}

VOID HvAdvanceGuestRip()
{
    ULONG_PTR instructionLength = HvGetInstructionLength();
    ULONG_PTR guestRip = HvGetGuestRip();

    HvSetGuestRip(guestRip + instructionLength);
}

ULONG_PTR HvGetInstructionLength()
{
    ULONG_PTR length = 0;
    __vmx_vmread(0x440C, &length);
    return length;
}

static ULONG_PTR HvGetGuestRegByIndex(PGUEST_REGISTERS regs, ULONG index)
{
    switch (index) {
        case 0:  return regs->Rax;
        case 1:  return regs->Rcx;
        case 2:  return regs->Rdx;
        case 3:  return regs->Rbx;
        case 4:  return regs->Rsp;
        case 5:  return regs->Rbp;
        case 6:  return regs->Rsi;
        case 7:  return regs->Rdi;
        case 8:  return regs->R8;
        case 9:  return regs->R9;
        case 10: return regs->R10;
        case 11: return regs->R11;
        case 12: return regs->R12;
        case 13: return regs->R13;
        case 14: return regs->R14;
        case 15: return regs->R15;
        default: return 0;
    }
}

static VOID HvSetGuestRegByIndex(PGUEST_REGISTERS regs, ULONG index, ULONG_PTR value)
{
    switch (index) {
        case 0:  regs->Rax = value; break;
        case 1:  regs->Rcx = value; break;
        case 2:  regs->Rdx = value; break;
        case 3:  regs->Rbx = value; break;
        case 4:  regs->Rsp = value; break;
        case 5:  regs->Rbp = value; break;
        case 6:  regs->Rsi = value; break;
        case 7:  regs->Rdi = value; break;
        case 8:  regs->R8 = value;  break;
        case 9:  regs->R9 = value;  break;
        case 10: regs->R10 = value; break;
        case 11: regs->R11 = value; break;
        case 12: regs->R12 = value; break;
        case 13: regs->R13 = value; break;
        case 14: regs->R14 = value; break;
        case 15: regs->R15 = value; break;
    }
}

VOID HvHandleCrAccess(PGUEST_REGISTERS guestRegisters)
{
    ULONG_PTR exitQualification = 0;
    __vmx_vmread(EXIT_QUALIFICATION, &exitQualification);

    ULONG crNumber = (ULONG)(exitQualification & 0xF);
    ULONG accessType = (ULONG)((exitQualification >> 4) & 0x3);
    ULONG regIndex = (ULONG)((exitQualification >> 8) & 0xF);

    switch (accessType) {
        case 0:
        {
            ULONG_PTR value = HvGetGuestRegByIndex(guestRegisters, regIndex);
            switch (crNumber) {
                case 0:
                    __vmx_vmwrite(GUEST_CR0, value);
                    break;
                case 3:
                    __vmx_vmwrite(GUEST_CR3, value);
                    break;
                case 4:
                    __vmx_vmwrite(GUEST_CR4, value);
                    break;
                default:
                    break;
            }
            break;
        }
        case 1:
        {
            ULONG_PTR value = 0;
            switch (crNumber) {
                case 0: __vmx_vmread(GUEST_CR0, &value); break;
                case 3: __vmx_vmread(GUEST_CR3, &value); break;
                case 4: __vmx_vmread(GUEST_CR4, &value); break;
                default: value = 0; break;
            }
            HvSetGuestRegByIndex(guestRegisters, regIndex, value);
            break;
        }
        case 2:
        {
            ULONG_PTR guestCr0;
            __vmx_vmread(GUEST_CR0, &guestCr0);
            guestCr0 &= ~CR0_TS;
            __vmx_vmwrite(GUEST_CR0, guestCr0);
            break;
        }
        case 3:
        {
            ULONG_PTR value = HvGetGuestRegByIndex(guestRegisters, regIndex);
            ULONG_PTR guestCr0;
            __vmx_vmread(GUEST_CR0, &guestCr0);
            guestCr0 = (guestCr0 & ~0xF) | (value & 0xF);
            __vmx_vmwrite(GUEST_CR0, guestCr0);
            break;
        }
    }

    HvAdvanceGuestRip();
}

VOID HvHandleDrAccess(PGUEST_REGISTERS guestRegisters)
{
    ULONG_PTR exitQualification = 0;
    __vmx_vmread(EXIT_QUALIFICATION, &exitQualification);

    ULONG drNumber = (ULONG)(exitQualification & 0x7);
    ULONG direction = (ULONG)((exitQualification >> 4) & 0x1);
    ULONG regIndex = (ULONG)((exitQualification >> 8) & 0xF);

    if (direction == 0) {
        ULONG_PTR value = HvGetGuestRegByIndex(guestRegisters, regIndex);

        if (drNumber < 4) {
            __writedr(drNumber, value);
        } else if (drNumber == 6) {
            value &= 0xFFFF0FF0;
            __writedr(6, value);
        } else if (drNumber == 7) {
            value &= 0xFFFFFFFF;
            __writedr(7, value);
            __vmx_vmwrite(GUEST_DR7, value);
        }
    } else {
        ULONG_PTR value = 0;

        if (drNumber < 4) {
            value = __readdr(drNumber);
        } else if (drNumber == 6) {
            value = __readdr(6);
            value &= 0xFFFF0FF0;
        } else if (drNumber == 7) {
            value = __readdr(7);
            value &= 0xFFFFFFFF;
        }

        HvSetGuestRegByIndex(guestRegisters, regIndex, value);
    }

    HvAdvanceGuestRip();
}

VOID HvHandleIoInstruction(PGUEST_REGISTERS guestRegisters)
{
    UNREFERENCED_PARAMETER(guestRegisters);

    DbgPrint("[HV] Unexpected I/O instruction exit\n");
    HvAdvanceGuestRip();
}
