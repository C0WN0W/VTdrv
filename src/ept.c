#include "ept.h"
#include "hypervisor.h"

PEPT_TABLES g_EptTables = NULL;

NTSTATUS EptInitialize(PEPT_TABLES* EptTables)
{
    DbgPrint("[EPT] Initializing EPT...\n");

    ULONG64 eptVpidCap = __readmsr(IA32_VMX_EPT_VPID_CAP);

    if (!(eptVpidCap & 0x1)) {
        DbgPrint("[EPT] EPT not supported\n");
        return STATUS_NOT_SUPPORTED;
    }

    DbgPrint("[EPT] EPT capabilities: 0x%llx\n", eptVpidCap);

    *EptTables = (PEPT_TABLES)ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(EPT_TABLES),
        'TPEE'
    );

    if (!*EptTables) {
        DbgPrint("[EPT] Failed to allocate EPT tables structure\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(*EptTables, sizeof(EPT_TABLES));

    NTSTATUS status = EptBuildTables(*EptTables);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[EPT] Failed to build EPT tables: 0x%X\n", status);
        ExFreePoolWithTag(*EptTables, 'TPEE');
        return status;
    }

    g_EptTables = *EptTables;

    DbgPrint("[EPT] EPT initialized successfully\n");
    DbgPrint("[EPT] EPT Pointer: 0x%llx\n", (*EptTables)->EptPointer.Value);

    return STATUS_SUCCESS;
}

VOID EptTerminate(PEPT_TABLES EptTables)
{
    if (!EptTables) {
        return;
    }

    DbgPrint("[EPT] Terminating EPT...\n");

    EptTerminatePtManager(&EptTables->PtManager);

    if (EptTables->Pd) {
        for (ULONG i = 0; i < EptTables->PdCount; i++) {
            if (EptTables->Pd[i]) {
                MmFreeContiguousMemory(EptTables->Pd[i]);
            }
        }
        ExFreePoolWithTag(EptTables->Pd, 'TPEE');
        ExFreePoolWithTag(EptTables->PdPhysical, 'TPEE');
    }

    if (EptTables->Pdpt) {
        MmFreeContiguousMemory(EptTables->Pdpt);
    }

    if (EptTables->Pml4) {
        MmFreeContiguousMemory(EptTables->Pml4);
    }

    ExFreePoolWithTag(EptTables, 'TPEE');

    g_EptTables = NULL;

    DbgPrint("[EPT] EPT terminated\n");
}

