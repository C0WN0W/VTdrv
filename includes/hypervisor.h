#pragma once
#include <ntifs.h>
#include <intrin.h>

#ifdef __cplusplus
extern "C" {
#endif

USHORT __readcs(VOID);
USHORT __readds(VOID);
USHORT __reades(VOID);
USHORT __readfs(VOID);
USHORT __readgs(VOID);
USHORT __readss(VOID);
USHORT __readldtr(VOID);
USHORT __readtr(VOID);
ULONG __lar(USHORT Selector);

#ifdef __cplusplus
}
#endif

#define VMX_OK                      0
#define VMX_ERROR_WITH_STATUS       1
#define VMX_ERROR_WITHOUT_STATUS    2

#define IA32_FEATURE_CONTROL        0x3A
#define IA32_VMX_BASIC              0x480
#define IA32_VMX_PINBASED_CTLS      0x481
#define IA32_VMX_PROCBASED_CTLS     0x482
#define IA32_VMX_EXIT_CTLS          0x483
#define IA32_VMX_ENTRY_CTLS         0x484
#define IA32_VMX_PROCBASED_CTLS2    0x48B
#define IA32_VMX_EPT_VPID_CAP       0x48C

#define IA32_SYSENTER_CS            0x174
#define IA32_SYSENTER_ESP           0x175
#define IA32_SYSENTER_EIP           0x176
#define IA32_DEBUGCTL               0x1D9
#define IA32_PAT                    0x277
#define IA32_EFER                   0xC0000080
#define IA32_FS_BASE                0xC0000100
#define IA32_GS_BASE                0xC0000101

#define CR0_PE                      (1UL << 0)
#define CR0_MP                      (1UL << 1)
#define CR0_EM                      (1UL << 2)
#define CR0_TS                      (1UL << 3)
#define CR0_ET                      (1UL << 4)
#define CR0_NE                      (1UL << 5)
#define CR0_WP                      (1UL << 16)
#define CR0_AM                      (1UL << 18)
#define CR0_NW                      (1UL << 29)
#define CR0_CD                      (1UL << 30)
#define CR0_PG                      (1UL << 31)

#define CR4_VME                     (1UL << 0)
#define CR4_PVI                     (1UL << 1)
#define CR4_TSD                     (1UL << 2)
#define CR4_DE                      (1UL << 3)
#define CR4_PSE                     (1UL << 4)
#define CR4_PAE                     (1UL << 5)
#define CR4_MCE                     (1UL << 6)
#define CR4_PGE                     (1UL << 7)
#define CR4_PCE                     (1UL << 8)
#define CR4_OSFXSR                  (1UL << 9)
#define CR4_OSXMMEXCPT              (1UL << 10)
#define CR4_UMIP                    (1UL << 11)
#define CR4_VMXE                    (1UL << 13)
#define CR4_SMXE                    (1UL << 14)
#define CR4_FSGSBASE                (1UL << 16)
#define CR4_PCIDE                   (1UL << 17)
#define CR4_OSXSAVE                 (1UL << 18)
#define CR4_SMEP                    (1UL << 20)
#define CR4_SMAP                    (1UL << 21)

#define VIRTUAL_PROCESSOR_ID        0x00000000
#define POSTED_INTR_NV              0x00000002
#define EPTP_INDEX                  0x00000004

#define GUEST_ES_SELECTOR           0x00000800
#define GUEST_CS_SELECTOR           0x00000802
#define GUEST_SS_SELECTOR           0x00000804
#define GUEST_DS_SELECTOR           0x00000806
#define GUEST_FS_SELECTOR           0x00000808
#define GUEST_GS_SELECTOR           0x0000080A
#define GUEST_LDTR_SELECTOR         0x0000080C
#define GUEST_TR_SELECTOR           0x0000080E
#define GUEST_INTR_STATUS           0x00000810
#define GUEST_PML_INDEX             0x00000812

#define HOST_ES_SELECTOR            0x00000C00
#define HOST_CS_SELECTOR            0x00000C02
#define HOST_SS_SELECTOR            0x00000C04
#define HOST_DS_SELECTOR            0x00000C06
#define HOST_FS_SELECTOR            0x00000C08
#define HOST_GS_SELECTOR            0x00000C0A
#define HOST_TR_SELECTOR            0x00000C0C

