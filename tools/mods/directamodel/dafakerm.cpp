/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#include "rm.h"

#include "core/include/mgrmgr.h"
#include "core/include/modsdrv.h"
#include "gpu/include/gpumgr.h"
#include "opengl/modsgl.h"

#include "class/cl003e.h"
#include "class/cl003f.h"
#include "class/cl0040.h"
#include "class/cl0070.h"
#include "class/cl0071.h"
#include "class/cl208f.h"
#include "class/cl9096.h"
#include "class/clc46f.h"
#include "class/clc56f.h"
#include "class/clc86f.h"
#include "class/clc6b5.h"
#include "ctrl/ctrl0073.h"
#include "ctrl/ctrl5070.h"
#include "ctrl/ctrl906f.h"
#include "ctrl/ctrl9096.h"
#include "ctrl/ctrla06f.h"

#include "lwVer.h"

#include "lwDirectAmodel.h"

#ifdef WIN_DA
#define FAKERMEXPORT extern "C" __declspec(dllexport)
#else
#define FAKERMEXPORT extern "C"
#endif

const __GLLWChipInfo* GetAmodelChipInfo();

#define FAKE_GPUID 0x1000

namespace
{

LwHandle s_ClientHandleID = 1;
LwU32 s_MemoryHandleID = 1;

void *s_ResourceMutex = nullptr;

struct PointerWithParent
{
    void* pointer;
    LwHandle hParent;
};
using ObjectPointers = map<LwHandle, PointerWithParent>;

struct OffsetWithParent
{
    LwU64 offset;
    LwHandle hParent;
};
using ObjectOffsets = map<LwHandle, OffsetWithParent>;

struct SparseMapping
{
    LwU64 virtualAddress;
    LwU64 size;
};
using OffsetSparseMapping = map<LwU64, SparseMapping>;
using ObjectSparseMappings = map<LwHandle, OffsetSparseMapping>;

struct RMClient
{
    set<LwHandle> devices;
    ObjectPointers modsDrvAllocs;
    ObjectOffsets virtualMappings;
    map<LwHandle, pair<LwHandle, LwHandle> > duplicateObjects;
    LwU64 nextSparseVA = 64ULL << 30; // First allowed value by amodel,
                                      // also used in __lhglInitSparseSession.
    ObjectOffsets sparseVAs;
    ObjectSparseMappings sparseMappings;
};

map<LwHandle, RMClient> s_RMClients;

LwU32 AddMemoryAllocation
(
    LwHandle hClient,
    LwHandle hParent,
    LwHandle hMemory,
    void *pAddress
)
{
    Tasker::MutexHolder resourceMutex(s_ResourceMutex);

    auto &modsDrvAllocs = s_RMClients[hClient].modsDrvAllocs;

    if (modsDrvAllocs.count(hMemory) > 0)
    {
        MASSERT(!"Memory allocation already exists");
        return LW_ERR_ILWALID_OBJECT;
    }

    modsDrvAllocs[hMemory] = { pAddress, hParent };

    return LW_OK;
}

LwU32 AddAmodelVirtualMapping
(
    LwHandle hClient,
    LwHandle hParent,
    LwHandle hMemory,
    LwU64 gpuVirtualAddress
)
{
    Tasker::MutexHolder resourceMutex(s_ResourceMutex);

    auto &virtualMappings = s_RMClients[hClient].virtualMappings;

    if (virtualMappings.count(hMemory) > 0)
    {
        MASSERT(!"Mapping already exists");
        return LW_ERR_ILWALID_OBJECT;
    }

    virtualMappings[hMemory] = { gpuVirtualAddress, hParent };

    return LW_OK;
}

LwU32 AddSparseVA
(
    LwHandle hClient,
    LwHandle hParent,
    LwHandle hMemory,
    LwU64 size,
    LwU64 *virtualAddress
)
{
    Tasker::MutexHolder resourceMutex(s_ResourceMutex);

    auto &rmClient = s_RMClients[hClient];
    auto &sparseVAs = rmClient.sparseVAs;

    if (sparseVAs.count(hMemory) > 0)
    {
        MASSERT(!"Sparse VA already exists");
        return LW_ERR_ILWALID_OBJECT;
    }

    *virtualAddress = rmClient.nextSparseVA;
    sparseVAs[hMemory] = { *virtualAddress, hParent };
    rmClient.nextSparseVA += size;

    return LW_OK;
}

} // namespace

RM_BOOL RmInitialize()
{
    if (s_ResourceMutex)
    {
        MASSERT(!"RM already initialized");
        return FALSE;
    }

    s_ResourceMutex = Tasker::AllocMutex("DAFakeRM::s_ResourceMutex",
        Tasker::mtxUnchecked);

    return TRUE;
}

LW_STATUS RmFindDevices()
{
    PciDevInfo subdevice = {};
    subdevice.LinLwBase = nullptr;
    subdevice.PhysLwBase = 4;
    subdevice.PhysFbBase = 8;
    subdevice.PhysInstBase = 0;
    subdevice.LwApertureSize = 16 * 1024 * 1024;
    subdevice.FbApertureSize = 256 * 1024 * 1024;
    subdevice.InstApertureSize = 0;
    subdevice.Irq = 0;
    subdevice.PciClassCode = Pci::CLASS_CODE_UNDETERMINED;
    subdevice.PciDeviceId = 0x1f;
    subdevice.ChipId = 0x5678;
    subdevice.PciBus = 0;
    subdevice.PciDevice = 0;
    subdevice.PciFunction = 0;
    subdevice.SbiosBoot = 0;
    subdevice.GpuInstance = 0;
    subdevice.SocDeviceIndex = ~0U;
    GpuDevMgr* const pGpuDevMgr = dynamic_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    MASSERT(pGpuDevMgr);
    pGpuDevMgr->AddGpuSubdevice(subdevice);

    return LW_OK;
}

LW_STATUS RmInitGpu
(
    PciDevInfo       *pSubdevice,
    UINT08           *BiosImage,
    UINT32            BiosImageSize,
    const UINT08     *GspImage,
    const UINT32      GspImageSize,
    const UINT08     *GspLoggingImage,
    const UINT32      GspLoggingImageSize,
    UINT08           *NetlistImage,
    UINT32            NetlistImageSize,
    RM_BOOL           SkipRmStateInit,
    RM_BOOL           SkipVbiosPost,
    RM_BOOL           ForceRepost,
    RM_BOOL           HotSbrRecovery
)
{
    return LW_OK;
}

LW_STATUS RmInitDevice
(
    UINT32 DevInst
)
{
    return LW_OK;
}

RM_BOOL RmSetPowerState
(
    UINT32 DevInst,
    UINT32 PowerState
)
{
    return FALSE;
}

RM_BOOL RmDisableInterrupts(UINT32)
{
    return FALSE;
}

RM_BOOL RmDestroyDevice
(
    UINT32 DevInst
)
{
    return TRUE;
}

RM_BOOL RmDestroyGpu
(
    PciDevInfo*
)
{
    return TRUE;
}

static RM_BOOL CheckForResourcesLeft
(
    LwHandle clientId,
    const char *resourceName,
    size_t numResources
)
{
    if (numResources == 0)
        return TRUE;

    Printf(Tee::PriError,
        "DirectAmodel: Not all %s released for client %d, %zu left.\n",
        resourceName, clientId, numResources);

    return FALSE;
}

RM_BOOL RmDestroy()
{
    RM_BOOL ret = TRUE;

    if (s_ResourceMutex)
    {
        Tasker::FreeMutex(s_ResourceMutex);
        s_ResourceMutex = nullptr;
    }

    for (const auto& rmClient : s_RMClients)
    {
        ret &= CheckForResourcesLeft(rmClient.first, "memory allocations",
            rmClient.second.modsDrvAllocs.size());
        ret &= CheckForResourcesLeft(rmClient.first, "virtual mappings",
            rmClient.second.virtualMappings.size());
        ret &= CheckForResourcesLeft(rmClient.first, "duplicate objects",
            rmClient.second.duplicateObjects.size());
        ret &= CheckForResourcesLeft(rmClient.first, "sparse VAs",
            rmClient.second.sparseVAs.size());
        ret &= CheckForResourcesLeft(rmClient.first, "sparse mappings",
            rmClient.second.sparseMappings.size());
    }

    return ret;
}

LW_STATUS RmInitSoc(SocInitInfo*)
{
    return LW_ERR_GENERIC;
}

RM_BOOL RmDestroySoc(SocInitInfo*)
{
    return FALSE;
}

void RmRun1HzCallbackNow(UINT32)
{
}

RM_BOOL RmIsr
(
    UINT32
)
{
    return TRUE;
}

RM_BOOL RmIsrSoc
(
    UINT32,
    UINT32
)
{
    return TRUE;
}

void RmRearmMsi
(
    UINT32
)
{
}

RM_BOOL RmEnableVga
(
    UINT32  DeviceReference,
    RM_BOOL IsDefaultDisplayRestored
)
{
    return TRUE;
}

RM_BOOL RmPreModeSet
(
    UINT32 DevInst
)
{
    return TRUE;
}

RM_BOOL RmPostModeSet
(
    UINT32 DevInst
)
{
    return TRUE;
}

RM_BOOL RmAcquireSema()
{
    return FALSE;
}

RM_BOOL RmCondAcquireSema()
{
    return FALSE;
}

void RmReleaseSema()
{
}

FAKERMEXPORT LwU32 LwRmAllocRoot(LwHandle * phClient)
{
    *phClient = s_ClientHandleID++;
    return LW_OK;
}