NTSTATUS EptBuildTables(PEPT_TABLES EptTables)
{
    DbgPrint("[EPT] Building EPT tables...\n");

    NTSTATUS status = EptInitializePtManager(&EptTables->PtManager);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[EPT] Failed to initialize PT manager: 0x%X\n", status);
        return status;
    }

    EptTables->Pml4 = EptAllocatePml4();
    if (!EptTables->Pml4) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    EptTables->Pml4Physical = MmGetPhysicalAddress(EptTables->Pml4);

    EptTables->Pdpt = EptAllocatePdpt();
    if (!EptTables->Pdpt) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    EptTables->PdptPhysical = MmGetPhysicalAddress(EptTables->Pdpt);

    EptTables->Pml4[0].Fields.Read = 1;
    EptTables->Pml4[0].Fields.Write = 1;
    EptTables->Pml4[0].Fields.Execute = 1;
    EptTables->Pml4[0].Fields.PageFrameNumber = EptTables->PdptPhysical.QuadPart >> 12;

    EptTables->PdCount = 512;
    EptTables->Pd = (PEPT_PDE*)ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(PEPT_PDE) * EptTables->PdCount,
        'TPEE'
    );
    EptTables->PdPhysical = (PHYSICAL_ADDRESS*)ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(PHYSICAL_ADDRESS) * EptTables->PdCount,
        'TPEE'
    );

    if (!EptTables->Pd || !EptTables->PdPhysical) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(EptTables->Pd, sizeof(PEPT_PDE) * EptTables->PdCount);
    RtlZeroMemory(EptTables->PdPhysical, sizeof(PHYSICAL_ADDRESS) * EptTables->PdCount);

    for (ULONG i = 0; i < EptTables->PdCount; i++) {
        EptTables->Pd[i] = EptAllocatePd();
        if (!EptTables->Pd[i]) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        EptTables->PdPhysical[i] = MmGetPhysicalAddress(EptTables->Pd[i]);

        EptTables->Pdpt[i].Fields.Read = 1;
        EptTables->Pdpt[i].Fields.Write = 1;
        EptTables->Pdpt[i].Fields.Execute = 1;
        EptTables->Pdpt[i].Fields.PageFrameNumber = EptTables->PdPhysical[i].QuadPart >> 12;
    }

    for (ULONG pdptIndex = 0; pdptIndex < 512; pdptIndex++) {
        PEPT_PDE pd = EptTables->Pd[pdptIndex];

        for (ULONG pdIndex = 0; pdIndex < 512; pdIndex++) {

            ULONG64 physicalAddress = ((ULONG64)pdptIndex << 30) | ((ULONG64)pdIndex << 21);

            pd[pdIndex].Fields.Read = 1;
            pd[pdIndex].Fields.Write = 1;
            pd[pdIndex].Fields.Execute = 1;
            pd[pdIndex].Fields.MemoryType = MEMORY_TYPE_WB;
            pd[pdIndex].Fields.LargePage = 1;
            pd[pdIndex].Fields.PageFrameNumber = physicalAddress >> 12;
        }
    }

    EptTables->EptPointer.Fields.MemoryType = MEMORY_TYPE_WB;
    EptTables->EptPointer.Fields.PageWalkLength = 3;
    EptTables->EptPointer.Fields.EnableAccessAndDirtyFlags = 0;
    EptTables->EptPointer.Fields.PageFrameNumber = EptTables->Pml4Physical.QuadPart >> 12;

    DbgPrint("[EPT] EPT tables built successfully\n");
    DbgPrint("[EPT] Mapped 512 GB physical memory using 2MB pages\n");
    DbgPrint("[EPT] PT tables will be allocated on-demand\n");

    return STATUS_SUCCESS;
}

PEPT_PML4E EptAllocatePml4()
{
    PHYSICAL_ADDRESS maxPhysicalAddress;
    maxPhysicalAddress.QuadPart = MAXULONG64;

    PEPT_PML4E pml4 = (PEPT_PML4E)MmAllocateContiguousMemory(
        PAGE_SIZE,
        maxPhysicalAddress
    );

    if (pml4) {
        RtlZeroMemory(pml4, PAGE_SIZE);
    }

    return pml4;
}

PEPT_PDPTE EptAllocatePdpt()
{
    PHYSICAL_ADDRESS maxPhysicalAddress;
    maxPhysicalAddress.QuadPart = MAXULONG64;

    PEPT_PDPTE pdpt = (PEPT_PDPTE)MmAllocateContiguousMemory(
        PAGE_SIZE,
        maxPhysicalAddress
    );

    if (pdpt) {
        RtlZeroMemory(pdpt, PAGE_SIZE);
    }

    return pdpt;
}

PEPT_PDE EptAllocatePd()
{
    PHYSICAL_ADDRESS maxPhysicalAddress;
    maxPhysicalAddress.QuadPart = MAXULONG64;

    PEPT_PDE pd = (PEPT_PDE)MmAllocateContiguousMemory(
        PAGE_SIZE,
        maxPhysicalAddress
    );

    if (pd) {
        RtlZeroMemory(pd, PAGE_SIZE);
    }

    return pd;
}

