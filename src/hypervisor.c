#include "hypervisor.h"
#include "ept.h"

HYPERVISOR_DATA g_HypervisorData = { 0 };
PVOID g_MsrBitmap = NULL;

NTSTATUS HvInitialize()
{
    DbgPrint("[HV] Initializing Hypervisor...\n");

    if (!HvCheckVmxSupport()) {
        DbgPrint("[HV] VMX not supported\n");
        return STATUS_NOT_SUPPORTED;
    }

    g_HypervisorData.ProcessorCount = KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
    DbgPrint("[HV] Processor count: %d\n", g_HypervisorData.ProcessorCount);

    g_HypervisorData.VcpuData = (PVCPU_DATA)ExAllocatePoolWithTag(
        NonPagedPool,
        sizeof(VCPU_DATA) * g_HypervisorData.ProcessorCount,
        'HVCP'
    );

    if (!g_HypervisorData.VcpuData) {
        DbgPrint("[HV] Failed to allocate VCPU data\n");
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(g_HypervisorData.VcpuData, sizeof(VCPU_DATA) * g_HypervisorData.ProcessorCount);

    g_MsrBitmap = ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, 'MSRB');
    if (!g_MsrBitmap) {
        DbgPrint("[HV] Failed to allocate MSR bitmap\n");
        ExFreePoolWithTag(g_HypervisorData.VcpuData, 'HVCP');
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(g_MsrBitmap, PAGE_SIZE);

    NTSTATUS eptStatus = EptInitialize(&g_EptTables);
    if (!NT_SUCCESS(eptStatus)) {
        DbgPrint("[HV] Failed to initialize EPT: 0x%X\n", eptStatus);
        ExFreePoolWithTag(g_MsrBitmap, 'MSRB');
        ExFreePoolWithTag(g_HypervisorData.VcpuData, 'HVCP');
        return eptStatus;
    }

    NTSTATUS status = HvVirtualizeAllProcessors();
    if (!NT_SUCCESS(status)) {
        DbgPrint("[HV] Failed to virtualize processors: 0x%X\n", status);
        EptTerminate(g_EptTables);
        ExFreePoolWithTag(g_MsrBitmap, 'MSRB');
        ExFreePoolWithTag(g_HypervisorData.VcpuData, 'HVCP');
        return status;
    }

    g_HypervisorData.IsActive = TRUE;
    DbgPrint("[HV] Hypervisor initialized successfully\n");

    return STATUS_SUCCESS;
}

VOID HvTerminate()
{
    if (!g_HypervisorData.IsActive) {
        return;
    }

    DbgPrint("[HV] Terminating Hypervisor...\n");

    for (ULONG i = 0; i < g_HypervisorData.ProcessorCount; i++) {
        HvDevirtualizeProcessor(i);
    }

    if (g_EptTables) {
        EptTerminate(g_EptTables);
        g_EptTables = NULL;
    }

    if (g_MsrBitmap) {
        ExFreePoolWithTag(g_MsrBitmap, 'MSRB');
        g_MsrBitmap = NULL;
    }

    if (g_HypervisorData.VcpuData) {
        ExFreePoolWithTag(g_HypervisorData.VcpuData, 'HVCP');
        g_HypervisorData.VcpuData = NULL;
    }

    g_HypervisorData.IsActive = FALSE;
    DbgPrint("[HV] Hypervisor terminated\n");
}

BOOLEAN HvIsActive()
{
    return g_HypervisorData.IsActive;
}

BOOLEAN HvCheckVmxSupport()
{
    int cpuInfo[4];

    __cpuid(cpuInfo, 1);

    if (!(cpuInfo[2] & (1 << 5))) {
        DbgPrint("[HV] VMX not supported by CPU\n");
        return FALSE;
    }

    ULONG64 featureControl = __readmsr(IA32_FEATURE_CONTROL);

    if (!(featureControl & 0x1)) {

        featureControl |= 0x1;
        featureControl |= 0x4;
        __writemsr(IA32_FEATURE_CONTROL, featureControl);
        DbgPrint("[HV] Enabled VMX in IA32_FEATURE_CONTROL\n");
    }

    featureControl = __readmsr(IA32_FEATURE_CONTROL);
    if (!(featureControl & 0x4)) {
        DbgPrint("[HV] VMX not enabled in BIOS\n");
        return FALSE;
    }

    DbgPrint("[HV] VMX supported and enabled\n");
    return TRUE;
}

NTSTATUS HvVirtualizeAllProcessors()
{
    for (ULONG i = 0; i < g_HypervisorData.ProcessorCount; i++) {

        GROUP_AFFINITY affinity = { 0 };
        GROUP_AFFINITY oldAffinity = { 0 };

        affinity.Group = 0;
        affinity.Mask = (KAFFINITY)(1ULL << i);

        KeSetSystemGroupAffinityThread(&affinity, &oldAffinity);

        NTSTATUS status = HvVirtualizeProcessor(i);

        KeRevertToUserGroupAffinityThread(&oldAffinity);

        if (!NT_SUCCESS(status)) {
            DbgPrint("[HV] Failed to virtualize CPU %d: 0x%X\n", i, status);

            for (ULONG j = 0; j < i; j++) {
                HvDevirtualizeProcessor(j);
            }

            return status;
        }

        DbgPrint("[HV] CPU %d virtualized\n", i);
    }

    return STATUS_SUCCESS;
}

NTSTATUS HvVirtualizeProcessor(ULONG processorNumber)
{
    PVCPU_DATA vcpu = &g_HypervisorData.VcpuData[processorNumber];
    vcpu->ProcessorNumber = processorNumber;

    vcpu->VmxonRegion = HvAllocateVmxRegion();
    if (!vcpu->VmxonRegion) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    vcpu->VmxonRegionPhysical = MmGetPhysicalAddress(vcpu->VmxonRegion);

    vcpu->VmcsRegion = HvAllocateVmxRegion();
    if (!vcpu->VmcsRegion) {
        HvFreeVmxRegion(vcpu->VmxonRegion);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    vcpu->VmcsRegionPhysical = MmGetPhysicalAddress(vcpu->VmcsRegion);

    vcpu->HostStack = ExAllocatePoolWithTag(NonPagedPool, 0x6000, 'HVST');
    if (!vcpu->HostStack) {
        HvFreeVmxRegion(vcpu->VmcsRegion);
        HvFreeVmxRegion(vcpu->VmxonRegion);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    vcpu->HostRsp = (ULONG_PTR)vcpu->HostStack + 0x6000 - 0x100;

    if (!HvEnableVmxOperation(vcpu)) {
        ExFreePoolWithTag(vcpu->HostStack, 'HVST');
        HvFreeVmxRegion(vcpu->VmcsRegion);
        HvFreeVmxRegion(vcpu->VmxonRegion);
        return STATUS_UNSUCCESSFUL;
    }

    if (!HvSetupVmcs(vcpu)) {
        HvDisableVmxOperation();
        ExFreePoolWithTag(vcpu->HostStack, 'HVST');
        HvFreeVmxRegion(vcpu->VmcsRegion);
        HvFreeVmxRegion(vcpu->VmxonRegion);
        return STATUS_UNSUCCESSFUL;
    }

    vcpu->IsVirtualized = TRUE;

    DbgPrint("[HV] CPU %d: VMCS setup complete (VMLAUNCH not implemented)\n", processorNumber);

    return STATUS_SUCCESS;
}

VOID HvDevirtualizeProcessor(ULONG processorNumber)
{
    PVCPU_DATA vcpu = &g_HypervisorData.VcpuData[processorNumber];

    if (!vcpu->IsVirtualized) {
        return;
    }

    GROUP_AFFINITY affinity = { 0 };
    GROUP_AFFINITY oldAffinity = { 0 };

    affinity.Group = 0;
    affinity.Mask = (KAFFINITY)(1ULL << processorNumber);

    KeSetSystemGroupAffinityThread(&affinity, &oldAffinity);

    HvDisableVmxOperation();

    if (vcpu->HostStack) {
        ExFreePoolWithTag(vcpu->HostStack, 'HVST');
    }

    if (vcpu->VmcsRegion) {
        HvFreeVmxRegion(vcpu->VmcsRegion);
    }

    if (vcpu->VmxonRegion) {
        HvFreeVmxRegion(vcpu->VmxonRegion);
    }

    vcpu->IsVirtualized = FALSE;

    KeRevertToUserGroupAffinityThread(&oldAffinity);

    DbgPrint("[HV] CPU %d devirtualized\n", processorNumber);
}

PVMX_REGION HvAllocateVmxRegion()
{
    PHYSICAL_ADDRESS maxPhysicalAddress;
    maxPhysicalAddress.QuadPart = MAXULONG64;

    PVMX_REGION region = (PVMX_REGION)MmAllocateContiguousMemory(
        PAGE_SIZE,
        maxPhysicalAddress
    );

    if (!region) {
        return NULL;
    }

    RtlZeroMemory(region, PAGE_SIZE);

    ULONG64 vmxBasic = __readmsr(IA32_VMX_BASIC);
    region->RevisionId = (ULONG)(vmxBasic & 0x7FFFFFFF);

    return region;
}

VOID HvFreeVmxRegion(PVMX_REGION region)
{
    if (region) {
        MmFreeContiguousMemory(region);
    }
}

BOOLEAN HvEnableVmxOperation(PVCPU_DATA vcpu)
{

    ULONG_PTR cr0 = __readcr0();
    ULONG64 vmxCr0Fixed0 = __readmsr(0x486);
    ULONG64 vmxCr0Fixed1 = __readmsr(0x487);

    cr0 &= vmxCr0Fixed1;
    cr0 |= vmxCr0Fixed0;
    __writecr0(cr0);

    ULONG_PTR cr4 = __readcr4();
    ULONG64 vmxCr4Fixed0 = __readmsr(0x488);
    ULONG64 vmxCr4Fixed1 = __readmsr(0x489);

    cr4 &= vmxCr4Fixed1;
    cr4 |= vmxCr4Fixed0;
    cr4 |= CR4_VMXE;
    __writecr4(cr4);

    if (__vmx_on(&vcpu->VmxonRegionPhysical.QuadPart) != VMX_OK) {
        DbgPrint("[HV] VMXON failed\n");
        return FALSE;
    }

    DbgPrint("[HV] VMX operation enabled\n");
    return TRUE;
}

VOID HvDisableVmxOperation()
{
    __vmx_off();

    ULONG_PTR cr4 = __readcr4();
    cr4 &= ~CR4_VMXE;
    __writecr4(cr4);
}

BOOLEAN HvSetupVmcs(PVCPU_DATA vcpu)
{

    if (__vmx_vmclear(&vcpu->VmcsRegionPhysical.QuadPart) != VMX_OK) {
        DbgPrint("[HV] VMCLEAR failed\n");
        return FALSE;
    }

    if (__vmx_vmptrld(&vcpu->VmcsRegionPhysical.QuadPart) != VMX_OK) {
        DbgPrint("[HV] VMPTRLD failed\n");
        return FALSE;
    }

    HvSetupVmcsControls();

    HvSetupVmcsGuestState();

    HvSetupVmcsHostState(vcpu);

    DbgPrint("[HV] VMCS setup complete\n");
    return TRUE;
}

VOID HvSetupVmcsControls()
{

    ULONG64 pinControls = __readmsr(IA32_VMX_PINBASED_CTLS);
    ULONG32 pinCtrlsValue = (ULONG32)(pinControls >> 32);

    __vmx_vmwrite(PIN_BASED_VM_EXEC_CONTROL, pinCtrlsValue);

    ULONG64 procControls = __readmsr(IA32_VMX_PROCBASED_CTLS);
    ULONG32 procCtrlsValue = (ULONG32)(procControls >> 32);

    procCtrlsValue |= (1 << 28);

    procCtrlsValue |= (1 << 31);

    __vmx_vmwrite(CPU_BASED_VM_EXEC_CONTROL, procCtrlsValue);

    ULONG64 procControls2 = __readmsr(IA32_VMX_PROCBASED_CTLS2);
    ULONG32 procCtrlsValue2 = (ULONG32)(procControls2 >> 32);

    procCtrlsValue2 |= (1 << 1);

    procCtrlsValue2 |= (1 << 3);

    procCtrlsValue2 |= (1 << 5);

    procCtrlsValue2 |= (1 << 7);

    procCtrlsValue2 |= (1 << 12);

    procCtrlsValue2 |= (1 << 20);

    __vmx_vmwrite(SECONDARY_VM_EXEC_CONTROL, procCtrlsValue2);

    ULONG64 exitControls = __readmsr(IA32_VMX_EXIT_CTLS);
    ULONG32 exitCtrlsValue = (ULONG32)(exitControls >> 32);

    exitCtrlsValue |= (1 << 9);

    exitCtrlsValue |= (1 << 15);

    exitCtrlsValue |= (1 << 18);

    exitCtrlsValue |= (1 << 19);

    exitCtrlsValue |= (1 << 20);

    exitCtrlsValue |= (1 << 21);

    __vmx_vmwrite(VM_EXIT_CONTROLS, exitCtrlsValue);

    ULONG64 entryControls = __readmsr(IA32_VMX_ENTRY_CTLS);
    ULONG32 entryCtrlsValue = (ULONG32)(entryControls >> 32);

    entryCtrlsValue |= (1 << 9);

    entryCtrlsValue |= (1 << 14);

    entryCtrlsValue |= (1 << 15);

    __vmx_vmwrite(VM_ENTRY_CONTROLS, entryCtrlsValue);

    __vmx_vmwrite(CR0_GUEST_HOST_MASK, 0);

    __vmx_vmwrite(CR4_GUEST_HOST_MASK, 0);

    __vmx_vmwrite(CR0_READ_SHADOW, 0);

    __vmx_vmwrite(CR4_READ_SHADOW, 0);

    __vmx_vmwrite(EXCEPTION_BITMAP, 0);

    __vmx_vmwrite(PAGE_FAULT_ERROR_CODE_MASK, 0);

    __vmx_vmwrite(PAGE_FAULT_ERROR_CODE_MATCH, 0);

    __vmx_vmwrite(CR3_TARGET_COUNT, 0);

    __vmx_vmwrite(TPR_THRESHOLD, 0);

    __vmx_vmwrite(VIRTUAL_PROCESSOR_ID, 1);

    if (g_EptTables) {
        __vmx_vmwrite(EPT_POINTER, g_EptTables->EptPointer.Value);
        __vmx_vmwrite(EPT_POINTER_HIGH, g_EptTables->EptPointer.Value >> 32);
    }

    if (g_MsrBitmap) {
        PHYSICAL_ADDRESS msrBitmapPhys = MmGetPhysicalAddress(g_MsrBitmap);
        __vmx_vmwrite(MSR_BITMAP, msrBitmapPhys.QuadPart);
        __vmx_vmwrite(MSR_BITMAP_HIGH, msrBitmapPhys.QuadPart >> 32);
    }
}

VOID HvSetupVmcsGuestState()
{
    SEGMENT_SELECTOR selector;
    DESCRIPTOR_TABLE_REGISTER gdtr, idtr;

    _sgdt(&gdtr);
    __sidt(&idtr);

    __vmx_vmwrite(GUEST_CR0, __readcr0());
    __vmx_vmwrite(GUEST_CR3, __readcr3());
    __vmx_vmwrite(GUEST_CR4, __readcr4());
    __vmx_vmwrite(GUEST_DR7, __readdr(7));

    HvGetSegmentDescriptor(&selector, __readcs(), (PUCHAR)gdtr.Base);
    __vmx_vmwrite(GUEST_CS_SELECTOR, selector.Selector);
    __vmx_vmwrite(GUEST_CS_BASE, selector.Base);
    __vmx_vmwrite(GUEST_CS_LIMIT, selector.Limit);
    __vmx_vmwrite(GUEST_CS_AR_BYTES, selector.AccessRights);

    HvGetSegmentDescriptor(&selector, __readss(), (PUCHAR)gdtr.Base);
    __vmx_vmwrite(GUEST_SS_SELECTOR, selector.Selector);
    __vmx_vmwrite(GUEST_SS_BASE, selector.Base);
    __vmx_vmwrite(GUEST_SS_LIMIT, selector.Limit);
    __vmx_vmwrite(GUEST_SS_AR_BYTES, selector.AccessRights);

    HvGetSegmentDescriptor(&selector, __readds(), (PUCHAR)gdtr.Base);
    __vmx_vmwrite(GUEST_DS_SELECTOR, selector.Selector);
    __vmx_vmwrite(GUEST_DS_BASE, selector.Base);
    __vmx_vmwrite(GUEST_DS_LIMIT, selector.Limit);
    __vmx_vmwrite(GUEST_DS_AR_BYTES, selector.AccessRights);

    HvGetSegmentDescriptor(&selector, __reades(), (PUCHAR)gdtr.Base);
    __vmx_vmwrite(GUEST_ES_SELECTOR, selector.Selector);
    __vmx_vmwrite(GUEST_ES_BASE, selector.Base);
    __vmx_vmwrite(GUEST_ES_LIMIT, selector.Limit);
    __vmx_vmwrite(GUEST_ES_AR_BYTES, selector.AccessRights);

    HvGetSegmentDescriptor(&selector, __readfs(), (PUCHAR)gdtr.Base);
    __vmx_vmwrite(GUEST_FS_SELECTOR, selector.Selector);
    __vmx_vmwrite(GUEST_FS_BASE, __readmsr(IA32_FS_BASE));
    __vmx_vmwrite(GUEST_FS_LIMIT, selector.Limit);
    __vmx_vmwrite(GUEST_FS_AR_BYTES, selector.AccessRights);

    HvGetSegmentDescriptor(&selector, __readgs(), (PUCHAR)gdtr.Base);
    __vmx_vmwrite(GUEST_GS_SELECTOR, selector.Selector);
    __vmx_vmwrite(GUEST_GS_BASE, __readmsr(IA32_GS_BASE));
    __vmx_vmwrite(GUEST_GS_LIMIT, selector.Limit);
    __vmx_vmwrite(GUEST_GS_AR_BYTES, selector.AccessRights);

    USHORT ldtr = __readldtr();
    if (ldtr != 0) {
        HvGetSegmentDescriptor(&selector, ldtr, (PUCHAR)gdtr.Base);
        __vmx_vmwrite(GUEST_LDTR_SELECTOR, selector.Selector);
        __vmx_vmwrite(GUEST_LDTR_BASE, selector.Base);
        __vmx_vmwrite(GUEST_LDTR_LIMIT, selector.Limit);
        __vmx_vmwrite(GUEST_LDTR_AR_BYTES, selector.AccessRights);
    }
    else {

        __vmx_vmwrite(GUEST_LDTR_SELECTOR, 0);
        __vmx_vmwrite(GUEST_LDTR_BASE, 0);
        __vmx_vmwrite(GUEST_LDTR_LIMIT, 0);
        __vmx_vmwrite(GUEST_LDTR_AR_BYTES, 0x10000);
    }

    HvGetSegmentDescriptor(&selector, __readtr(), (PUCHAR)gdtr.Base);
    __vmx_vmwrite(GUEST_TR_SELECTOR, selector.Selector);
    __vmx_vmwrite(GUEST_TR_BASE, selector.Base);
    __vmx_vmwrite(GUEST_TR_LIMIT, selector.Limit);
    __vmx_vmwrite(GUEST_TR_AR_BYTES, selector.AccessRights);

    __vmx_vmwrite(GUEST_GDTR_BASE, gdtr.Base);
    __vmx_vmwrite(GUEST_GDTR_LIMIT, gdtr.Limit);
    __vmx_vmwrite(GUEST_IDTR_BASE, idtr.Base);
    __vmx_vmwrite(GUEST_IDTR_LIMIT, idtr.Limit);

    __vmx_vmwrite(GUEST_RSP, (ULONG_PTR)_AddressOfReturnAddress());
    __vmx_vmwrite(GUEST_RIP, (ULONG_PTR)_ReturnAddress());
    __vmx_vmwrite(GUEST_RFLAGS, __readeflags());

    __vmx_vmwrite(GUEST_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS));
    __vmx_vmwrite(GUEST_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP));
    __vmx_vmwrite(GUEST_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP));

    __vmx_vmwrite(GUEST_IA32_DEBUGCTL, __readmsr(IA32_DEBUGCTL));
    __vmx_vmwrite(GUEST_IA32_DEBUGCTL_HIGH, __readmsr(IA32_DEBUGCTL) >> 32);

    __vmx_vmwrite(GUEST_ACTIVITY_STATE, 0);

    __vmx_vmwrite(GUEST_INTERRUPTIBILITY_INFO, 0);

    __vmx_vmwrite(GUEST_PENDING_DBG_EXCEPTIONS, 0);

    __vmx_vmwrite(VMCS_LINK_POINTER, 0xFFFFFFFFFFFFFFFF);
    __vmx_vmwrite(VMCS_LINK_POINTER_HIGH, 0xFFFFFFFF);
}

