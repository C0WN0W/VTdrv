#include "hypervisor.h"

VOID __fastcall HvVmExitHandler(PGUEST_REGISTERS guestRegisters)
{
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
            HvAdvanceGuestRip();
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
            DbgPrint("[HV] RDTSC instruction\n");

            guestRegisters->Rax = (ULONG)(__rdtsc() & 0xFFFFFFFF);
            guestRegisters->Rdx = (ULONG)(__rdtsc() >> 32);
            HvAdvanceGuestRip();
            break;

        case EXIT_REASON_CR_ACCESS:
            DbgPrint("[HV] CR access\n");
            HvAdvanceGuestRip();
            break;

        case EXIT_REASON_DR_ACCESS:
            DbgPrint("[HV] DR access\n");
            HvAdvanceGuestRip();
            break;

        case EXIT_REASON_IO_INSTRUCTION:
            DbgPrint("[HV] I/O instruction\n");
            HvAdvanceGuestRip();
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

            guestRegisters->Rax = 0;
            break;
        }

        case 0x1001:
        {
            PVOID address = (PVOID)guestRegisters->Rdx;
            ULONG size = (ULONG)guestRegisters->R8;
            PVOID buffer = (PVOID)guestRegisters->R9;

            DbgPrint("[HV] Hypercall: Write memory at 0x%p, size=%d\n", address, size);

            guestRegisters->Rax = 0;
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

            guestRegisters->Rax = 0;
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
    ULONG64 value = __readmsr(msr);

    DbgPrint("[HV] MSR Read: 0x%X = 0x%llx\n", msr, value);

    if (msr == 0x1A0) {

    }

    guestRegisters->Rax = (ULONG)(value & 0xFFFFFFFF);
    guestRegisters->Rdx = (ULONG)(value >> 32);

    HvAdvanceGuestRip();
}

VOID HvHandleMsrWrite(PGUEST_REGISTERS guestRegisters)
{
    ULONG msr = (ULONG)guestRegisters->Rcx;
    ULONG64 value = ((ULONG64)guestRegisters->Rdx << 32) | (ULONG)guestRegisters->Rax;

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

    HvAdvanceGuestRip();
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