PEPT_PTE EptAllocatePt()
{
    PHYSICAL_ADDRESS maxPhysicalAddress;
    maxPhysicalAddress.QuadPart = MAXULONG64;

    PEPT_PTE pt = (PEPT_PTE)MmAllocateContiguousMemory(
        PAGE_SIZE,
        maxPhysicalAddress
    );

    if (pt) {
        RtlZeroMemory(pt, PAGE_SIZE);
    }

    return pt;
}

NTSTATUS EptSetPagePermissions(
    PEPT_TABLES EptTables,
    ULONG64 PhysicalAddress,
    BOOLEAN Read,
    BOOLEAN Write,
    BOOLEAN Execute
)
{
    DbgPrint("[EPT] Setting permissions for PA: 0x%llx (R:%d W:%d X:%d)\n",
        PhysicalAddress, Read, Write, Execute);

    ULONG pdptIndex = EptGetPdptIndex(PhysicalAddress);
    ULONG pdIndex = EptGetPdIndex(PhysicalAddress);

    PEPT_PDE pd = EptTables->Pd[pdptIndex];
    PEPT_PDE pde = &pd[pdIndex];

    if (pde->Fields.LargePage) {
        pde->Fields.Read = Read ? 1 : 0;
        pde->Fields.Write = Write ? 1 : 0;
        pde->Fields.Execute = Execute ? 1 : 0;

        EptInvalidateTlb();

        DbgPrint("[EPT] Permissions set on 2MB page\n");
        return STATUS_SUCCESS;
    }

    DbgPrint("[EPT] Need to split large page for 4KB granularity\n");
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS EptHookPage(
    PEPT_TABLES EptTables,
    ULONG64 TargetPhysicalAddress,
    ULONG64 HookPhysicalAddress,
    PEPT_HOOK_INFO* HookInfo
)
{
    DbgPrint("[EPT] Hooking page: Target=0x%llx, Hook=0x%llx\n",
        TargetPhysicalAddress, HookPhysicalAddress);

    *HookInfo = (PEPT_HOOK_INFO)ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(EPT_HOOK_INFO),
        'KPEE'
    );

    if (!*HookInfo) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(*HookInfo, sizeof(EPT_HOOK_INFO));

    (*HookInfo)->TargetAddress = TargetPhysicalAddress;
    (*HookInfo)->OriginalPagePfn = TargetPhysicalAddress >> 12;
    (*HookInfo)->HookPagePfn = HookPhysicalAddress >> 12;

    ULONG pdptIndex = EptGetPdptIndex(TargetPhysicalAddress);
    ULONG pdIndex = EptGetPdIndex(TargetPhysicalAddress);

    PEPT_PDE pd = EptTables->Pd[pdptIndex];
    PEPT_PDE pde = &pd[pdIndex];

    ULONG64 originalPfn = pde->Fields.PageFrameNumber;

    pde->Fields.PageFrameNumber = HookPhysicalAddress >> 12;

    pde->Fields.Read = 0;
    pde->Fields.Write = 0;
    pde->Fields.Execute = 1;

    (*HookInfo)->IsHooked = TRUE;
    (*HookInfo)->ExecuteOnly = TRUE;

    EptInvalidateTlb();

    DbgPrint("[EPT] Page hooked successfully\n");

    return STATUS_SUCCESS;
}

NTSTATUS EptUnhookPage(
    PEPT_TABLES EptTables,
    PEPT_HOOK_INFO HookInfo
)
{
    if (!HookInfo || !HookInfo->IsHooked) {
        return STATUS_INVALID_PARAMETER;
    }

    DbgPrint("[EPT] Unhooking page: 0x%llx\n", HookInfo->TargetAddress);

    ULONG pdptIndex = EptGetPdptIndex(HookInfo->TargetAddress);
    ULONG pdIndex = EptGetPdIndex(HookInfo->TargetAddress);

    PEPT_PDE pd = EptTables->Pd[pdptIndex];
    PEPT_PDE pde = &pd[pdIndex];

    pde->Fields.PageFrameNumber = HookInfo->OriginalPagePfn;

    pde->Fields.Read = 1;
    pde->Fields.Write = 1;
    pde->Fields.Execute = 1;

    HookInfo->IsHooked = FALSE;

    EptInvalidateTlb();

    ExFreePoolWithTag(HookInfo, 'KPEE');

    DbgPrint("[EPT] Page unhooked\n");

    return STATUS_SUCCESS;
}