FAKERMEXPORT LwU32 LwRmAlloc
(
    LwHandle    hClient,
    LwHandle    hParent,
    LwHandle    hObject,
    LwU32       hClass,
    void       *pAllocParms
)
{
    switch (hClass)
    {
        case LW01_DEVICE_0:
        {
            Tasker::MutexHolder resourceMutex(s_ResourceMutex);
            s_RMClients[hClient].devices.insert(hObject);
            break;
        }
        case LW20_SUBDEVICE_0:
        case LW20_SUBDEVICE_DIAG:
        case GF100_ZBC_CLEAR:
        case TURING_CHANNEL_GPFIFO_A:
        case AMPERE_CHANNEL_GPFIFO_A:
        case HOPPER_CHANNEL_GPFIFO_A:
        case LW01_MEMORY_VIRTUAL:
            break;
        default:
            dglDirectAmodelAllocObject(hParent, hObject, hClass);
            break;
    }

    return LW_OK;
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmAllocMemory64
(
    LwHandle    hClient,
    LwHandle    hParent,
    LwHandle    hMemory,
    LwU32       hClass,
    LwU32       flags,
    void      **ppAddress,
    LwU64      *pLimit
)
{
    LwU32 rmStatus = LW_ERR_OPERATING_SYSTEM;
    bool doAlloc = true;

    Printf(Tee::PriDebug, "LwRmAllocMemory64\n");
    Printf(Tee::PriDebug, "  hClient    = 0x%08x\n", hClient);
    Printf(Tee::PriDebug, "  hParent    = 0x%08x\n", hParent);
    Printf(Tee::PriDebug, "  hMemory    = 0x%08x\n", hMemory);
    Printf(Tee::PriDebug, "  hClass     = 0x%08x\n", hClass);
    Printf(Tee::PriDebug, "  flags      = 0x%08x\n", flags);
    Printf(Tee::PriDebug, "  *ppAddress = 0x%p\n", *ppAddress);
    Printf(Tee::PriDebug, "  *pLimit    = 0x%016llx\n", *pLimit);

    LwU64 size = *pLimit + 1;
    if (DRF_VAL(OS02, _FLAGS, _ALLOC, flags) == LWOS02_FLAGS_ALLOC_NONE)
    {
        // Query only
        doAlloc = false;
        size = 0;
    }

    if (*pLimit == 0)
    {
        doAlloc = false;
    }

    if (hClass != LW01_MEMORY_SYSTEM_OS_DESCRIPTOR)
    {
        *ppAddress = nullptr;
        *pLimit = 0;
    }

    switch (hClass)
    {
        case LW01_MEMORY_LOCAL_USER:
            // do not assume we ever allocate
            MASSERT(!doAlloc);
            if (doAlloc)
                return LW_ERR_ILWALID_HEAP;

            size = 1073741824;
            *pLimit = size - 1;
            rmStatus = LW_OK;
            break;

        case LW01_MEMORY_LOCAL_PRIVILEGED:
            if (!doAlloc)
                rmStatus = LW_OK;
            break;

        case LW01_MEMORY_SYSTEM:
            if (doAlloc)
            {
                *ppAddress = ModsDrvAlignedMalloc(UNSIGNED_CAST(UINT32, size + 4), 4096, 0);
                if (*ppAddress == nullptr)
                    return LW_ERR_NO_MEMORY;

                rmStatus = AddMemoryAllocation(hClient, hParent, hMemory, *ppAddress);
                if (rmStatus != LW_OK)
                    return rmStatus;

                const LwU64 gpuVirtualAddress =
                    dglDirectAmodelAllocVirtualMapping(hMemory, *ppAddress,
                        size + 4, 0, 0, 0);

                rmStatus = AddAmodelVirtualMapping(hClient, hParent, hMemory, gpuVirtualAddress);
                if (rmStatus != LW_OK)
                    return rmStatus;

                *pLimit = size - 1;
                rmStatus = LW_OK;
            }
            break;

        case LW01_MEMORY_SYSTEM_DYNAMIC:
            if (!doAlloc)
            {
                *pLimit = 0 - 1;
                rmStatus = LW_OK;
            }
            break;

        default:
            break;
    }

    Printf(Tee::PriDebug, " ret *ppAddress = 0x%p\n", *ppAddress);
    Printf(Tee::PriDebug, " ret *pLimit    = 0x%016llx\n", *pLimit);

    return rmStatus;
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmAllocObject
(
    LwHandle    hClient,
    LwHandle    hChannel,
    LwHandle    hObject,
    LwU32       hClass
)
{
    return LW_ERR_OPERATING_SYSTEM;
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmFree
(
    LwHandle hClient,
    LwHandle hParent,
    LwHandle hObject
)
{
    Tasker::MutexHolder resourceMutex(s_ResourceMutex);

    Printf(Tee::PriDebug, "LwRmFree\n");
    Printf(Tee::PriDebug, "  hClient    = 0x%08x\n", hClient);
    Printf(Tee::PriDebug, "  hParent    = 0x%08x\n", hParent);
    Printf(Tee::PriDebug, "  hObject    = 0x%08x\n", hObject);

    auto &rmClient = s_RMClients[hClient];

    const auto &dmaSparseMappings = rmClient.sparseMappings.find(hObject);
    if (dmaSparseMappings != rmClient.sparseMappings.end())
    {
        for (const auto &sparseMapping : dmaSparseMappings->second)
        {
            dglDirectAmodelUnmapSparseVA(sparseMapping.second.virtualAddress,
                sparseMapping.second.size, false);
        }
        rmClient.sparseMappings.erase(dmaSparseMappings);
    }

    const auto &sparseVA = rmClient.sparseVAs.find(hObject);
    if (sparseVA != rmClient.sparseVAs.end())
    {
        dglDirectAmodelFreeSparseVA(sparseVA->second.offset);
        rmClient.sparseVAs.erase(sparseVA);
    }

    const auto &modsDrvAlloc = rmClient.modsDrvAllocs.find(hObject);
    if (modsDrvAlloc != rmClient.modsDrvAllocs.end())
    {
        ModsDrvFree(modsDrvAlloc->second.pointer);
        rmClient.modsDrvAllocs.erase(modsDrvAlloc);
    }

    const auto &virtualMapping = rmClient.virtualMappings.find(hObject);
    if (virtualMapping != rmClient.virtualMappings.end())
    {
        dglDirectAmodelFreeVirtualMapping(virtualMapping->second.offset);
        rmClient.virtualMappings.erase(virtualMapping);
    }

    rmClient.duplicateObjects.erase(hObject);
    for (auto &rmClientIterator : s_RMClients)
    {
        for (const auto &dupObject : rmClientIterator.second.duplicateObjects)
        {
            if (dupObject.second == make_pair(hClient, hObject))
            {
                rmClientIterator.second.duplicateObjects.erase(dupObject.first);
                break;
            }
        }
    }

    if (rmClient.devices.find(hObject) != rmClient.devices.end())
    {
        rmClient.devices.erase(hObject);

        set<LwHandle> objectsToFree;
        for (const auto &sparseVAToFree : rmClient.sparseVAs)
        {
            if (sparseVAToFree.second.hParent == hObject)
            {
                objectsToFree.insert(sparseVAToFree.first);
            }
        }
        for (const auto &virtualMappingToFree : rmClient.virtualMappings)
        {
            if (virtualMappingToFree.second.hParent == hObject)
            {
                objectsToFree.insert(virtualMappingToFree.first);
            }
        }
        for (const auto &allocToFree : rmClient.modsDrvAllocs)
        {
            if (allocToFree.second.hParent == hObject)
            {
                objectsToFree.insert(allocToFree.first);
            }
        }
        for (const auto objectToErase : objectsToFree)
        {
            LwRmFree(hClient, hObject, objectToErase);
        }
    }

    return LW_OK;
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmAllocEvent
(
    LwHandle    hClient,
    LwHandle    hObjectParent,
    LwHandle    hObjectNew,
    LwU32       hClass,
    LwU32       index,
    void       *data
)
{
    return LW_ERR_OPERATING_SYSTEM;
}

static void PrintAllocSize(LWOS32_PARAMETERS *params)
{
    Printf(Tee::PriDebug, "LWOS32_FUNCTION_ALLOC_SIZE:\n");
    Printf(Tee::PriDebug, "  owner           = 0x%08x\n", params->data.AllocSize.owner);
    Printf(Tee::PriDebug, "  hMemory         = 0x%08x\n", params->data.AllocSize.hMemory);
    Printf(Tee::PriDebug, "  type            = 0x%08x\n", params->data.AllocSize.type);
    Printf(Tee::PriDebug, "  flags           = 0x%08x\n", params->data.AllocSize.flags);
    Printf(Tee::PriDebug, "  attr            = 0x%08x\n", params->data.AllocSize.attr);
    Printf(Tee::PriDebug, "  format          = 0x%08x\n", params->data.AllocSize.format);
    Printf(Tee::PriDebug, "  comprCovg       = 0x%08x\n", params->data.AllocSize.comprCovg);
    Printf(Tee::PriDebug, "  zlwllCovg       = 0x%08x\n", params->data.AllocSize.zlwllCovg);
    Printf(Tee::PriDebug, "  partitionStride = 0x%08x\n", params->data.AllocSize.partitionStride);
    Printf(Tee::PriDebug, "  width           = 0x%08x\n", params->data.AllocSize.width);
    Printf(Tee::PriDebug, "  height          = 0x%08x\n", params->data.AllocSize.height);
    Printf(Tee::PriDebug, "  size            = 0x%016llx\n", params->data.AllocSize.size);
    Printf(Tee::PriDebug, "  alignment       = 0x%016llx\n", params->data.AllocSize.alignment);
    Printf(Tee::PriDebug, "  offset          = 0x%016llx\n", params->data.AllocSize.offset);
    Printf(Tee::PriDebug, "  limit           = 0x%016llx\n", params->data.AllocSize.limit);
    Printf(Tee::PriDebug, "  address         = 0x%p\n", params->data.AllocSize.address);
    Printf(Tee::PriDebug, "  rangeBegin      = 0x%016llx\n", params->data.AllocSize.rangeBegin);
    Printf(Tee::PriDebug, "  rangeEnd        = 0x%016llx\n", params->data.AllocSize.rangeEnd);
    Printf(Tee::PriDebug, "  attr2           = 0x%08x\n", params->data.AllocSize.attr2);
    Printf(Tee::PriDebug, "  ctagOffset      = 0x%08x\n", params->data.AllocSize.ctagOffset);
}

static void PrintAllocTiledPitchHeight(LWOS32_PARAMETERS *params)
{
    Printf(Tee::PriDebug, "LWOS32_FUNCTION_ALLOC_TILED_PITCH_HEIGHT:\n");
    Printf(Tee::PriDebug, "  owner           = 0x%08x\n", params->data.AllocTiledPitchHeight.owner);
    Printf(Tee::PriDebug, "  hMemory         = 0x%08x\n", params->data.AllocTiledPitchHeight.hMemory);
    Printf(Tee::PriDebug, "  type            = 0x%08x\n", params->data.AllocTiledPitchHeight.type);
    Printf(Tee::PriDebug, "  flags           = 0x%08x\n", params->data.AllocTiledPitchHeight.flags);
    Printf(Tee::PriDebug, "  height          = 0x%08x\n", params->data.AllocTiledPitchHeight.height);
    Printf(Tee::PriDebug, "  pitch           = 0x%08x\n", params->data.AllocTiledPitchHeight.pitch);
    Printf(Tee::PriDebug, "  attr            = 0x%08x\n", params->data.AllocTiledPitchHeight.attr);
    Printf(Tee::PriDebug, "  width           = 0x%08x\n", params->data.AllocTiledPitchHeight.width);
    Printf(Tee::PriDebug, "  format          = 0x%08x\n", params->data.AllocTiledPitchHeight.format);
    Printf(Tee::PriDebug, "  comprCovg       = 0x%08x\n", params->data.AllocTiledPitchHeight.comprCovg);
    Printf(Tee::PriDebug, "  zlwllCovg       = 0x%08x\n", params->data.AllocTiledPitchHeight.zlwllCovg);
    Printf(Tee::PriDebug, "  partitionStride = 0x%08x\n", params->data.AllocTiledPitchHeight.partitionStride);
    Printf(Tee::PriDebug, "  size            = 0x%016llx\n", params->data.AllocTiledPitchHeight.size);
    Printf(Tee::PriDebug, "  alignment       = 0x%016llx\n", params->data.AllocTiledPitchHeight.alignment);
    Printf(Tee::PriDebug, "  offset          = 0x%016llx\n", params->data.AllocTiledPitchHeight.offset);
    Printf(Tee::PriDebug, "  limit           = 0x%016llx\n", params->data.AllocTiledPitchHeight.limit);
    Printf(Tee::PriDebug, "  address         = 0x%p\n", params->data.AllocTiledPitchHeight.address);
    Printf(Tee::PriDebug, "  rangeBegin      = 0x%016llx\n", params->data.AllocTiledPitchHeight.rangeBegin);
    Printf(Tee::PriDebug, "  rangeEnd        = 0x%016llx\n", params->data.AllocTiledPitchHeight.rangeEnd);
    Printf(Tee::PriDebug, "  attr2           = 0x%08x\n", params->data.AllocTiledPitchHeight.attr2);
    Printf(Tee::PriDebug, "  ctagOffset      = 0x%08x\n", params->data.AllocTiledPitchHeight.ctagOffset);
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmVidHeapControl
(
    void *pVidHeapControlParms
)
{
    LwU32 rmStatus = LW_ERR_OPERATING_SYSTEM;
    LWOS32_PARAMETERS *params = static_cast<LWOS32_PARAMETERS *>(pVidHeapControlParms);

    if (!(params->data.AllocSize.flags & LWOS32_ALLOC_FLAGS_MEMORY_HANDLE_PROVIDED))
    {
        const LwU32 hMemory = 0xcafe0000 ^ s_MemoryHandleID;
        s_MemoryHandleID++;

        params->data.AllocSize.hMemory = hMemory;
    }

    switch (params->function)
    {
        case LWOS32_FUNCTION_ALLOC_SIZE:
        {
            PrintAllocSize(params);

            if (params->data.AllocSize.flags & LWOS32_ALLOC_FLAGS_SPARSE)
            {
                rmStatus = AddSparseVA(params->hRoot, params->hObjectParent,
                    params->data.AllocSize.hMemory, params->data.AllocSize.size,
                    &params->data.AllocSize.offset);
                if (rmStatus != LW_OK)
                    return rmStatus;

                dglDirectAmodelAllocSparseVA(params->data.AllocSize.offset,
                    params->data.AllocSize.size);
            }
            else
            {
                void *ptr = ModsDrvAlignedMalloc(
                    UNSIGNED_CAST(UINT32, params->data.AllocSize.size + 4),
                    UNSIGNED_CAST(UINT32,
                        MAX(params->data.AllocSize.alignment, 4096)), 0);

                rmStatus = AddMemoryAllocation(params->hRoot, params->hObjectParent,
                    params->data.AllocSize.hMemory, ptr);
                if (rmStatus != LW_OK)
                    return rmStatus;

                params->data.AllocSize.offset =
                    dglDirectAmodelAllocVirtualMapping(params->data.AllocSize.hMemory,
                        ptr,
                        params->data.AllocSize.size + 4,
                        params->data.AllocSize.attr,
                        params->data.AllocSize.attr2,
                        params->data.AllocSize.type);

                rmStatus = AddAmodelVirtualMapping(params->hRoot, params->hObjectParent,
                    params->data.AllocSize.hMemory, params->data.AllocSize.offset);
                if (rmStatus != LW_OK)
                    return rmStatus;
            }

            params->data.AllocSize.limit = params->data.AllocSize.size - 1;
            rmStatus = LW_OK;

            Printf(Tee::PriDebug, "  ret offset      = 0x%016llx\n", params->data.AllocSize.offset);
            Printf(Tee::PriDebug, "  ret limit       = 0x%016llx\n", params->data.AllocSize.limit);

            break;
        }

        case LWOS32_FUNCTION_ALLOC_TILED_PITCH_HEIGHT:
        {
            PrintAllocTiledPitchHeight(params);

            params->data.AllocTiledPitchHeight.size =
                params->data.AllocTiledPitchHeight.height *
                params->data.AllocTiledPitchHeight.pitch;

            void *ptr = ModsDrvAlignedMalloc(
                UNSIGNED_CAST(UINT32, params->data.AllocTiledPitchHeight.size),
                UNSIGNED_CAST(UINT32,
                    MAX(params->data.AllocTiledPitchHeight.alignment, 4096)), 0);

            rmStatus = AddMemoryAllocation(params->hRoot, params->hObjectParent,
                params->data.AllocTiledPitchHeight.hMemory, ptr);
            if (rmStatus != LW_OK)
                return rmStatus;

            params->data.AllocTiledPitchHeight.offset =
                dglDirectAmodelAllocVirtualMapping(
                    params->data.AllocTiledPitchHeight.hMemory,
                    ptr,
                    params->data.AllocTiledPitchHeight.size,
                    params->data.AllocTiledPitchHeight.attr,
                    params->data.AllocTiledPitchHeight.attr2,
                    params->data.AllocTiledPitchHeight.type);

            rmStatus = AddAmodelVirtualMapping(params->hRoot, params->hObjectParent,
                params->data.AllocTiledPitchHeight.hMemory,
                params->data.AllocTiledPitchHeight.offset);
            if (rmStatus != LW_OK)
                return rmStatus;

            params->data.AllocTiledPitchHeight.limit = params->data.AllocTiledPitchHeight.size - 1;
            rmStatus = LW_OK;

            Printf(Tee::PriDebug, "  ret offset      = 0x%016llx\n", params->data.AllocTiledPitchHeight.offset);
            Printf(Tee::PriDebug, "  ret limit       = 0x%016llx\n", params->data.AllocTiledPitchHeight.limit);

            break;
        }

        default:
            MASSERT(!"Unsupported LwRmVidHeapControl function");
            break;
    }

    return rmStatus;
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmConfigGet
(
    LwHandle    hClient,
    LwHandle    hDevice,
    LwU32       index,
    LwU32      *pValue
)
{
    return LW_ERR_OPERATING_SYSTEM;
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmConfigSet
(
    LwHandle    hClient,
    LwHandle    hDevice,
    LwU32       index,
    LwU32       newValue,
    LwU32      *pOldValue
)
{
    return LW_ERR_OPERATING_SYSTEM;
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmConfigSetEx
(
    LwHandle    hClient,
    LwHandle    hDevice,
    LwU32       index,
    void       *paramStructPtr,
    LwU32       paramSize
)
{
    return LW_ERR_OPERATING_SYSTEM;
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmConfigGetEx
(
    LwHandle    hClient,
    LwHandle    hDevice,
    LwU32       index,
    void       *paramStructPtr,
    LwU32       paramSize
)
{
    return LW_ERR_OPERATING_SYSTEM;
}

FAKERMEXPORT LwU32 LwRmI2CAccess
(
    LwHandle    hClient,
    LwHandle    hDevice,
    void       *ctrlStructPtr
)
{
    return LW_ERR_OPERATING_SYSTEM;
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmIdleChannels
(
    LwHandle    hClient,
    LwHandle    hDevice,
    LwHandle    hChannel,
    LwU32       numChannels,
    LwHandle   *phClients,
    LwHandle   *phDevices,
    LwHandle   *phChannels,
    LwU32       flags,
    LwU32       timeout
)
{
    return LW_ERR_OPERATING_SYSTEM;
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmMapMemory
(
    LwHandle    hClient,
    LwHandle    hDevice,
    LwHandle    hMemory,
    LwU64       offset,
    LwU64       length,
    void      **ppLinearAddress,
    LwU32       flags
)
{
    Tasker::MutexHolder resourceMutex(s_ResourceMutex);

    Printf(Tee::PriDebug, "LwRmMapMemory\n");
    Printf(Tee::PriDebug, "  hClient          = 0x%08x\n", hClient);
    Printf(Tee::PriDebug, "  hDevice          = 0x%08x\n", hDevice);
    Printf(Tee::PriDebug, "  hMemory          = 0x%08x\n", hMemory);
    Printf(Tee::PriDebug, "  offset           = 0x%016llx\n", offset);
    Printf(Tee::PriDebug, "  length           = 0x%016llx\n", length);
    Printf(Tee::PriDebug, "  flags            = 0x%08x\n", flags);
    Printf(Tee::PriDebug, "  *ppLinearAddress = 0x%p\n", *ppLinearAddress);

    const auto &duplicateObjects = s_RMClients[hClient].duplicateObjects;
    const auto &dupObject = duplicateObjects.find(hMemory);
    if (dupObject != duplicateObjects.end())
    {
        hClient = dupObject->second.first;
        hMemory = dupObject->second.second;
    }

    *ppLinearAddress = static_cast<char *>(dglDirectAmodelAllocationPtr(hMemory)) + offset;

    Printf(Tee::PriDebug, " ret *ppLinearAddress = 0x%p\n", *ppLinearAddress);

    return LW_OK;
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmUnmapMemory
(
    LwHandle    hClient,
    LwHandle    hDevice,
    LwHandle    hMemory,
    void       *pLinearAddress,
    LwU32       flags
)
{
    return LW_OK;
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmAllocContextDma2
(
    LwHandle    hClient,
    LwHandle    hDma,
    LwU32       hClass,
    LwU32       flags,
    LwHandle    hMemory,
    LwU64       offset,
    LwU64       limit
)
{
    return LW_OK;
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmBindContextDma
(
    LwHandle hClient,
    LwHandle hChannel,
    LwHandle hCtxDma
)
{
    return LW_ERR_OPERATING_SYSTEM;
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmMapMemoryDma
(
    LwHandle    hClient,
    LwHandle    hDevice,
    LwHandle    hDma,
    LwHandle    hMemory,
    LwU64       offset,
    LwU64       length,
    LwU32       flags,
    LwU64      *pDmaOffset
)
{
    Tasker::MutexHolder resourceMutex(s_ResourceMutex);

    Printf(Tee::PriDebug, "LwRmMapMemoryDma\n");
    Printf(Tee::PriDebug, "  hClient     = 0x%08x\n", hClient);
    Printf(Tee::PriDebug, "  hDevice     = 0x%08x\n", hDevice);
    Printf(Tee::PriDebug, "  hDma        = 0x%08x\n", hDma);
    Printf(Tee::PriDebug, "  hMemory     = 0x%08x\n", hMemory);
    Printf(Tee::PriDebug, "  offset      = 0x%016llx\n", offset);
    Printf(Tee::PriDebug, "  length      = 0x%016llx\n", length);
    Printf(Tee::PriDebug, "  flags       = 0x%08x\n", flags);
    Printf(Tee::PriDebug, "  *pDmaOffset = 0x%016llx\n", *pDmaOffset);

    DEFER
    {
        Printf(Tee::PriDebug, " ret *pDmaOffset = 0x%016llx\n", *pDmaOffset);
    };

    auto &rmClient = s_RMClients[hClient];

    const auto &dupObject = rmClient.duplicateObjects.find(hMemory);
    if (dupObject != rmClient.duplicateObjects.end())
    {
        hClient = dupObject->second.first;
        hMemory = dupObject->second.second;
    }

    LwU64 virt = 0;

    const auto &sparseVA = rmClient.sparseVAs.find(hDma);
    if (sparseVA != rmClient.sparseVAs.end())
    {
        virt = sparseVA->second.offset;
    }
    else if (rmClient.modsDrvAllocs.find(hDma) != rmClient.modsDrvAllocs.end())
    {
        virt = dglDirectAmodelAllocatiolwAddr(hDma);
    }

    if (virt != 0)
    {
        const LwU64 pageOffset = offset & (Xp::GetPageSize() - 1);
        offset -= pageOffset;

        virt += *pDmaOffset;

        LwU64 phys = reinterpret_cast<LwU64>(dglDirectAmodelAllocationPtr(hMemory));
        phys += offset;

        dglDirectAmodelMapSparseVA(virt, phys, length + pageOffset);

        *pDmaOffset = virt + pageOffset;

        const SparseMapping sparseMapping = { virt, length + pageOffset };

        rmClient.sparseMappings[hDma][*pDmaOffset] = sparseMapping;

        return LW_OK;
    }

    *pDmaOffset = dglDirectAmodelAllocatiolwAddr(hMemory) + offset;

    if (*pDmaOffset == 0)
    {
        const auto &modsDrvAllocMemory = rmClient.modsDrvAllocs.find(hMemory);
        if (modsDrvAllocMemory != rmClient.modsDrvAllocs.end())
        {
            MASSERT(offset == 0);

            *pDmaOffset =
                dglDirectAmodelAllocVirtualMapping(hMemory,
                    modsDrvAllocMemory->second.pointer,
                    length,
                    0,
                    0,
                    0);

            const LwU32 rmStatus = AddAmodelVirtualMapping(hClient, hDevice,
                hMemory, *pDmaOffset);
            if (rmStatus != LW_OK)
                return rmStatus;
        }
    }

    return LW_OK;
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmUnmapMemoryDma
(
    LwHandle    hClient,
    LwHandle    hDevice,
    LwHandle    hDma,
    LwHandle    hMemory,
    LwU32       flags,
    LwU64       dmaOffset
)
{
    Tasker::MutexHolder resourceMutex(s_ResourceMutex);

    Printf(Tee::PriDebug, "LwRmUnmapMemoryDma\n");
    Printf(Tee::PriDebug, "  hClient   = 0x%08x\n", hClient);
    Printf(Tee::PriDebug, "  hDevice   = 0x%08x\n", hDevice);
    Printf(Tee::PriDebug, "  hDma      = 0x%08x\n", hDma);
    Printf(Tee::PriDebug, "  hMemory   = 0x%08x\n", hMemory);
    Printf(Tee::PriDebug, "  flags     = 0x%08x\n", flags);
    Printf(Tee::PriDebug, "  dmaOffset = 0x%016llx\n", dmaOffset);

    auto &sparseMappings = s_RMClients[hClient].sparseMappings;

    const auto &dmaSparseMappings = sparseMappings.find(hDma);
    if (dmaSparseMappings != sparseMappings.end())
    {
        const auto &sparseMapping = dmaSparseMappings->second.find(dmaOffset);
        if (sparseMapping != dmaSparseMappings->second.end())
        {
            dglDirectAmodelUnmapSparseVA(sparseMapping->second.virtualAddress,
                sparseMapping->second.size, false);
            dmaSparseMappings->second.erase(sparseMapping);
        }
    }

    dglDirectAmodelFreeVirtualMapping(dmaOffset);

    return LW_OK;
}

static UINT32 GetNumGpcs()
{
    const LwU32 *info = GetAmodelChipInfo()->keyValueGR2080;
    const LwU32 size = GetAmodelChipInfo()->sizeKeyValueGR2080;

    for (LwU32 j = 0; j < size; j += 2)
    {
        if (info[j] == LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_GPCS)
        {
            return info[j + 1];
        }
    }

    return 1;
}

static UINT32 GetNumTpcs()
{
    const LwU32 *info = GetAmodelChipInfo()->keyValueGR2080;
    const LwU32 size = GetAmodelChipInfo()->sizeKeyValueGR2080;

    for (LwU32 j = 0; j < size; j += 2)
    {
        if (info[j] == LW2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_SUB_COUNT)
        {
            return info[j + 1];
        }
    }

    return GetNumGpcs();
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmControl
(
    LwHandle    hClient,
    LwHandle    hObject,
    LwU32       cmd,
    void       *pParams,
    LwU32       paramsSize
)
{
    LwU32 rmStatus = LW_OK;
    switch (cmd)
    {
        case LW0000_CTRL_CMD_GPU_ATTACH_IDS:
        case LWA06F_CTRL_CMD_BIND:
        case LWA06F_CTRL_CMD_GPFIFO_SCHEDULE:
            break;

        case LW0000_CTRL_CMD_SYSTEM_NOTIFY_EVENT:
        case LW0073_CTRL_CMD_SYSTEM_GET_DMI_SCANLINE:
        case LW0073_CTRL_CMD_SYSTEM_GET_SCANLINE:
        case LW0080_CTRL_CMD_PERF_ADJUST_LIMIT_BY_PERFORMANCE:
        case LW2080_CTRL_CMD_BIOS_GET_INFO:
        case LW2080_CTRL_CMD_BIOS_GET_NBSI:
        case LW2080_CTRL_CMD_BIOS_GET_SKU_INFO:
        case LW2080_CTRL_CMD_BUS_CLEAR_PEX_COUNTERS:
        case LW2080_CTRL_CMD_FB_GET_OFFLINED_PAGES:
        case LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO:
        case LW2080_CTRL_CMD_GPU_EXEC_REG_OPS:
        case LW2080_CTRL_CMD_GPU_GET_SW_FEATURES:
        case LW2080_CTRL_CMD_GPU_QUERY_ECC_CONFIGURATION:
        case LW2080_CTRL_CMD_GPU_QUERY_ECC_STATUS:
        case LW2080_CTRL_CMD_PERF_GET_POWER_CONNECTOR_STATUS:
        case LW2080_CTRL_CMD_PERF_GET_PSTATES_INFO:
        case LW2080_CTRL_CMD_PERF_SET_FORCE_PSTATE:
        case LW2080_CTRL_CMD_PERF_SET_POWERSTATE:
        case LW208F_CTRL_CMD_GPU_VERIFY_INFOROM:
        case LW2080_CTRL_CMD_FLCN_GET_DMEM_USAGE:
        case LW0080_CTRL_CMD_BSP_GET_CAPS:
        case LW0080_CTRL_CMD_MSENC_GET_CAPS:
        case LW2080_CTRL_CMD_GPU_GET_PARTITIONS:
        case LW2080_CTRL_CMD_GPU_SET_PARTITIONS:
        case LW2080_CTRL_CMD_GPU_CONFIGURE_PARTITION:
        case LW2080_CTRL_CMD_BIOS_GET_OEM_INFO:
        case LW2080_CTRL_CMD_EVENT_SET_MEMORY_NOTIFIES:
        case LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION:
            rmStatus = LW_ERR_NOT_SUPPORTED;
            break;

        case LW0041_CTRL_CMD_GET_SURFACE_PARTITION_STRIDE:
        case LW0073_CTRL_CMD_SYSTEM_GET_ACTIVE:
        case LW0073_CTRL_CMD_SYSTEM_GET_CAPS_V2:
        case LW2080_CTRL_CMD_CLK_GET_DOMAINS:
        case LW2080_CTRL_CMD_CLK_GET_INFO:
        case LW2080_CTRL_CMD_GPU_IS_GRID_SDK_QUALIFIED_GPU:
        case LW2080_CTRL_CMD_GR_GET_TILE_HEIGHT:
        case LW2080_CTRL_CMD_PERF_BOOST:
        case LW9096_CTRL_CMD_GET_ZBC_CLEAR_TABLE:
        case LW9096_CTRL_CMD_GET_ZBC_CLEAR_TABLE_SIZE:
            rmStatus = LW_ERR_OPERATING_SYSTEM;
            break;

        case LW0073_CTRL_CMD_SYSTEM_GET_VBLANK_COUNTER:
            rmStatus = LW_ERR_CARD_NOT_PRESENT;
            break;

        case LW0000_CTRL_CMD_GPU_GET_ATTACHED_IDS:
        {
            LW0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS *attachedIdsParams =
                static_cast<LW0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS *>(pParams);

            for (unsigned int &gpuId : attachedIdsParams->gpuIds)
            {
                gpuId = LW0000_CTRL_GPU_ILWALID_ID;
            }
            attachedIdsParams->gpuIds[0] = FAKE_GPUID;
            break;
        }

        case LW0000_CTRL_CMD_GPU_GET_DEVICE_IDS:
        {
            LW0000_CTRL_GPU_GET_DEVICE_IDS_PARAMS *deviceIdsParams =
                static_cast<LW0000_CTRL_GPU_GET_DEVICE_IDS_PARAMS *>(pParams);
            deviceIdsParams->deviceIds = 1;
            break;
        }

        case LW0000_CTRL_CMD_GPU_GET_ID_INFO:
        {
            LW0000_CTRL_GPU_GET_ID_INFO_PARAMS *idInfoParams =
                static_cast<LW0000_CTRL_GPU_GET_ID_INFO_PARAMS *>(pParams);
            memset(idInfoParams, 0, sizeof(*idInfoParams));
            idInfoParams->gpuId = FAKE_GPUID;
            idInfoParams->gpuFlags = DRF_DEF(0000, _CTRL_GPU_ID_INFO, _MOBILE, _FALSE);
            break;
        }

        case LW0000_CTRL_CMD_GPU_GET_ID_INFO_V2:
        {
            LW0000_CTRL_GPU_GET_ID_INFO_V2_PARAMS *idInfoParams =
                static_cast<LW0000_CTRL_GPU_GET_ID_INFO_V2_PARAMS *>(pParams);
            memset(idInfoParams, 0, sizeof(*idInfoParams));
            idInfoParams->gpuId = FAKE_GPUID;
            idInfoParams->gpuFlags = DRF_DEF(0000, _CTRL_GPU_ID_INFO, _MOBILE, _FALSE);
            break;
        }

        case LW0000_CTRL_CMD_GPU_GET_UUID_FROM_GPU_ID:
        {
            LW0000_CTRL_GPU_GET_UUID_FROM_GPU_ID_PARAMS* uuidParams =
                static_cast<LW0000_CTRL_GPU_GET_UUID_FROM_GPU_ID_PARAMS*>(pParams);
            static const LwU8 FAKE_SHA1_UUID[] =
            {
                255, 255, 255, 255, 255, 255, 255, 255,
                255, 255, 255, 255, 255, 255, 255, 255,
            };
            const LwU32 flagsFormat =
                DRF_VAL(0000_CTRL_CMD_GPU, _GET_UUID_FROM_GPU_ID, _FLAGS_FORMAT,
                    uuidParams->flags);

            switch (flagsFormat)
            {
                case LW0000_CTRL_CMD_GPU_GET_UUID_FROM_GPU_ID_FLAGS_FORMAT_BINARY:
                    memcpy(uuidParams->gpuUuid, FAKE_SHA1_UUID, sizeof(FAKE_SHA1_UUID));
                    uuidParams->uuidStrLen = sizeof(FAKE_SHA1_UUID);
                    break;

                default:
                    return LW_ERR_NOT_SUPPORTED;
            }
            break;
        }

        case LW0000_CTRL_CMD_GSYNC_GET_ATTACHED_IDS:
        {
            LW0000_CTRL_GSYNC_GET_ATTACHED_IDS_PARAMS *attachedGsyncParams =
                static_cast<LW0000_CTRL_GSYNC_GET_ATTACHED_IDS_PARAMS *>(pParams);
            for (unsigned int &gsyncId : attachedGsyncParams->gsyncIds)
            {
                gsyncId = LW0000_CTRL_GSYNC_ILWALID_ID;
            }
            break;
        }

        case LW0000_CTRL_CMD_SYSTEM_GET_BUILD_VERSION:
        {
            LW0000_CTRL_SYSTEM_GET_BUILD_VERSION_PARAMS *buildVersionParams =
                static_cast<LW0000_CTRL_SYSTEM_GET_BUILD_VERSION_PARAMS *>(pParams);

            if ((buildVersionParams->pVersionBuffer == nullptr) ||
                (buildVersionParams->pTitleBuffer == nullptr) ||
                (buildVersionParams->pDriverVersionBuffer == nullptr))
            {
                buildVersionParams->sizeOfStrings = 80;
            }
            else
            {
                strcpy(static_cast<char*>(buildVersionParams->pVersionBuffer), LW_VERSION_STRING);
                strcpy(static_cast<char*>(buildVersionParams->pTitleBuffer), "unknown");
                strcpy(static_cast<char*>(buildVersionParams->pDriverVersionBuffer), LW_VERSION_STRING);
                buildVersionParams->changelistNumber = 1;
                buildVersionParams->officialChangelistNumber = 1;
            }
            break;
        }

        case LW0000_CTRL_CMD_SYSTEM_GET_CHIPSET_INFO:
        {
            LW0000_CTRL_SYSTEM_GET_CHIPSET_INFO_PARAMS *chipsetParams =
                static_cast<LW0000_CTRL_SYSTEM_GET_CHIPSET_INFO_PARAMS *>(pParams);
            memset(chipsetParams, 0, sizeof(LW0000_CTRL_SYSTEM_GET_CHIPSET_INFO_PARAMS));
            break;
        }

        case LW0000_CTRL_CMD_SYSTEM_GET_CPU_INFO:
        {
            LW0000_CTRL_SYSTEM_GET_CPU_INFO_PARAMS* temp =
                static_cast<LW0000_CTRL_SYSTEM_GET_CPU_INFO_PARAMS*>(pParams);
            temp->numLogicalCpus = 1;
            temp->dataCacheLineSize = 1;
            temp->capabilities |= LW0000_CTRL_SYSTEM_CPU_CAP_SSE;
            temp->capabilities |= LW0000_CTRL_SYSTEM_CPU_CAP_SFENCE;
            break;
        }

        case LW0041_CTRL_CMD_GET_SURFACE_ZLWLL_ID:
        {
            LwU32 *val = static_cast<LwU32*>(pParams);
            *val = 0;
            break;
        }

        case LW2080_CTRL_CMD_GR_GET_ZLWLL_INFO:
        {
            if (GetAmodelChipInfo()->zlwllInfoParams == nullptr)
            {
                rmStatus = LWOS_STATUS_ERROR_NOT_SUPPORTED;
                break;
            }
            memcpy(pParams, GetAmodelChipInfo()->zlwllInfoParams,
                sizeof(LW2080_CTRL_GR_GET_ZLWLL_INFO_PARAMS));
            break;
        }

        case LW2080_CTRL_CMD_GR_CTXSW_ZLWLL_MODE:
        {
            if (GetAmodelChipInfo()->zlwllInfoParams == nullptr)
            {
                rmStatus = LWOS_STATUS_ERROR_NOT_SUPPORTED;
            }
            break;
        }

        case LW0080_CTRL_CMD_DMA_ADV_SCHED_GET_VA_CAPS:
        {
            LW0080_CTRL_DMA_ADV_SCHED_GET_VA_CAPS_PARAMS* dmaVACapsParams =
                static_cast<LW0080_CTRL_DMA_ADV_SCHED_GET_VA_CAPS_PARAMS*>(pParams);
            memset(dmaVACapsParams, 0, sizeof(*dmaVACapsParams));
            dmaVACapsParams->bigPageSize = 64 * 1024;
            dmaVACapsParams->pdeCoverageBitCount = 26; // 64MB
            break;
        }

        case LW0080_CTRL_CMD_DMA_GET_CAPS:
        {
            LW0080_CTRL_DMA_GET_CAPS_PARAMS *dmaCapsParams =
                static_cast<LW0080_CTRL_DMA_GET_CAPS_PARAMS *>(pParams);
            memset(dmaCapsParams->capsTbl, 0, dmaCapsParams->capsTblSize);
            const LwU32 count = GetAmodelChipInfo()->sizeKeyValueDMA0080;
            LwU8* pCapsTbl = static_cast<LwU8*>(dmaCapsParams->capsTbl);

            for (LwU32 i = 0; i < count; i++)
            {
                const __GLLWKey0080* key = &(GetAmodelChipInfo()->keyValueDMA0080[i]);
                pCapsTbl[key->idx] = (pCapsTbl[key->idx] & ~(key->mask)) | key->value;
            }
            break;
        }

        case LW0080_CTRL_CMD_FB_GET_CAPS:
        {
            LW0080_CTRL_FB_GET_CAPS_PARAMS *fbCapsParams =
                static_cast<LW0080_CTRL_FB_GET_CAPS_PARAMS *>(pParams);
            memset(fbCapsParams->capsTbl, 0, fbCapsParams->capsTblSize);
            const LwU32 count = GetAmodelChipInfo()->sizeKeyValueFB0080;
            LwU8* pCapsTbl = static_cast<LwU8*>(fbCapsParams->capsTbl);

            for (LwU32 i = 0; i < count; i++)
            {
                const __GLLWKey0080* key = &(GetAmodelChipInfo()->keyValueFB0080[i]);
                pCapsTbl[key->idx] = (pCapsTbl[key->idx] & ~(key->mask)) | key->value;
            }
            break;
        }

        case LW0080_CTRL_CMD_FIFO_GET_CAPS:
        {
            LW0080_CTRL_FIFO_GET_CAPS_PARAMS *fifoCapsParams =
                static_cast<LW0080_CTRL_FIFO_GET_CAPS_PARAMS *>(pParams);
            memset(fifoCapsParams->capsTbl, 0, fifoCapsParams->capsTblSize);
            const LwU32 count = GetAmodelChipInfo()->sizeKeyValueFIFO0080;
            LwU8* pCapsTbl = static_cast<LwU8*>(fifoCapsParams->capsTbl);

            for (LwU32 i = 0; i < count; i++)
            {
                const __GLLWKey0080* key = &(GetAmodelChipInfo()->keyValueFIFO0080[i]);
                pCapsTbl[key->idx] = (pCapsTbl[key->idx] & ~(key->mask)) | key->value;
            }
            break;
        }

        case LW0080_CTRL_CMD_GPU_GET_CLASSLIST:
        {
            // Call V2 version which has real implementation.
            LW0080_CTRL_GPU_GET_CLASSLIST_V2_PARAMS v2params = { };
            rmStatus = LwRmControl(hClient, hObject,
                                    LW0080_CTRL_CMD_GPU_GET_CLASSLIST_V2,
                                    &v2params, sizeof(v2params));
            // If successful, translate back to V1 params.
            if (rmStatus == LW_OK) {
                LW0080_CTRL_GPU_GET_CLASSLIST_PARAMS *getClassListParams =
                    static_cast<LW0080_CTRL_GPU_GET_CLASSLIST_PARAMS *>(pParams);
                if (getClassListParams->numClasses == 0)
                {
                    getClassListParams->numClasses = v2params.numClasses;
                }
                else if (getClassListParams->numClasses == v2params.numClasses)
                {
                    memcpy(LwP64_VALUE(getClassListParams->classList),
                        v2params.classList,
                        v2params.numClasses * sizeof(v2params.classList[0]));
                }
                else
                {
                    rmStatus = LW_ERR_ILWALID_PARAMETER;
                }
            }
            break;
        }

        case LW0080_CTRL_CMD_GPU_GET_CLASSLIST_V2:
        {
            // This is just to satisfy ::IsSupported of some tests,
            // may not be matching current chip.
            static const UINT32 moreClasses[] =
            {
                0xC6B0, // LWDEC
                0xC7B7, // LWENC
                0xC4D1, // LWJPG
                0xC6FA, // OFA
            };

            const LwU32 numClasses = static_cast<LwU32>(NUMELEMS(moreClasses)) +
                GetAmodelChipInfo()->numClasses;
            LW0080_CTRL_GPU_GET_CLASSLIST_V2_PARAMS *getClassListParams =
                static_cast<LW0080_CTRL_GPU_GET_CLASSLIST_V2_PARAMS *>(pParams);

            MASSERT(numClasses <= NUMELEMS(getClassListParams->classList));
            getClassListParams->numClasses = numClasses;
            memcpy(&getClassListParams->classList,
                moreClasses,
                sizeof(moreClasses));
            memcpy(reinterpret_cast<UINT08*>(
                &getClassListParams->classList) + sizeof(moreClasses),
                GetAmodelChipInfo()->classes,
                GetAmodelChipInfo()->numClasses * sizeof(LwU32));
            break;
        }

        case LW0080_CTRL_CMD_GPU_GET_DISPLAY_OWNER:
        {
            LW0080_CTRL_GPU_GET_DISPLAY_OWNER_PARAMS *displayOwnerParams =
                static_cast<LW0080_CTRL_GPU_GET_DISPLAY_OWNER_PARAMS *>(pParams);
            displayOwnerParams->subDeviceInstance = 0;
            break;
        }

        case LW0080_CTRL_CMD_GPU_GET_NUM_SUBDEVICES:
        {
            LW0080_CTRL_GPU_GET_NUM_SUBDEVICES_PARAMS *subdevParams =
                static_cast<LW0080_CTRL_GPU_GET_NUM_SUBDEVICES_PARAMS *>(pParams);
            subdevParams->numSubDevices = 1;
            break;
        }

        case LW0080_CTRL_CMD_GPU_GET_VIRTUALIZATION_MODE:
        {
            LW0080_CTRL_GPU_GET_VIRTUALIZATION_MODE_PARAMS* virtualizationModeParams =
                static_cast<LW0080_CTRL_GPU_GET_VIRTUALIZATION_MODE_PARAMS*>(pParams);
            memset(virtualizationModeParams, 0, sizeof(*virtualizationModeParams));
            virtualizationModeParams->virtualizationMode = LW0080_CTRL_GPU_VIRTUALIZATION_MODE_NONE;
            break;
        }

        case LW0080_CTRL_CMD_GR_GET_CAPS:
        {
            LW0080_CTRL_GR_GET_CAPS_PARAMS *grCapsParams =
                static_cast<LW0080_CTRL_GR_GET_CAPS_PARAMS *>(pParams);
            memset(grCapsParams->capsTbl, 0, grCapsParams->capsTblSize);
            const LwU32 count = GetAmodelChipInfo()->sizeKeyValueGR0080;
            LwU8* pCapsTbl = static_cast<LwU8*>(grCapsParams->capsTbl);

            for (LwU32 i = 0; i < count; i++)
            {
                const __GLLWKey0080* key = &(GetAmodelChipInfo()->keyValueGR0080[i]);
                pCapsTbl[key->idx] = (pCapsTbl[key->idx] & ~(key->mask)) | key->value;
            }

            if (GetAmodelChipInfo()->graphicsCaps & LW_CFG_GRAPHICS_CAPS_QUADRO_GENERIC)
            {
                pCapsTbl[true ? LW0080_CTRL_GR_CAPS_QUADRO_GENERIC] |=
                    (false ? LW0080_CTRL_GR_CAPS_QUADRO_GENERIC);
            }
            break;
        }

        case LW0080_CTRL_CMD_GR_GET_INFO:
        {
            LW0080_CTRL_GR_GET_INFO_PARAMS *getInfoParams =
                static_cast<LW0080_CTRL_GR_GET_INFO_PARAMS *>(pParams);
            LW0080_CTRL_GR_INFO *grInfo = static_cast<LW0080_CTRL_GR_INFO *>(
                getInfoParams->grInfoList);
            for (LwU32 i = 0; i < getInfoParams->grInfoListSize; i++)
            {
                switch (grInfo[i].index)
                {
                    case LW0080_CTRL_GR_INFO_INDEX_MAXCLIPS:
                        grInfo[i].data =
                            (GetAmodelChipInfo()->graphicsCaps &
                                LW_CFG_GRAPHICS_CAPS_MAXCLIPS_MASK) >>
                            LW_CFG_GRAPHICS_CAPS_MAXCLIPS_SHIFT;
                        break;
                    case LW0080_CTRL_GR_INFO_INDEX_MIN_ATTRS_BUG_261894:
                        grInfo[i].data = 0;
                        break;
                    default:
                        return LW_ERR_NOT_SUPPORTED;
                }
            }
            break;
        }

        case LW0080_CTRL_CMD_HOST_GET_CAPS:
        {
            LW0080_CTRL_HOST_GET_CAPS_PARAMS *hostCapsParams =
                static_cast<LW0080_CTRL_HOST_GET_CAPS_PARAMS *>(pParams);
            memset(hostCapsParams->capsTbl, 0, hostCapsParams->capsTblSize);
            break;
        }

        case LW2080_CTRL_CMD_BUS_GET_INFO_V2:
        {
            LW2080_CTRL_BUS_GET_INFO_V2_PARAMS *getInfoParams =
                static_cast<LW2080_CTRL_BUS_GET_INFO_V2_PARAMS *>(pParams);
            LW2080_CTRL_BUS_INFO *busInfo = getInfoParams->busInfoList;

            if (getInfoParams->busInfoListSize > NUMELEMS(getInfoParams->busInfoList))
            {
                return LW_ERR_ILWALID_ARGUMENT;
            }

            for (LwU32 i = 0; i < getInfoParams->busInfoListSize; i++)
            {
                switch (busInfo[i].index)
                {
                    case LW2080_CTRL_BUS_INFO_INDEX_GPU_GART_FLAGS:
                        busInfo[i].data =
                            DRF_DEF(2080_CTRL_BUS_INFO, _GPU_GART_FLAGS, _UNIFIED, _TRUE) |
                            DRF_DEF(2080_CTRL_BUS_INFO, _GPU_GART_FLAGS, _REQFLUSH, _FALSE);
                        break;
                    case LW2080_CTRL_BUS_INFO_INDEX_COHERENT_DMA_FLAGS:
                        busInfo[i].data =
                            DRF_DEF(2080_CTRL_BUS_INFO, _COHERENT_DMA_FLAGS, _CTXDMA, _TRUE) |
                            DRF_DEF(2080_CTRL_BUS_INFO, _COHERENT_DMA_FLAGS, _GPUGART, _TRUE);
                        break;
                    case LW2080_CTRL_BUS_INFO_INDEX_NONCOHERENT_DMA_FLAGS:
                        busInfo[i].data =
                            DRF_DEF(2080_CTRL_BUS_INFO, _NONCOHERENT_DMA_FLAGS, _CTXDMA, _FALSE) |
                            DRF_DEF(2080_CTRL_BUS_INFO, _NONCOHERENT_DMA_FLAGS, _GPUGART, _FALSE) |
                            DRF_DEF(2080_CTRL_BUS_INFO, _NONCOHERENT_DMA_FLAGS, _COH_MODE, _FALSE);
                        break;
                    case LW2080_CTRL_BUS_INFO_INDEX_GPU_GART_SIZE:
                        busInfo[i].data = static_cast<LwU32>(
                            GetAmodelChipInfo()->sysmemBytes >> 20); // want megabytes
                        break;
                    case LW2080_CTRL_BUS_INFO_INDEX_GPU_GART_SIZE_HI:
                        busInfo[i].data = static_cast<LwU32>(
                            (GetAmodelChipInfo()->sysmemBytes >> 20) >> 32); // want megabytes
                        break;
                    case LW2080_CTRL_BUS_INFO_INDEX_TYPE:
                        busInfo[i].data = GetAmodelChipInfo()->busType;
                        break;
                    case LW2080_CTRL_BUS_INFO_INDEX_CAPS:
                    default:
                        busInfo[i].data = 0;
                        break;
                }
            }
            break;
        }

        case LW2080_CTRL_CMD_BUS_GET_INFO:
        {
            LW2080_CTRL_BUS_GET_INFO_PARAMS *getInfoParams =
                static_cast<LW2080_CTRL_BUS_GET_INFO_PARAMS *>(pParams);
            LW2080_CTRL_BUS_INFO *busInfo =
                static_cast<LW2080_CTRL_BUS_INFO *>(getInfoParams->busInfoList);

            LW2080_CTRL_BUS_GET_INFO_V2_PARAMS getInfoParamsV2 = { };
            LW2080_CTRL_BUS_INFO *busInfoV2 = getInfoParamsV2.busInfoList;

            if (getInfoParams->busInfoListSize > NUMELEMS(getInfoParamsV2.busInfoList))
            {
                return LW_ERR_ILWALID_ARGUMENT;
            }

            getInfoParamsV2.busInfoListSize = getInfoParams->busInfoListSize;
            for (LwU32 i = 0; i < getInfoParams->busInfoListSize; i++)
            {
                busInfoV2[i] = busInfo[i];
            }

            LwU32 statusV2 = LwRmControl(hClient, hObject,
                                         LW2080_CTRL_CMD_BUS_GET_INFO_V2,
                                         &getInfoParamsV2, sizeof(getInfoParamsV2));
            if (statusV2 != LW_OK)
            {
                return statusV2;
            }

            for (LwU32 i = 0; i < getInfoParams->busInfoListSize; i++)
            {
                busInfo[i] = busInfoV2[i];
            }
            break;
        }

        case LW2080_CTRL_CMD_BUS_GET_PCI_BAR_INFO:
        {
            LW2080_CTRL_BUS_GET_PCI_BAR_INFO_PARAMS *pciBarInfoParams =
                static_cast<LW2080_CTRL_BUS_GET_PCI_BAR_INFO_PARAMS *>(pParams);
            memset(pciBarInfoParams, 0, sizeof(*pciBarInfoParams));
            break;
        }

        case LW2080_CTRL_CMD_BUS_GET_PCI_INFO:
        {
            LW2080_CTRL_BUS_GET_PCI_INFO_PARAMS *pciInfoParams =
                static_cast<LW2080_CTRL_BUS_GET_PCI_INFO_PARAMS *>(pParams);
            memset(pciInfoParams, 0, sizeof(*pciInfoParams));
            break;
        }

        case LW2080_CTRL_CMD_CE_GET_CAPS:
        {
            LW2080_CTRL_CE_GET_CAPS_PARAMS *ceCapsParams =
                static_cast<LW2080_CTRL_CE_GET_CAPS_PARAMS *>(pParams);
            memset(ceCapsParams->capsTbl, 0, ceCapsParams->capsTblSize);
            LwU8* pCapsTbl = static_cast<LwU8*>(ceCapsParams->capsTbl);

            pCapsTbl[true ? LW2080_CTRL_CE_CAPS_CE_GRCE] |=
                (false ? LW2080_CTRL_CE_CAPS_CE_GRCE);
            pCapsTbl[true ? LW2080_CTRL_CE_CAPS_CE_SUPPORTS_NONPIPELINED_BL] |=
                (false ? LW2080_CTRL_CE_CAPS_CE_SUPPORTS_NONPIPELINED_BL);
            pCapsTbl[true ? LW2080_CTRL_CE_CAPS_CE_SUPPORTS_PIPELINED_BL] |=
                (false ? LW2080_CTRL_CE_CAPS_CE_SUPPORTS_PIPELINED_BL);

            break;
        }

        case LW2080_CTRL_CMD_CE_GET_CAPS_V2:
        {
            LW2080_CTRL_CE_GET_CAPS_V2_PARAMS *ceCapsParams =
                static_cast<LW2080_CTRL_CE_GET_CAPS_V2_PARAMS *>(pParams);
            memset(ceCapsParams->capsTbl, 0, sizeof ceCapsParams->capsTbl);

            ceCapsParams->capsTbl[true ? LW2080_CTRL_CE_CAPS_CE_SUPPORTS_NONPIPELINED_BL] |=
                (false ? LW2080_CTRL_CE_CAPS_CE_SUPPORTS_NONPIPELINED_BL);
            ceCapsParams->capsTbl[true ? LW2080_CTRL_CE_CAPS_CE_SUPPORTS_PIPELINED_BL] |=
                (false ? LW2080_CTRL_CE_CAPS_CE_SUPPORTS_PIPELINED_BL);

            if (ceCapsParams->ceEngineType == LW2080_ENGINE_TYPE_COPY0)
            {
                ceCapsParams->capsTbl[true ? LW2080_CTRL_CE_CAPS_CE_GRCE] |=
                    (false ? LW2080_CTRL_CE_CAPS_CE_GRCE);
            }

            break;
        }

        case LW2080_CTRL_CMD_CE_GET_CE_PCE_MASK:
        {
            LW2080_CTRL_CE_GET_CE_PCE_MASK_PARAMS *ceMaskParams =
                static_cast<LW2080_CTRL_CE_GET_CE_PCE_MASK_PARAMS *>(pParams);
            ceMaskParams->pceMask = 0x1;
            break;
        }

        case LW2080_CTRL_CMD_FAN_COOLER_GET_INFO:
        {
            LW2080_CTRL_FAN_COOLER_INFO_PARAMS *coolerPolicyParams =
                static_cast<LW2080_CTRL_FAN_COOLER_INFO_PARAMS *>(pParams);
            coolerPolicyParams->coolerMask = 0;
            break;
        }

        case LW2080_CTRL_CMD_FAN_POLICY_GET_INFO:
        {
            LW2080_CTRL_FAN_POLICY_INFO_PARAMS *fanPolicyParams =
                static_cast<LW2080_CTRL_FAN_POLICY_INFO_PARAMS *>(pParams);
            fanPolicyParams->policyMask = 0;
            break;
        }

        case LW2080_CTRL_CMD_FAN_ARBITER_GET_INFO:
        {
            LW2080_CTRL_FAN_ARBITER_INFO_PARAMS *fanArbiterParams =
                static_cast<LW2080_CTRL_FAN_ARBITER_INFO_PARAMS *>(pParams);
            fanArbiterParams->super.objMask.super.pData[0] = 0;
            break;
        }

        case LW2080_CTRL_CMD_FB_GET_GPU_CACHE_INFO:
        {
            LW2080_CTRL_FB_GET_GPU_CACHE_INFO_PARAMS *getInfoParams =
                static_cast<LW2080_CTRL_FB_GET_GPU_CACHE_INFO_PARAMS *>(pParams);
            getInfoParams->powerState = LW2080_CTRL_FB_GET_GPU_CACHE_INFO_POWER_STATE_ENABLED;
            getInfoParams->writeMode = LW2080_CTRL_FB_GET_GPU_CACHE_INFO_WRITE_MODE_WRITEBACK;
            getInfoParams->rcmState = LW2080_CTRL_FB_GET_GPU_CACHE_INFO_RCM_STATE_FULL;
            break;
        }

        case LW2080_CTRL_CMD_FB_GET_INFO:
        {
            LW2080_CTRL_FB_GET_INFO_PARAMS *getInfoParams =
                static_cast<LW2080_CTRL_FB_GET_INFO_PARAMS *>(pParams);
            LW2080_CTRL_FB_INFO *fbInfo = static_cast<LW2080_CTRL_FB_INFO*>(
                getInfoParams->fbInfoList);
            for (LwU32 i = 0; i < getInfoParams->fbInfoListSize; i++)
            {
                const LwU32 *info = GetAmodelChipInfo()->keyValueFB2080;
                const LwU32 size = GetAmodelChipInfo()->sizeKeyValueFB2080;

                fbInfo[i].data = 0;
                for (LwU32 j = 0; j < size; j += 2)
                {
                    if (info[j] == fbInfo[i].index)
                    {
                        fbInfo[i].data = info[j + 1];
                        break;
                    }
                }
            }
            break;
        }

        case LW2080_CTRL_CMD_FB_GET_LTC_INFO_FOR_FBP:
        {
            LW2080_CTRL_FB_GET_LTC_INFO_FOR_FBP_PARAMS *ltcPerFbpParams =
                static_cast<LW2080_CTRL_FB_GET_LTC_INFO_FOR_FBP_PARAMS *>(pParams);

            ltcPerFbpParams->ltcCount = 2;
            ltcPerFbpParams->ltcMask = 3;
            break;
        }

        case LW2080_CTRL_CMD_GPU_GET_ENGINE_PARTNERLIST:
        {
            LW2080_CTRL_GPU_GET_ENGINE_PARTNERLIST_PARAMS *partnerListParams =
                static_cast<LW2080_CTRL_GPU_GET_ENGINE_PARTNERLIST_PARAMS *>(pParams);
            partnerListParams->numPartners = 1;
            partnerListParams->partnerList[0] = LW2080_ENGINE_TYPE_COPY0;
            break;
        }

        case LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST:
        {
            LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS *engineClasslistParams =
                static_cast<LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS *>(pParams);

            if (engineClasslistParams->classList == nullptr)
            {
                engineClasslistParams->numClasses = 1;
            }
            else
            {
                switch (engineClasslistParams->engineType)
                {
                    case LW2080_ENGINE_TYPE_COPY1:
                    {
                        LwU32 *classList = static_cast<LwU32 *>(engineClasslistParams->classList);
                        classList[0] = AMPERE_DMA_COPY_A;
                        break;
                    }
                    default:
                        return LW_ERR_ILWALID_ARGUMENT;
                }
            }
            break;
        }

        case LW2080_CTRL_CMD_GPU_GET_ENGINES:
        {
            LW2080_CTRL_GPU_GET_ENGINES_PARAMS *engineParams =
                static_cast<LW2080_CTRL_GPU_GET_ENGINES_PARAMS *>(pParams);
            engineParams->engineCount = 3;
            if (engineParams->engineList != nullptr)
            {
                LwU32 *engines = static_cast<LwU32 *>(engineParams->engineList);
                engines[0] = LW2080_ENGINE_TYPE_GRAPHICS;
                engines[1] = LW2080_ENGINE_TYPE_COPY0;
                engines[2] = LW2080_ENGINE_TYPE_COPY1;
            }
            break;
        }

        case LW2080_CTRL_CMD_GPU_GET_ENGINES_V2:
        {
            LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS *engineParams =
                static_cast<LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS *>(pParams);
            engineParams->engineCount = 5;
            engineParams->engineList[0] = LW2080_ENGINE_TYPE_GRAPHICS;
            engineParams->engineList[1] = LW2080_ENGINE_TYPE_COPY0;
            engineParams->engineList[2] = LW2080_ENGINE_TYPE_COPY1;
            engineParams->engineList[3] = LW2080_ENGINE_TYPE_LWDEC0;
            engineParams->engineList[4] = LW2080_ENGINE_TYPE_LWJPEG0;
            break;
        }

        case LW2080_CTRL_CMD_GPU_GET_FERMI_GPC_INFO:
        {
            LW2080_CTRL_GPU_GET_FERMI_GPC_INFO_PARAMS *gpcParams =
                static_cast<LW2080_CTRL_GPU_GET_FERMI_GPC_INFO_PARAMS *>(pParams);
            gpcParams->gpcMask = (1 << GetNumGpcs()) - 1;
            break;
        }

        case LW2080_CTRL_CMD_GPU_GET_FERMI_TPC_INFO:
        {
            LW2080_CTRL_GPU_GET_FERMI_TPC_INFO_PARAMS *tpcParams =
                static_cast<LW2080_CTRL_GPU_GET_FERMI_TPC_INFO_PARAMS *>(pParams);
            tpcParams->tpcMask = (1 << GetNumTpcs()/ GetNumGpcs()) - 1;
            break;
        }

        case LW2080_CTRL_CMD_GPU_GET_ID:
        {
            LW2080_CTRL_GPU_GET_ID_PARAMS *gpuIdParams =
                static_cast<LW2080_CTRL_GPU_GET_ID_PARAMS *>(pParams);
            gpuIdParams->gpuId = FAKE_GPUID;
            break;
        }

        case LW2080_CTRL_CMD_GPU_GET_INFO:
        {
            LW2080_CTRL_GPU_GET_INFO_PARAMS *getInfoParams =
                static_cast<LW2080_CTRL_GPU_GET_INFO_PARAMS *>(pParams);
            LW2080_CTRL_GPU_INFO *gpuInfo =
                static_cast<LW2080_CTRL_GPU_INFO *>(getInfoParams->gpuInfoList);
            for (LwU32 i = 0; i < getInfoParams->gpuInfoListSize; i++)
            {
                gpuInfo[i].data = 0;
            }
            break;
        }

        case LW2080_CTRL_CMD_GPU_GET_INFO_V2:
        {
            LW2080_CTRL_GPU_GET_INFO_V2_PARAMS *getInfoParams =
                static_cast<LW2080_CTRL_GPU_GET_INFO_V2_PARAMS *>(pParams);

            for (LwU32 i = 0; i < getInfoParams->gpuInfoListSize; i++)
            {
                getInfoParams->gpuInfoList[i].data = 0;
            }
            break;
        }

        case LW2080_CTRL_CMD_GPU_GET_NAME_STRING:
        {
            LW2080_CTRL_GPU_GET_NAME_STRING_PARAMS *getNameParams =
                static_cast<LW2080_CTRL_GPU_GET_NAME_STRING_PARAMS*>(pParams);
            getNameParams->gpuNameStringFlags = 0;
            sprintf(reinterpret_cast<char*>(&getNameParams->gpuNameString.ascii[0]),
                "%s%s/DirectAmodel", GetAmodelChipInfo()->name,
                (GetAmodelChipInfo()->graphicsCaps &
                    LW_CFG_GRAPHICS_CAPS_QUADRO_GENERIC) ? "GL" : "");
            break;
        }

        case LW2080_CTRL_CMD_GPU_GET_SIMULATION_INFO:
        {
            LW2080_CTRL_GPU_GET_SIMULATION_INFO_PARAMS *simInfoParams =
                static_cast<LW2080_CTRL_GPU_GET_SIMULATION_INFO_PARAMS *>(pParams);
            simInfoParams->type = LW2080_CTRL_GPU_GET_SIMULATION_INFO_TYPE_LIVE_AMODEL;
            break;
        }

        case LW2080_CTRL_CMD_GR_GET_CAPS_V2:
        {
            LW2080_CTRL_GR_GET_CAPS_V2_PARAMS *grCapsV2Params =
                static_cast<LW2080_CTRL_GR_GET_CAPS_V2_PARAMS *>(pParams);
            memset(grCapsV2Params, 0, sizeof(LW2080_CTRL_GR_GET_CAPS_V2_PARAMS));

#define MODS_LW0080_CTRL_GR_SET_CAP(tbl,c)              ((tbl[(1?c)]) |= (0?c))
            MODS_LW0080_CTRL_GR_SET_CAP(grCapsV2Params->capsTbl, LW0080_CTRL_GR_CAPS_AA_LINES);
            MODS_LW0080_CTRL_GR_SET_CAP(grCapsV2Params->capsTbl, LW0080_CTRL_GR_CAPS_AA_POLYS);
            MODS_LW0080_CTRL_GR_SET_CAP(grCapsV2Params->capsTbl, LW0080_CTRL_GR_CAPS_LOGIC_OPS);
            MODS_LW0080_CTRL_GR_SET_CAP(grCapsV2Params->capsTbl, LW0080_CTRL_GR_CAPS_2SIDED_LIGHTING);
            MODS_LW0080_CTRL_GR_SET_CAP(grCapsV2Params->capsTbl, LW0080_CTRL_GR_CAPS_3D_TEXTURES);
            MODS_LW0080_CTRL_GR_SET_CAP(grCapsV2Params->capsTbl, LW0080_CTRL_GR_CAPS_ANISOTROPIC);
            MODS_LW0080_CTRL_GR_SET_CAP(grCapsV2Params->capsTbl, LW0080_CTRL_GR_CAPS_SET_SHADER_SAMPLE_MASK_SUPPORTED);
#undef  MODS_LW0080_CTRL_GR_SET_CAP

            // For reference this is what we get from real RM on TU102 silicon:
            //grCapsV2Params->capsTbl[0x00000000] = 0xb0;
            //grCapsV2Params->capsTbl[0x00000001] = 0x62;
            //grCapsV2Params->capsTbl[0x00000002] = 0x00;
            //grCapsV2Params->capsTbl[0x00000003] = 0x00;
            //grCapsV2Params->capsTbl[0x00000004] = 0x00;
            //grCapsV2Params->capsTbl[0x00000005] = 0x00;
            //grCapsV2Params->capsTbl[0x00000006] = 0x00;
            //grCapsV2Params->capsTbl[0x00000007] = 0x01;
            //grCapsV2Params->capsTbl[0x00000008] = 0x00;
            //grCapsV2Params->capsTbl[0x00000009] = 0x00;
            //grCapsV2Params->capsTbl[0x0000000a] = 0x00;
            //grCapsV2Params->capsTbl[0x0000000b] = 0x10;
            //grCapsV2Params->capsTbl[0x0000000c] = 0x01;
            //grCapsV2Params->capsTbl[0x0000000d] = 0x08;
            //grCapsV2Params->capsTbl[0x0000000e] = 0x00;
            //grCapsV2Params->capsTbl[0x0000000f] = 0x10;
            //grCapsV2Params->capsTbl[0x00000010] = 0x10;
            //grCapsV2Params->capsTbl[0x00000011] = 0x00;
            //grCapsV2Params->capsTbl[0x00000012] = 0x80;
            //grCapsV2Params->capsTbl[0x00000013] = 0x00;
            //grCapsV2Params->capsTbl[0x00000014] = 0x04;
            //grCapsV2Params->capsTbl[0x00000015] = 0x20;

            break;
        }

        case LW2080_CTRL_CMD_GR_GET_INFO:
        {
            LW2080_CTRL_GR_GET_INFO_PARAMS *getInfoParams =
                static_cast<LW2080_CTRL_GR_GET_INFO_PARAMS *>(pParams);
            LW2080_CTRL_GR_INFO *grInfo = static_cast<LW2080_CTRL_GR_INFO *>(
                getInfoParams->grInfoList);
            for (LwU32 i = 0; i < getInfoParams->grInfoListSize; i++)
            {
                const LwU32 *info = GetAmodelChipInfo()->keyValueGR2080;
                const LwU32 size = GetAmodelChipInfo()->sizeKeyValueGR2080;

                grInfo[i].data = 0;
                for (LwU32 j = 0; j < size; j += 2)
                {
                    if (info[j] == grInfo[i].index)
                    {
                        grInfo[i].data = info[j + 1];
                        break;
                    }
                }
            }
            break;
        }

        case LW2080_CTRL_CMD_MC_GET_ARCH_INFO:
        {
            LW2080_CTRL_MC_GET_ARCH_INFO_PARAMS *archInfoParams =
                static_cast<LW2080_CTRL_MC_GET_ARCH_INFO_PARAMS *>(pParams);
            memset(archInfoParams, 0, sizeof(*archInfoParams));
            archInfoParams->implementation = GetAmodelChipInfo()->implementation;
            archInfoParams->architecture = GetAmodelChipInfo()->architecture;
            break;
        }

        case LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS:
        {
            LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS_PARAMS *lwlinkCapsParams =
                static_cast<LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS_PARAMS *>(pParams);
            memset(lwlinkCapsParams, 0, sizeof(*lwlinkCapsParams));
            break;
        }

        case LW2080_CTRL_CMD_PERF_GET_SAMPLED_VOLTAGE_INFO:
        {
            LW2080_CTRL_PERF_GET_SAMPLED_VOLTAGE_INFO_PARAMS *sampledVoltageParams =
                static_cast<LW2080_CTRL_PERF_GET_SAMPLED_VOLTAGE_INFO_PARAMS *>(pParams);
            sampledVoltageParams->voltage = 1;
            break;
        }

        case LW2080_CTRL_CMD_PERF_PSTATES_GET_INFO:
        {
            LW2080_CTRL_PERF_PSTATES_INFO *perfPstatesInfo =
                static_cast<LW2080_CTRL_PERF_PSTATES_INFO *>(pParams);
            perfPstatesInfo->version = LW2080_CTRL_PERF_PSTATE_VERSION_ILWALID;
            break;
        }

        case LW2080_CTRL_CMD_TIMER_GET_TIME:
        {
            LW2080_CTRL_TIMER_GET_TIME_PARAMS *timerGetTimeParams =
                static_cast<LW2080_CTRL_TIMER_GET_TIME_PARAMS *>(pParams);
            timerGetTimeParams->time_nsec = Xp::GetWallTimeNS();
            break;
        }

        case LW5070_CTRL_CMD_SYSTEM_GET_CAPS_V2:
        {
            LW5070_CTRL_SYSTEM_GET_CAPS_V2_PARAMS *evoSysCapsParams =
                static_cast<LW5070_CTRL_SYSTEM_GET_CAPS_V2_PARAMS *>(pParams);
            memset(evoSysCapsParams->capsTbl, 0, sizeof(evoSysCapsParams->capsTbl));
            break;
        }

        case LW906F_CTRL_GET_CLASS_ENGINEID:
        {
            LW906F_CTRL_GET_CLASS_ENGINEID_PARAMS *getClassEngineParams =
                static_cast<LW906F_CTRL_GET_CLASS_ENGINEID_PARAMS *>(pParams);
            getClassEngineParams->classEngineID = getClassEngineParams->hObject;
            getClassEngineParams->classID = 0;
            getClassEngineParams->engineID = 0;
            break;
        }

        case LW2080_CTRL_CMD_GPU_GET_NUM_MMUS_PER_GPC:
        {
            LW2080_CTRL_GPU_GET_NUM_MMUS_PER_GPC_PARAMS *pGetNumMMUsPerGpcParams =
                static_cast<LW2080_CTRL_GPU_GET_NUM_MMUS_PER_GPC_PARAMS *>(pParams);
            const LwU32 architecture = GetAmodelChipInfo()->architecture;
            const LwU32 implementation = GetAmodelChipInfo()->implementation;

            if ((architecture == LW2080_CTRL_MC_ARCH_INFO_ARCHITECTURE_GA100) &&
                (implementation == LW2080_CTRL_MC_ARCH_INFO_IMPLEMENTATION_GA100))
            {
                pGetNumMMUsPerGpcParams->count = 2;
            }
            else
            {
                pGetNumMMUsPerGpcParams->count = 1;
            }
            break;
        }
        default:
            Printf(Tee::PriWarn, "Direct Amodel LwRmCtrl cmd:0x%x is not supported!\n", cmd);
            MASSERT(!"Unknown direct amodel RM control");
            rmStatus = LW_ERR_OPERATING_SYSTEM;
            break;
    }

    return rmStatus;
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmDupObject
(
    LwHandle    hClient,
    LwHandle    hParent,
    LwHandle    hObject,
    LwHandle    hClientSrc,
    LwHandle    hObjectSrc,
    LwU32       flags
)
{
    Tasker::MutexHolder resourceMutex(s_ResourceMutex);

    auto &duplicateObjects = s_RMClients[hClient].duplicateObjects;
    if (duplicateObjects.count(hObject) > 0)
    {
        MASSERT(!"Duplicate object already exists");
        return LW_ERR_ILWALID_OBJECT;
    }

    duplicateObjects[hObject] = make_pair(hClientSrc, hObjectSrc);

    return LW_OK;
}

FAKERMEXPORT LwU32 LW_APIENTRY LwRmDupObject2
(
    LwHandle    hClient,
    LwHandle    hParent,
    LwHandle    *hObject,
    LwHandle    hClientSrc,
    LwHandle    hObjectSrc,
    LwU32       flags
)
{
    Tasker::MutexHolder resourceMutex(s_ResourceMutex);

    const LwU32 hMemory = 0xcafe0000 ^ s_MemoryHandleID;
    *hObject = hMemory;

    s_MemoryHandleID++;

    auto &duplicateObjects = s_RMClients[hClient].duplicateObjects;
    if (duplicateObjects.count(*hObject) > 0)
    {
        MASSERT(!"Duplicate object already exists");
        return LW_ERR_ILWALID_OBJECT;
    }

    duplicateObjects[*hObject] = make_pair(hClientSrc, hObjectSrc);

    return LW_OK;
}