#define IO_BITMAP_A                 0x00002000
#define IO_BITMAP_A_HIGH            0x00002001
#define IO_BITMAP_B                 0x00002002
#define IO_BITMAP_B_HIGH            0x00002003
#define MSR_BITMAP                  0x00002004
#define MSR_BITMAP_HIGH             0x00002005
#define VM_EXIT_MSR_STORE_ADDR      0x00002006
#define VM_EXIT_MSR_STORE_ADDR_HIGH 0x00002007
#define VM_EXIT_MSR_LOAD_ADDR       0x00002008
#define VM_EXIT_MSR_LOAD_ADDR_HIGH  0x00002009
#define VM_ENTRY_MSR_LOAD_ADDR      0x0000200A
#define VM_ENTRY_MSR_LOAD_ADDR_HIGH 0x0000200B
#define EXECUTIVE_VMCS_POINTER      0x0000200C
#define EXECUTIVE_VMCS_POINTER_HIGH 0x0000200D
#define PML_ADDRESS                 0x0000200E
#define PML_ADDRESS_HIGH            0x0000200F
#define TSC_OFFSET                  0x00002010
#define TSC_OFFSET_HIGH             0x00002011
#define VIRTUAL_APIC_PAGE_ADDR      0x00002012
#define VIRTUAL_APIC_PAGE_ADDR_HIGH 0x00002013
#define APIC_ACCESS_ADDR            0x00002014
#define APIC_ACCESS_ADDR_HIGH       0x00002015
#define POSTED_INTR_DESC_ADDR       0x00002016
#define POSTED_INTR_DESC_ADDR_HIGH  0x00002017
#define VM_FUNCTION_CONTROL         0x00002018
#define VM_FUNCTION_CONTROL_HIGH    0x00002019
#define EPT_POINTER                 0x0000201A
#define EPT_POINTER_HIGH            0x0000201B
#define EOI_EXIT_BITMAP0            0x0000201C
#define EOI_EXIT_BITMAP0_HIGH       0x0000201D
#define EOI_EXIT_BITMAP1            0x0000201E
#define EOI_EXIT_BITMAP1_HIGH       0x0000201F
#define EOI_EXIT_BITMAP2            0x00002020
#define EOI_EXIT_BITMAP2_HIGH       0x00002021
#define EOI_EXIT_BITMAP3            0x00002022
#define EOI_EXIT_BITMAP3_HIGH       0x00002023
#define EPTP_LIST_ADDRESS           0x00002024
#define EPTP_LIST_ADDRESS_HIGH      0x00002025
#define VMREAD_BITMAP               0x00002026
#define VMREAD_BITMAP_HIGH          0x00002027
#define VMWRITE_BITMAP              0x00002028
#define VMWRITE_BITMAP_HIGH         0x00002029
#define VE_INFO_ADDRESS             0x0000202A
#define VE_INFO_ADDRESS_HIGH        0x0000202B
#define XSS_EXIT_BITMAP             0x0000202C
#define XSS_EXIT_BITMAP_HIGH        0x0000202D
#define ENCLS_EXITING_BITMAP        0x0000202E
#define ENCLS_EXITING_BITMAP_HIGH   0x0000202F
#define TSC_MULTIPLIER              0x00002032
#define TSC_MULTIPLIER_HIGH         0x00002033

#define GUEST_PHYSICAL_ADDRESS      0x00002400
#define GUEST_PHYSICAL_ADDRESS_HIGH 0x00002401

#define VMCS_LINK_POINTER           0x00002800
#define VMCS_LINK_POINTER_HIGH      0x00002801
#define GUEST_IA32_DEBUGCTL         0x00002802
#define GUEST_IA32_DEBUGCTL_HIGH    0x00002803
#define GUEST_IA32_PAT              0x00002804
#define GUEST_IA32_PAT_HIGH         0x00002805
#define GUEST_IA32_EFER             0x00002806
#define GUEST_IA32_EFER_HIGH        0x00002807
#define GUEST_IA32_PERF_GLOBAL_CTRL 0x00002808
#define GUEST_IA32_PERF_GLOBAL_CTRL_HIGH 0x00002809
#define GUEST_PDPTR0                0x0000280A
#define GUEST_PDPTR0_HIGH           0x0000280B
#define GUEST_PDPTR1                0x0000280C
#define GUEST_PDPTR1_HIGH           0x0000280D
#define GUEST_PDPTR2                0x0000280E
#define GUEST_PDPTR2_HIGH           0x0000280F
#define GUEST_PDPTR3                0x00002810
#define GUEST_PDPTR3_HIGH           0x00002811
#define GUEST_BNDCFGS               0x00002812
#define GUEST_BNDCFGS_HIGH          0x00002813