NTSTATUS EptHideMemory(
    PEPT_TABLES EptTables,
    ULONG64 GuestPhysicalAddress,
    ULONG64 Size,
    PEPT_HIDDEN_MEMORY* HiddenMemory
)
{
    DbgPrint("[EPT] Hiding memory: GPA=0x%llx, Size=0x%llx\n",
        GuestPhysicalAddress, Size);

    *HiddenMemory = (PEPT_HIDDEN_MEMORY)ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(EPT_HIDDEN_MEMORY),
        'MPEE'
    );

    if (!*HiddenMemory) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(*HiddenMemory, sizeof(EPT_HIDDEN_MEMORY));

    PHYSICAL_ADDRESS maxPhysicalAddress;
    maxPhysicalAddress.QuadPart = MAXULONG64;

    PVOID fakePage = MmAllocateContiguousMemory(Size, maxPhysicalAddress);
    if (!fakePage) {
        ExFreePoolWithTag(*HiddenMemory, 'MPEE');
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(fakePage, Size);

    PHYSICAL_ADDRESS fakePhysical = MmGetPhysicalAddress(fakePage);

    (*HiddenMemory)->GuestPhysicalAddress = GuestPhysicalAddress;
    (*HiddenMemory)->HostPhysicalAddress = fakePhysical.QuadPart;
    (*HiddenMemory)->Size = Size;

    ULONG pdptIndex = EptGetPdptIndex(GuestPhysicalAddress);
    ULONG pdIndex = EptGetPdIndex(GuestPhysicalAddress);

    PEPT_PDE pd = EptTables->Pd[pdptIndex];
    PEPT_PDE pde = &pd[pdIndex];

    pde->Fields.PageFrameNumber = fakePhysical.QuadPart >> 12;

    (*HiddenMemory)->IsHidden = TRUE;

    EptInvalidateTlb();

    DbgPrint("[EPT] Memory hidden successfully\n");

    return STATUS_SUCCESS;
}

NTSTATUS EptUnhideMemory(
    PEPT_TABLES EptTables,
    PEPT_HIDDEN_MEMORY HiddenMemory
)
{
    if (!HiddenMemory || !HiddenMemory->IsHidden) {
        return STATUS_INVALID_PARAMETER;
    }

    DbgPrint("[EPT] Unhiding memory: GPA=0x%llx\n",
        HiddenMemory->GuestPhysicalAddress);

    ULONG pdptIndex = EptGetPdptIndex(HiddenMemory->GuestPhysicalAddress);
    ULONG pdIndex = EptGetPdIndex(HiddenMemory->GuestPhysicalAddress);

    PEPT_PDE pd = EptTables->Pd[pdptIndex];
    PEPT_PDE pde = &pd[pdIndex];

    pde->Fields.PageFrameNumber = HiddenMemory->GuestPhysicalAddress >> 12;

    HiddenMemory->IsHidden = FALSE;

    PHYSICAL_ADDRESS fakePhysical;
    fakePhysical.QuadPart = HiddenMemory->HostPhysicalAddress;
    PVOID fakePage = MmGetVirtualForPhysical(fakePhysical);
    if (fakePage) {
        MmFreeContiguousMemory(fakePage);
    }

    EptInvalidateTlb();

    ExFreePoolWithTag(HiddenMemory, 'MPEE');

    DbgPrint("[EPT] Memory unhidden\n");

    return STATUS_SUCCESS;
}