VOID HvSetupVmcsHostState(PVCPU_DATA vcpu)
{
    DESCRIPTOR_TABLE_REGISTER gdtr, idtr;
    SEGMENT_SELECTOR trSelector;

    _sgdt(&gdtr);
    __sidt(&idtr);

    __vmx_vmwrite(HOST_CR0, __readcr0());
    __vmx_vmwrite(HOST_CR3, __readcr3());
    __vmx_vmwrite(HOST_CR4, __readcr4());

    __vmx_vmwrite(HOST_CS_SELECTOR, __readcs() & 0xF8);

    __vmx_vmwrite(HOST_SS_SELECTOR, __readss() & 0xF8);

    __vmx_vmwrite(HOST_DS_SELECTOR, __readds() & 0xF8);

    __vmx_vmwrite(HOST_ES_SELECTOR, __reades() & 0xF8);

    __vmx_vmwrite(HOST_FS_SELECTOR, __readfs() & 0xF8);

    __vmx_vmwrite(HOST_GS_SELECTOR, __readgs() & 0xF8);

    __vmx_vmwrite(HOST_TR_SELECTOR, __readtr() & 0xF8);

    __vmx_vmwrite(HOST_FS_BASE, __readmsr(IA32_FS_BASE));

    __vmx_vmwrite(HOST_GS_BASE, __readmsr(IA32_GS_BASE));

    HvGetSegmentDescriptor(&trSelector, __readtr(), (PUCHAR)gdtr.Base);
    __vmx_vmwrite(HOST_TR_BASE, trSelector.Base);

    __vmx_vmwrite(HOST_GDTR_BASE, gdtr.Base);
    __vmx_vmwrite(HOST_IDTR_BASE, idtr.Base);

    __vmx_vmwrite(HOST_IA32_SYSENTER_CS, __readmsr(IA32_SYSENTER_CS));
    __vmx_vmwrite(HOST_IA32_SYSENTER_ESP, __readmsr(IA32_SYSENTER_ESP));
    __vmx_vmwrite(HOST_IA32_SYSENTER_EIP, __readmsr(IA32_SYSENTER_EIP));

    __vmx_vmwrite(HOST_IA32_PAT, __readmsr(IA32_PAT));
    __vmx_vmwrite(HOST_IA32_PAT_HIGH, __readmsr(IA32_PAT) >> 32);

    __vmx_vmwrite(HOST_IA32_EFER, __readmsr(IA32_EFER));
    __vmx_vmwrite(HOST_IA32_EFER_HIGH, __readmsr(IA32_EFER) >> 32);

    __vmx_vmwrite(HOST_RSP, vcpu->HostRsp);

    __vmx_vmwrite(HOST_RIP, (ULONG_PTR)HvVmExitHandler);
}

VOID HvGetSegmentDescriptor(PSEGMENT_SELECTOR selector, USHORT segmentSelector, PUCHAR gdtBase)
{
    PSEGMENT_DESCRIPTOR descriptor = (PSEGMENT_DESCRIPTOR)(gdtBase + (segmentSelector & ~0x7));

    selector->Selector = segmentSelector;
    selector->Base = descriptor->Base0 | (descriptor->Base1 << 16) | (descriptor->Base2 << 24);
    selector->Limit = descriptor->Limit0 | ((descriptor->Attributes2 & 0x0F) << 16);
    selector->AccessRights = HvGetSegmentAccessRights(segmentSelector);

    if (descriptor->Attributes2 & 0x80) {
        selector->Limit = (selector->Limit << 12) | 0xFFF;
    }
}

ULONG HvGetSegmentAccessRights(USHORT segmentSelector)
{
    ULONG accessRights = (__lar(segmentSelector) >> 8) & 0xF0FF;

    if ((segmentSelector & 0xFFFC) == 0) {
        accessRights |= 0x10000;
    }

    return accessRights;
}