#define HOST_IA32_PAT               0x00002C00
#define HOST_IA32_PAT_HIGH          0x00002C01
#define HOST_IA32_EFER              0x00002C02
#define HOST_IA32_EFER_HIGH         0x00002C03
#define HOST_IA32_PERF_GLOBAL_CTRL  0x00002C04
#define HOST_IA32_PERF_GLOBAL_CTRL_HIGH 0x00002C05

#define PIN_BASED_VM_EXEC_CONTROL   0x00004000
#define CPU_BASED_VM_EXEC_CONTROL   0x00004002
#define EXCEPTION_BITMAP            0x00004004
#define PAGE_FAULT_ERROR_CODE_MASK  0x00004006
#define PAGE_FAULT_ERROR_CODE_MATCH 0x00004008
#define CR3_TARGET_COUNT            0x0000400A
#define VM_EXIT_CONTROLS            0x0000400C
#define VM_EXIT_MSR_STORE_COUNT     0x0000400E
#define VM_EXIT_MSR_LOAD_COUNT      0x00004010
#define VM_ENTRY_CONTROLS           0x00004012
#define VM_ENTRY_MSR_LOAD_COUNT     0x00004014
#define VM_ENTRY_INTR_INFO_FIELD    0x00004016
#define VM_ENTRY_EXCEPTION_ERROR_CODE 0x00004018
#define VM_ENTRY_INSTRUCTION_LEN    0x0000401A
#define TPR_THRESHOLD               0x0000401C
#define SECONDARY_VM_EXEC_CONTROL   0x0000401E
#define PLE_GAP                     0x00004020
#define PLE_WINDOW                  0x00004022

#define VM_INSTRUCTION_ERROR        0x00004400
#define VM_EXIT_REASON              0x00004402
#define VM_EXIT_INTR_INFO           0x00004404
#define VM_EXIT_INTR_ERROR_CODE     0x00004406
#define IDT_VECTORING_INFO_FIELD    0x00004408
#define IDT_VECTORING_ERROR_CODE    0x0000440A
#define VM_EXIT_INSTRUCTION_LEN     0x0000440C
#define VMX_INSTRUCTION_INFO        0x0000440E

#define GUEST_ES_LIMIT              0x00004800
#define GUEST_CS_LIMIT              0x00004802
#define GUEST_SS_LIMIT              0x00004804
#define GUEST_DS_LIMIT              0x00004806
#define GUEST_FS_LIMIT              0x00004808
#define GUEST_GS_LIMIT              0x0000480A
#define GUEST_LDTR_LIMIT            0x0000480C
#define GUEST_TR_LIMIT              0x0000480E
#define GUEST_GDTR_LIMIT            0x00004810
#define GUEST_IDTR_LIMIT            0x00004812
#define GUEST_ES_AR_BYTES           0x00004814
#define GUEST_CS_AR_BYTES           0x00004816
#define GUEST_SS_AR_BYTES           0x00004818
#define GUEST_DS_AR_BYTES           0x0000481A
#define GUEST_FS_AR_BYTES           0x0000481C
#define GUEST_GS_AR_BYTES           0x0000481E
#define GUEST_LDTR_AR_BYTES         0x00004820
#define GUEST_TR_AR_BYTES           0x00004822
#define GUEST_INTERRUPTIBILITY_INFO 0x00004824
#define GUEST_ACTIVITY_STATE        0x00004826
#define GUEST_SMBASE                0x00004828
#define GUEST_SYSENTER_CS           0x0000482A
#define GUEST_VMX_PREEMPTION_TIMER_VALUE 0x0000482E

#define HOST_IA32_SYSENTER_CS       0x00004C00

#define CR0_GUEST_HOST_MASK         0x00006000
#define CR4_GUEST_HOST_MASK         0x00006002
#define CR0_READ_SHADOW             0x00006004
#define CR4_READ_SHADOW             0x00006006
#define CR3_TARGET_VALUE0           0x00006008
#define CR3_TARGET_VALUE1           0x0000600A
#define CR3_TARGET_VALUE2           0x0000600C
#define CR3_TARGET_VALUE3           0x0000600E

#define EXIT_QUALIFICATION          0x00006400
#define IO_RCX                      0x00006402
#define IO_RSI                      0x00006404
#define IO_RDI                      0x00006406
#define IO_RIP                      0x00006408
#define GUEST_LINEAR_ADDRESS        0x0000640A