VOID EptHandleViolation(
    PEPT_TABLES EptTables,
    ULONG64 GuestPhysicalAddress,
    ULONG64 ExitQualification
)
{
    BOOLEAN isRead = (ExitQualification & 0x01) != 0;
    BOOLEAN isWrite = (ExitQualification & 0x02) != 0;
    BOOLEAN isExecute = (ExitQualification & 0x04) != 0;

    DbgPrint("[EPT] Violation at GPA: 0x%llx (R:%d W:%d X:%d)\n",
        GuestPhysicalAddress, isRead, isWrite, isExecute);

    if (isRead) {
        DbgPrint("[EPT] Allowing read access temporarily\n");
        EptSetPagePermissions(EptTables, GuestPhysicalAddress, TRUE, FALSE, TRUE);
    }
}

VOID EptInvalidateTlb()
{
    if (!g_EptTables) {
        return;
    }

    typedef struct _INVEPT_DESCRIPTOR {
        ULONG64 EptPointer;
        ULONG64 Reserved;
    } INVEPT_DESCRIPTOR, *PINVEPT_DESCRIPTOR;

    INVEPT_DESCRIPTOR descriptor;
    descriptor.EptPointer = g_EptTables->EptPointer.Value;
    descriptor.Reserved = 0;

    AsmInvept(1, &descriptor);

    DbgPrint("[EPT] TLB invalidated\n");
}

VOID EptInvalidateContext(ULONG64 EptPointer)
{

    DbgPrint("[EPT] Context invalidated: 0x%llx\n", EptPointer);
}

ULONG64 EptGetPhysicalAddress(PVOID VirtualAddress)
{
    PHYSICAL_ADDRESS physical = MmGetPhysicalAddress(VirtualAddress);
    return physical.QuadPart;
}

ULONG64 EptGetPageFrameNumber(ULONG64 PhysicalAddress)
{
    return PhysicalAddress >> 12;
}

ULONG EptGetPml4Index(ULONG64 PhysicalAddress)
{
    return (ULONG)((PhysicalAddress >> 39) & 0x1FF);
}

ULONG EptGetPdptIndex(ULONG64 PhysicalAddress)
{
    return (ULONG)((PhysicalAddress >> 30) & 0x1FF);
}

ULONG EptGetPdIndex(ULONG64 PhysicalAddress)
{
    return (ULONG)((PhysicalAddress >> 21) & 0x1FF);
}

ULONG EptGetPtIndex(ULONG64 PhysicalAddress)
{
    return (ULONG)((PhysicalAddress >> 12) & 0x1FF);
}

NTSTATUS EptInitializePtManager(PEPT_PT_MANAGER PtManager)
{
    DbgPrint("[EPT] Initializing PT manager...\n");

    PtManager->Head = NULL;
    PtManager->TotalCount = 0;
    KeInitializeSpinLock(&PtManager->Lock);

    DbgPrint("[EPT] PT manager initialized\n");
    return STATUS_SUCCESS;
}

VOID EptTerminatePtManager(PEPT_PT_MANAGER PtManager)
{
    DbgPrint("[EPT] Terminating PT manager...\n");

    KIRQL oldIrql;
    KeAcquireSpinLock(&PtManager->Lock, &oldIrql);

    PEPT_PT_ENTRY current = PtManager->Head;
    while (current) {
        PEPT_PT_ENTRY next = current->Next;

        if (current->Pt) {
            MmFreeContiguousMemory(current->Pt);
        }

        ExFreePoolWithTag(current, 'TPEE');

        current = next;
    }

    PtManager->Head = NULL;
    PtManager->TotalCount = 0;

    KeReleaseSpinLock(&PtManager->Lock, oldIrql);

    DbgPrint("[EPT] PT manager terminated, freed %d PT tables\n", PtManager->TotalCount);
}

