#pragma once
#include <ntifs.h>

#define MEMORY_TYPE_UC  0
#define MEMORY_TYPE_WC  1
#define MEMORY_TYPE_WT  4
#define MEMORY_TYPE_WP  5
#define MEMORY_TYPE_WB  6

#define EPT_PAGE_SIZE_4KB   0
#define EPT_PAGE_SIZE_2MB   1
#define EPT_PAGE_SIZE_1GB   2

typedef union _EPT_PML4E {
    ULONG64 Value;
    struct {
        ULONG64 Read : 1;
        ULONG64 Write : 1;
        ULONG64 Execute : 1;
        ULONG64 Reserved1 : 5;
        ULONG64 Accessed : 1;
        ULONG64 Reserved2 : 3;
        ULONG64 PageFrameNumber : 40;
        ULONG64 Reserved3 : 12;
    } Fields;
} EPT_PML4E, *PEPT_PML4E;

typedef union _EPT_PDPTE {
    ULONG64 Value;
    struct {
        ULONG64 Read : 1;
        ULONG64 Write : 1;
        ULONG64 Execute : 1;
        ULONG64 Reserved1 : 4;
        ULONG64 LargePage : 1;
        ULONG64 Accessed : 1;
        ULONG64 Dirty : 1;
        ULONG64 Reserved2 : 2;
        ULONG64 PageFrameNumber : 40;
        ULONG64 Reserved3 : 12;
    } Fields;
} EPT_PDPTE, *PEPT_PDPTE;

typedef union _EPT_PDE {
    ULONG64 Value;
    struct {
        ULONG64 Read : 1;
        ULONG64 Write : 1;
        ULONG64 Execute : 1;
        ULONG64 MemoryType : 3;
        ULONG64 IgnorePat : 1;
        ULONG64 LargePage : 1;
        ULONG64 Accessed : 1;
        ULONG64 Dirty : 1;
        ULONG64 Reserved1 : 2;
        ULONG64 PageFrameNumber : 40;
        ULONG64 Reserved2 : 11;
        ULONG64 SuppressVE : 1;
    } Fields;
} EPT_PDE, *PEPT_PDE;

typedef union _EPT_PTE {
    ULONG64 Value;
    struct {
        ULONG64 Read : 1;
        ULONG64 Write : 1;
        ULONG64 Execute : 1;
        ULONG64 MemoryType : 3;
        ULONG64 IgnorePat : 1;
        ULONG64 Reserved1 : 1;
        ULONG64 Accessed : 1;
        ULONG64 Dirty : 1;
        ULONG64 Reserved2 : 2;
        ULONG64 PageFrameNumber : 40;
        ULONG64 Reserved3 : 11;
        ULONG64 SuppressVE : 1;
    } Fields;
} EPT_PTE, *PEPT_PTE;

typedef union _EPT_POINTER_STRUCT {
    ULONG64 Value;
    struct {
        ULONG64 MemoryType : 3;
        ULONG64 PageWalkLength : 3;
        ULONG64 EnableAccessAndDirtyFlags : 1;
        ULONG64 Reserved1 : 5;
        ULONG64 PageFrameNumber : 40;
        ULONG64 Reserved2 : 12;
    } Fields;
} EPT_POINTER_STRUCT, *PEPT_POINTER_STRUCT;

typedef struct _EPT_PT_ENTRY {
    PEPT_PTE Pt;
    PHYSICAL_ADDRESS PtPhysical;
    ULONG64 CoveredAddress;
    BOOLEAN IsAllocated;
    struct _EPT_PT_ENTRY* Next;
} EPT_PT_ENTRY, *PEPT_PT_ENTRY;

typedef struct _EPT_PT_MANAGER {
    PEPT_PT_ENTRY Head;
    ULONG TotalCount;
    KSPIN_LOCK Lock;
} EPT_PT_MANAGER, *PEPT_PT_MANAGER;

typedef struct _EPT_TABLES {

    PEPT_PML4E Pml4;
    PHYSICAL_ADDRESS Pml4Physical;

    PEPT_PDPTE Pdpt;
    PHYSICAL_ADDRESS PdptPhysical;

    PEPT_PDE* Pd;
    PHYSICAL_ADDRESS* PdPhysical;
    ULONG PdCount;

    EPT_PT_MANAGER PtManager;

    EPT_POINTER_STRUCT EptPointer;
} EPT_TABLES, *PEPT_TABLES;