#define GUEST_CR0                   0x00006800
#define GUEST_CR3                   0x00006802
#define GUEST_CR4                   0x00006804
#define GUEST_ES_BASE               0x00006806
#define GUEST_CS_BASE               0x00006808
#define GUEST_SS_BASE               0x0000680A
#define GUEST_DS_BASE               0x0000680C
#define GUEST_FS_BASE               0x0000680E
#define GUEST_GS_BASE               0x00006810
#define GUEST_LDTR_BASE             0x00006812
#define GUEST_TR_BASE               0x00006814
#define GUEST_GDTR_BASE             0x00006816
#define GUEST_IDTR_BASE             0x00006818
#define GUEST_DR7                   0x0000681A
#define GUEST_RSP                   0x0000681C
#define GUEST_RIP                   0x0000681E
#define GUEST_RFLAGS                0x00006820
#define GUEST_PENDING_DBG_EXCEPTIONS 0x00006822
#define GUEST_SYSENTER_ESP          0x00006824
#define GUEST_SYSENTER_EIP          0x00006826

#define HOST_CR0                    0x00006C00
#define HOST_CR3                    0x00006C02
#define HOST_CR4                    0x00006C04
#define HOST_FS_BASE                0x00006C06
#define HOST_GS_BASE                0x00006C08
#define HOST_TR_BASE                0x00006C0A
#define HOST_GDTR_BASE              0x00006C0C
#define HOST_IDTR_BASE              0x00006C0E
#define HOST_IA32_SYSENTER_ESP      0x00006C10
#define HOST_IA32_SYSENTER_EIP      0x00006C12
#define HOST_RSP                    0x00006C14
#define HOST_RIP                    0x00006C16

#define EXIT_REASON_EXCEPTION_NMI   0
#define EXIT_REASON_EXTERNAL_INTERRUPT 1
#define EXIT_REASON_TRIPLE_FAULT    2
#define EXIT_REASON_INIT            3
#define EXIT_REASON_SIPI            4
#define EXIT_REASON_CPUID           10
#define EXIT_REASON_HLT             12
#define EXIT_REASON_INVD            13
#define EXIT_REASON_INVLPG          14
#define EXIT_REASON_RDPMC           15
#define EXIT_REASON_RDTSC           16
#define EXIT_REASON_VMCALL          18
#define EXIT_REASON_VMCLEAR         19
#define EXIT_REASON_VMLAUNCH        20
#define EXIT_REASON_VMPTRLD         21
#define EXIT_REASON_VMPTRST         22
#define EXIT_REASON_VMREAD          23
#define EXIT_REASON_VMRESUME        24
#define EXIT_REASON_VMWRITE         25
#define EXIT_REASON_VMXOFF          26
#define EXIT_REASON_VMXON           27
#define EXIT_REASON_CR_ACCESS       28
#define EXIT_REASON_DR_ACCESS       29
#define EXIT_REASON_IO_INSTRUCTION  30
#define EXIT_REASON_MSR_READ        31
#define EXIT_REASON_MSR_WRITE       32
#define EXIT_REASON_EPT_VIOLATION   48
#define EXIT_REASON_EPT_MISCONFIG   49
#define EXIT_REASON_INVEPT          50
#define EXIT_REASON_RDTSCP          51
#define EXIT_REASON_PREEMPTION_TIMER 52
#define EXIT_REASON_INVVPID         53
#define EXIT_REASON_WBINVD          54
#define EXIT_REASON_XSETBV          55

typedef struct _VMX_REGION {
    ULONG RevisionId;
    UCHAR Data[PAGE_SIZE - sizeof(ULONG)];
} VMX_REGION, * PVMX_REGION;

typedef struct _VCPU_DATA {
    PVMX_REGION VmxonRegion;
    PHYSICAL_ADDRESS VmxonRegionPhysical;

    PVMX_REGION VmcsRegion;
    PHYSICAL_ADDRESS VmcsRegionPhysical;

    PVOID HostStack;
    ULONG_PTR HostRsp;

    BOOLEAN IsVirtualized;
    BOOLEAN ShutdownRequested;
    ULONG ProcessorNumber;

    CONTEXT LaunchContext;
    KDPC LaunchDpc;
} VCPU_DATA, * PVCPU_DATA;

typedef struct _HYPERVISOR_DATA {
    PVCPU_DATA VcpuData;
    ULONG ProcessorCount;
    BOOLEAN IsActive;
} HYPERVISOR_DATA, * PHYPERVISOR_DATA;

typedef struct _SEGMENT_DESCRIPTOR {
    USHORT Limit0;
    USHORT Base0;
    UCHAR Base1;
    UCHAR Attributes1;
    UCHAR Attributes2;
    UCHAR Base2;
} SEGMENT_DESCRIPTOR, * PSEGMENT_DESCRIPTOR;

typedef struct _SEGMENT_SELECTOR {
    USHORT Selector;
    ULONG Base;
    ULONG Limit;
    ULONG AccessRights;
} SEGMENT_SELECTOR, * PSEGMENT_SELECTOR;