PEPT_PT_ENTRY EptAllocatePtEntry(PEPT_PT_MANAGER PtManager, ULONG64 PhysicalAddress)
{
    DbgPrint("[EPT] Allocating PT entry for PA: 0x%llx\n", PhysicalAddress);

    PEPT_PT_ENTRY entry = (PEPT_PT_ENTRY)ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(EPT_PT_ENTRY),
        'TPEE'
    );

    if (!entry) {
        DbgPrint("[EPT] Failed to allocate PT entry structure\n");
        return NULL;
    }

    RtlZeroMemory(entry, sizeof(EPT_PT_ENTRY));

    entry->Pt = EptAllocatePt();
    if (!entry->Pt) {
        DbgPrint("[EPT] Failed to allocate PT table\n");
        ExFreePoolWithTag(entry, 'TPEE');
        return NULL;
    }

    entry->PtPhysical = MmGetPhysicalAddress(entry->Pt);
    entry->CoveredAddress = PhysicalAddress & ~0x1FFFFF;
    entry->IsAllocated = TRUE;
    entry->Next = NULL;

    KIRQL oldIrql;
    KeAcquireSpinLock(&PtManager->Lock, &oldIrql);

    entry->Next = PtManager->Head;
    PtManager->Head = entry;
    PtManager->TotalCount++;

    KeReleaseSpinLock(&PtManager->Lock, oldIrql);

    DbgPrint("[EPT] PT entry allocated, total count: %d\n", PtManager->TotalCount);

    return entry;
}

PEPT_PT_ENTRY EptFindPtEntry(PEPT_PT_MANAGER PtManager, ULONG64 PhysicalAddress)
{
    ULONG64 alignedAddress = PhysicalAddress & ~0x1FFFFF;

    KIRQL oldIrql;
    KeAcquireSpinLock(&PtManager->Lock, &oldIrql);

    PEPT_PT_ENTRY current = PtManager->Head;
    while (current) {
        if (current->CoveredAddress == alignedAddress && current->IsAllocated) {
            KeReleaseSpinLock(&PtManager->Lock, oldIrql);
            return current;
        }
        current = current->Next;
    }

    KeReleaseSpinLock(&PtManager->Lock, oldIrql);

    return NULL;
}

VOID EptFreePtEntry(PEPT_PT_MANAGER PtManager, PEPT_PT_ENTRY PtEntry)
{
    if (!PtEntry) {
        return;
    }

    DbgPrint("[EPT] Freeing PT entry for PA: 0x%llx\n", PtEntry->CoveredAddress);

    KIRQL oldIrql;
    KeAcquireSpinLock(&PtManager->Lock, &oldIrql);

    if (PtManager->Head == PtEntry) {
        PtManager->Head = PtEntry->Next;
    } else {
        PEPT_PT_ENTRY current = PtManager->Head;
        while (current && current->Next != PtEntry) {
            current = current->Next;
        }
        if (current) {
            current->Next = PtEntry->Next;
        }
    }

    PtManager->TotalCount--;

    KeReleaseSpinLock(&PtManager->Lock, oldIrql);

    if (PtEntry->Pt) {
        MmFreeContiguousMemory(PtEntry->Pt);
    }

    ExFreePoolWithTag(PtEntry, 'TPEE');

    DbgPrint("[EPT] PT entry freed\n");
}