typedef struct _EPT_HOOK_INFO {
    ULONG64 TargetAddress;
    ULONG64 OriginalPagePfn;
    ULONG64 FakePagePfn;
    ULONG64 HookPagePfn;
    PEPT_PTE TargetPte;
    BOOLEAN IsHooked;
    BOOLEAN ExecuteOnly;
} EPT_HOOK_INFO, *PEPT_HOOK_INFO;

typedef struct _EPT_HIDDEN_MEMORY {
    ULONG64 GuestPhysicalAddress;
    ULONG64 HostPhysicalAddress;
    ULONG64 Size;
    PEPT_PTE TargetPte;
    BOOLEAN IsHidden;
} EPT_HIDDEN_MEMORY, *PEPT_HIDDEN_MEMORY;

NTSTATUS EptInitialize(PEPT_TABLES* EptTables);
VOID EptTerminate(PEPT_TABLES EptTables);

NTSTATUS EptBuildTables(PEPT_TABLES EptTables);
PEPT_PML4E EptAllocatePml4();
PEPT_PDPTE EptAllocatePdpt();
PEPT_PDE EptAllocatePd();
PEPT_PTE EptAllocatePt();

NTSTATUS EptMapAddressRange(
    PEPT_TABLES EptTables,
    ULONG64 GuestPhysicalAddress,
    ULONG64 HostPhysicalAddress,
    ULONG64 Size,
    BOOLEAN Read,
    BOOLEAN Write,
    BOOLEAN Execute,
    ULONG MemoryType
);

PEPT_PTE EptGetPte(PEPT_TABLES EptTables, ULONG64 PhysicalAddress);
PEPT_PDE EptGetPde(PEPT_TABLES EptTables, ULONG64 PhysicalAddress);
PEPT_PDPTE EptGetPdpte(PEPT_TABLES EptTables, ULONG64 PhysicalAddress);
PEPT_PML4E EptGetPml4e(PEPT_TABLES EptTables, ULONG64 PhysicalAddress);

NTSTATUS EptSetPagePermissions(
    PEPT_TABLES EptTables,
    ULONG64 PhysicalAddress,
    BOOLEAN Read,
    BOOLEAN Write,
    BOOLEAN Execute
);

NTSTATUS EptHookPage(
    PEPT_TABLES EptTables,
    ULONG64 TargetPhysicalAddress,
    ULONG64 HookPhysicalAddress,
    PEPT_HOOK_INFO* HookInfo
);

NTSTATUS EptUnhookPage(
    PEPT_TABLES EptTables,
    PEPT_HOOK_INFO HookInfo
);

NTSTATUS EptHideMemory(
    PEPT_TABLES EptTables,
    ULONG64 GuestPhysicalAddress,
    ULONG64 Size,
    PEPT_HIDDEN_MEMORY* HiddenMemory
);

NTSTATUS EptUnhideMemory(
    PEPT_TABLES EptTables,
    PEPT_HIDDEN_MEMORY HiddenMemory
);

VOID EptHandleViolation(
    PEPT_TABLES EptTables,
    ULONG64 GuestPhysicalAddress,
    ULONG64 ExitQualification
);

NTSTATUS EptSplitLargePage(
    PEPT_TABLES EptTables,
    PEPT_PDE LargePde,
    ULONG PageSize
);

NTSTATUS EptInitializePtManager(PEPT_PT_MANAGER PtManager);
VOID EptTerminatePtManager(PEPT_PT_MANAGER PtManager);
PEPT_PT_ENTRY EptAllocatePtEntry(PEPT_PT_MANAGER PtManager, ULONG64 PhysicalAddress);
PEPT_PT_ENTRY EptFindPtEntry(PEPT_PT_MANAGER PtManager, ULONG64 PhysicalAddress);
VOID EptFreePtEntry(PEPT_PT_MANAGER PtManager, PEPT_PT_ENTRY PtEntry);
NTSTATUS EptSplit2MbTo4Kb(PEPT_TABLES EptTables, ULONG64 PhysicalAddress);

VOID EptInvalidateTlb();
VOID EptInvalidateContext(ULONG64 EptPointer);

ULONG64 EptGetPhysicalAddress(PVOID VirtualAddress);
ULONG64 EptGetPageFrameNumber(ULONG64 PhysicalAddress);
ULONG EptGetPml4Index(ULONG64 PhysicalAddress);
ULONG EptGetPdptIndex(ULONG64 PhysicalAddress);
ULONG EptGetPdIndex(ULONG64 PhysicalAddress);
ULONG EptGetPtIndex(ULONG64 PhysicalAddress);

NTSTATUS AsmInvept(ULONG Type, PVOID Descriptor);

extern PEPT_TABLES g_EptTables;