typedef struct _DESCRIPTOR_TABLE_REGISTER {
    USHORT Limit;
    ULONG_PTR Base;
} DESCRIPTOR_TABLE_REGISTER, * PDESCRIPTOR_TABLE_REGISTER;

typedef struct _GUEST_REGISTERS {
    ULONG_PTR Rax;
    ULONG_PTR Rcx;
    ULONG_PTR Rdx;
    ULONG_PTR Rbx;
    ULONG_PTR Rsp;
    ULONG_PTR Rbp;
    ULONG_PTR Rsi;
    ULONG_PTR Rdi;
    ULONG_PTR R8;
    ULONG_PTR R9;
    ULONG_PTR R10;
    ULONG_PTR R11;
    ULONG_PTR R12;
    ULONG_PTR R13;
    ULONG_PTR R14;
    ULONG_PTR R15;
} GUEST_REGISTERS, * PGUEST_REGISTERS;

NTSTATUS HvInitialize();
VOID HvTerminate();
BOOLEAN HvIsActive();

BOOLEAN HvCheckVmxSupport();
NTSTATUS HvVirtualizeAllProcessors();
NTSTATUS HvVirtualizeProcessor(ULONG processorNumber);
VOID HvDevirtualizeProcessor(ULONG processorNumber);
ULONG_PTR HvLaunchDpcRoutine(KDPC* Dpc, PVOID Context, PVOID SystemArgument1, PVOID SystemArgument2);

PVMX_REGION HvAllocateVmxRegion();
VOID HvFreeVmxRegion(PVMX_REGION region);

BOOLEAN HvEnableVmxOperation(PVCPU_DATA vcpu);
VOID HvDisableVmxOperation();

BOOLEAN HvSetupVmcs(PVCPU_DATA vcpu);
VOID HvSetupVmcsControls();
VOID HvSetupVmcsGuestState();
VOID HvSetupVmcsHostState(PVCPU_DATA vcpu);

VOID HvGetSegmentDescriptor(PSEGMENT_SELECTOR selector, USHORT segmentSelector, PUCHAR gdtBase);
ULONG HvGetSegmentAccessRights(USHORT segmentSelector);

VOID __fastcall HvVmExitHandler(PGUEST_REGISTERS guestRegisters);
VOID HvHandleCpuid(PGUEST_REGISTERS guestRegisters);
VOID HvHandleVmcall(PGUEST_REGISTERS guestRegisters);
VOID HvHandleMsrRead(PGUEST_REGISTERS guestRegisters);
VOID HvHandleMsrWrite(PGUEST_REGISTERS guestRegisters);
VOID HvHandleEptViolation(PGUEST_REGISTERS guestRegisters);
VOID HvHandleCrAccess(PGUEST_REGISTERS guestRegisters);
VOID HvHandleDrAccess(PGUEST_REGISTERS guestRegisters);
VOID HvHandleIoInstruction(PGUEST_REGISTERS guestRegisters);

ULONG_PTR HvGetGuestRip();
VOID HvSetGuestRip(ULONG_PTR rip);
VOID HvAdvanceGuestRip();
ULONG_PTR HvGetInstructionLength();

VOID AsmVmExitEntry();
NTSTATUS AsmVmxLaunch();
VOID AsmVmxCall();
NTSTATUS AsmInvept(ULONG Type, PVOID Descriptor);
NTSTATUS AsmInvvpid(ULONG Type, PVOID Descriptor);
VOID AsmVmxSaveState(PGUEST_REGISTERS GuestRegisters);
VOID AsmVmxRestoreState(PGUEST_REGISTERS GuestRegisters);

extern HYPERVISOR_DATA g_HypervisorData;
extern PVOID g_MsrBitmap;
extern PVOID g_IoBitmapA;
extern PVOID g_IoBitmapB;
extern volatile LONG g_HvShutdownRequested;

NTSYSAPI VOID NTAPI RtlCaptureContext(PCONTEXT ContextRecord);
NTSYSAPI VOID NTAPI RtlRestoreContext(PCONTEXT ContextRecord, PEXCEPTION_RECORD ExceptionRecord);

typedef struct _PHYSICAL_MEMORY_RANGE {
    PHYSICAL_ADDRESS BaseAddress;
    PHYSICAL_ADDRESS NumberOfBytes;
} PHYSICAL_MEMORY_RANGE, *PPHYSICAL_MEMORY_RANGE;

NTSYSAPI PPHYSICAL_MEMORY_RANGE NTAPI MmGetPhysicalMemoryRanges(VOID);