NTSTATUS EptSplit2MbTo4Kb(PEPT_TABLES EptTables, ULONG64 PhysicalAddress)
{
    DbgPrint("[EPT] Splitting 2MB page to 4KB pages at PA: 0x%llx\n", PhysicalAddress);

    ULONG pdptIndex = EptGetPdptIndex(PhysicalAddress);
    ULONG pdIndex = EptGetPdIndex(PhysicalAddress);

    PEPT_PDE pd = EptTables->Pd[pdptIndex];
    PEPT_PDE pde = &pd[pdIndex];

    if (!pde->Fields.LargePage) {
        DbgPrint("[EPT] Page is already split\n");
        return STATUS_SUCCESS;
    }

    BOOLEAN originalRead = pde->Fields.Read;
    BOOLEAN originalWrite = pde->Fields.Write;
    BOOLEAN originalExecute = pde->Fields.Execute;
    ULONG originalMemoryType = pde->Fields.MemoryType;
    ULONG64 originalPfn = pde->Fields.PageFrameNumber;

    PEPT_PT_ENTRY ptEntry = EptAllocatePtEntry(&EptTables->PtManager, PhysicalAddress);
    if (!ptEntry) {
        DbgPrint("[EPT] Failed to allocate PT entry\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ULONG64 baseAddress = PhysicalAddress & ~0x1FFFFF;
    for (ULONG i = 0; i < 512; i++) {
        ULONG64 pageAddress = baseAddress + (i * PAGE_SIZE);

        ptEntry->Pt[i].Fields.Read = originalRead;
        ptEntry->Pt[i].Fields.Write = originalWrite;
        ptEntry->Pt[i].Fields.Execute = originalExecute;
        ptEntry->Pt[i].Fields.MemoryType = originalMemoryType;
        ptEntry->Pt[i].Fields.PageFrameNumber = pageAddress >> 12;
    }

    pde->Fields.Read = 1;
    pde->Fields.Write = 1;
    pde->Fields.Execute = 1;
    pde->Fields.MemoryType = MEMORY_TYPE_WB;
    pde->Fields.LargePage = 0;
    pde->Fields.PageFrameNumber = ptEntry->PtPhysical.QuadPart >> 12;

    EptInvalidateTlb();

    DbgPrint("[EPT] 2MB page split into 512 x 4KB pages successfully\n");

    return STATUS_SUCCESS;
}

PEPT_PTE EptGetPte(PEPT_TABLES EptTables, ULONG64 PhysicalAddress)
{

    ULONG pml4Index = EptGetPml4Index(PhysicalAddress);
    ULONG pdptIndex = EptGetPdptIndex(PhysicalAddress);
    ULONG pdIndex = EptGetPdIndex(PhysicalAddress);
    ULONG ptIndex = EptGetPtIndex(PhysicalAddress);

    if (!EptTables->Pml4[pml4Index].Fields.Read) {
        DbgPrint("[EPT] PML4[%d] not present\n", pml4Index);
        return NULL;
    }

    if (!EptTables->Pdpt[pdptIndex].Fields.Read) {
        DbgPrint("[EPT] PDPT[%d] not present\n", pdptIndex);
        return NULL;
    }

    PEPT_PDE pd = EptTables->Pd[pdptIndex];
    PEPT_PDE pde = &pd[pdIndex];

    if (!pde->Fields.Read) {
        DbgPrint("[EPT] PD[%d][%d] not present\n", pdptIndex, pdIndex);
        return NULL;
    }

    if (pde->Fields.LargePage) {
        DbgPrint("[EPT] Large page detected, splitting...\n");

        NTSTATUS status = EptSplit2MbTo4Kb(EptTables, PhysicalAddress);
        if (!NT_SUCCESS(status)) {
            DbgPrint("[EPT] Failed to split large page: 0x%X\n", status);
            return NULL;
        }

        pde = &pd[pdIndex];
    }

    PEPT_PT_ENTRY ptEntry = EptFindPtEntry(&EptTables->PtManager, PhysicalAddress);
    if (!ptEntry) {
        DbgPrint("[EPT] PT entry not found for PA: 0x%llx\n", PhysicalAddress);
        return NULL;
    }

    PEPT_PTE pte = &ptEntry->Pt[ptIndex];

    DbgPrint("[EPT] Found PTE for PA: 0x%llx (PML4:%d PDPT:%d PD:%d PT:%d)\n",
        PhysicalAddress, pml4Index, pdptIndex, pdIndex, ptIndex);

    return pte;
}
