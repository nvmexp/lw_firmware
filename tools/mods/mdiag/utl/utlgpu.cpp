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
#include "core/include/mgrmgr.h"
#include "core/include/rc.h"
#include "core/include/registry.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/protmanager.h"
#include "lwRmReg.h"
#include "ctrl/ctrl2080/ctrl2080lwlink.h"
#include "turing/tu102/dev_fb.h"

#include "mdiag/smc/smcresourcecontroller.h"
#include "mdiag/smc/smcparser.h"
#include "mdiag/lwgpu.h"
#include "mdiag/sysspec.h"
#include "mdiag/tests/test.h"
#include "mdiag/utils/vaspaceutils.h"

#include "utl.h"
#include "utlgpupartition.h"
#include "utlgpuregctrl.h"
#include "utlsmcengine.h"
#include "utlutil.h"

#include "utlgpu.h"
#include "utlengine.h"
#include "utlchannel.h"
#include "utlsurface.h"
#include "utlvaspace.h"
#include "utlmemctrl.h"
#include "utlfbinfo.h"

map<GpuSubdevice*, unique_ptr<UtlGpu>> UtlGpu::s_Gpus;
vector<UtlGpu*> UtlGpu::s_PythonGpus;

void UtlGpu::Register(py::module module)
{
    UtlKwargsMgr* kwargs = UtlKwargsMgr::Instance();
    kwargs->RegisterKwarg("Gpu.CreateGpuPartition", "gpcCount", false, "Can be a single integer (creates and returns one partition with the specified gpcCount) or a list (creates and returns a list of partitons with the specified gpcCount[s])");
    kwargs->RegisterKwarg("Gpu.CreateGpuPartition", "partitionFlag", false, "List of Partition flags (FULL, HALF, QUARTER, EIGHTH) of the GpuPartition to be created");
    kwargs->RegisterKwarg("Gpu.CreateGpuPartition", "partitionName", false, "Name of the gpu partition to be created");
    kwargs->RegisterKwarg("Gpu.GetGpuPartition", "sysPipe", false, "SysPipe owned by the gpu partition that is to be retrieved; must be in range 0..7");
    kwargs->RegisterKwarg("Gpu.GetGpuPartition", "partitionName", false, "Name of the gpu partition to be retrieved");
    kwargs->RegisterKwarg("Gpu.GetEngines", "engineName", true, "enum indicating engine type");
    kwargs->RegisterKwarg("Gpu.GetEngines", "gpuPartition", false, "partition to query when SMC is enabled");
    kwargs->RegisterKwarg("Gpu.GetMemCtrl", "memCtrlType", true, "MemCtrlType enum (FBMEM, MMIO_PRIV or MMIO_USER) indicating memory type");
    kwargs->RegisterKwarg("Gpu.GetMemCtrl", "address", false, "start address of the memory region. For FBMEM it is the GPU PA.");
    kwargs->RegisterKwarg("Gpu.GetMemCtrl", "memSize", false, "size of the memory region");
    kwargs->RegisterKwarg("Gpu.PreemptRunlist", "engine", false, "engine to preempt");
    kwargs->RegisterKwarg("Gpu.PreemptRunlist", "smcEngine", false, "SMC engine to preempt");
    kwargs->RegisterKwarg("Gpu.RemoveGpuPartition", "sysPipe", false, "sysPipe resource owned by the GpuPartition to be removed");
    kwargs->RegisterKwarg("Gpu.RemoveGpuPartition", "gpuPartition", false, "GpuPartition to be removed");
    kwargs->RegisterKwarg("Gpu.RemoveGpuPartition", "partitionName", false, "name of gpu partition to be removed");
    kwargs->RegisterKwarg("Gpu.Reset", "type", true, "reset type enum");
    kwargs->RegisterKwarg("Gpu.ResubmitRunlist", "engine", false, "target engine");
    kwargs->RegisterKwarg("Gpu.ResubmitRunlist", "smcEngine", false, "target SMC engine");
    kwargs->RegisterKwarg("Gpu.SetGpuPartitionMode", "partitionMode", true, "mode enum");
    kwargs->RegisterKwarg("Gpu.GetAvailablePartitionTypeCount", "partitionFlag", false, "List of Partition flags to be queried");
    kwargs->RegisterKwarg("Gpu.HasFeature", "feature", true, "A specific gpu feature to be queried");
    kwargs->RegisterKwarg("Gpu.TlbIlwalidate", "vaSpace", true, "Ilwalidate this vaspace related tlb");
    kwargs->RegisterKwarg("Gpu.GetPdbConfigure", "surface", false, "Surface");
    kwargs->RegisterKwarg("Gpu.GetPdbConfigure", "targetChannel", false, "PDB target channel");
    kwargs->RegisterKwarg("Gpu.GetPdbConfigure", "shouldIlwalidateReplay", false, "True if ilwalidateReplay is CANCEL_VA_GLOBAL for TLB ilwalidate action. Default value is False.");
    kwargs->RegisterKwarg("Gpu.GetPdbConfigure", "ilwalidateBar1", false, "True if BAR1 ilwalidate is required. Default value is False.");
    kwargs->RegisterKwarg("Gpu.GetPdbConfigure", "ilwalidateBar2", false, "True if BAR2 ilwalidate is required. Default value is False.");
    kwargs->RegisterKwarg("Gpu.CheckConfigureLevel", "targetChannel", false, "PDB target channel");
    kwargs->RegisterKwarg("Gpu.CheckConfigureLevel", "ilwalidateLevelNum", true, "Ilwalidate level num for TLB ilwalidate action");
    kwargs->RegisterKwarg("Gpu.GetTpcCount", "virtualGpcNum", false, "virtual GPC number whose TPC count is to be retrieved");
    kwargs->RegisterKwarg("Gpu.GetPesCount", "virtualGpcNum", false, "virtual GPC number whose PES count is to be retrieved");
    kwargs->RegisterKwarg("Gpu.L2Operation", "targetMem", true, "target memory enum for L2 operation");
    kwargs->RegisterKwarg("Gpu.L2Operation", "operationType", true, "L2 operation type enum");
    kwargs->RegisterKwarg("Gpu.SetRcRecovery", "enable", true, "boolean to enable/disable RC recovery");

    py::class_<UtlGpu> gpu(module, "Gpu",
        "The Gpu class is used for objects that represent GPU devices.  All Gpu objects are created by MODS during initialization, which happens before any UTL scripts are run.");
    gpu.def_static("GetGpus", &UtlGpu::GetGpusPy,
        "This function returns a list of all Gpu objects.");
    gpu.def("GetUprRegions", &UtlGpu::GetUprRegions,
        "This function returns a list of unprotected regions. Each item in the list is a tuple of length 2 with base physical address and size as its members.");
    gpu.def("CreateGpuPartition", &UtlGpu::CreateGpuPartitionPy,
        UTL_DOCSTRING("Gpu.CreateGpuPartition", "This function creates a single or a list of Gpu Partition."),
        py::return_value_policy::reference);
    gpu.def("RemoveGpuPartition", &UtlGpu::RemoveGpuPartition,
        UTL_DOCSTRING("Gpu.RemoveGpuPartition", "This function removes the specified GpuPartition. Requires exactly one of the following arguments: sysPipe or gpuPartition."));
    gpu.def("GetGpuPartition", &UtlGpu::GetGpuPartitionPy,
        UTL_DOCSTRING("Gpu.GetGpuPartition", "This function returns the specified Gpu Partition."),
        py::return_value_policy::reference);
    gpu.def("GetGpuPartitionList", &UtlGpu::GetGpuPartitionListPy,
        "This function returns a list of all Gpu Partitions.\n");
    gpu.def("GetSmCount", &UtlGpu::GetSmCount,
        "This function returns the total number of Shader Modules (SMs) in the Gpu.");
    gpu.def("GetTpcCount", &UtlGpu::GetTpcCountPy,
        "This function returns the total number of Texture Processing Clusters (TPCs) in the GPU. If the optional argument virtualGpcNum is provided then it will return the TPC count for that virtual GPC only.");
    gpu.def("GetPesCount", &UtlGpu::GetPesCountPy,
        "This function returns the total number of Primitive Engine Shared (PESs) in the GPU. If the optional argument virtualGpcNum is provided then it will return the PES count for that virtual GPC only.");
    gpu.def("GetGpcCount", &UtlGpu::GetGpcCountPy,
        "This function returns the total number of Graphics Processing Clusters (GPCs) in the GPU.");
    gpu.def("GetEngines", &UtlGpu::GetEngines,
        UTL_DOCSTRING("Gpu.GetEngines", "This function returns the engines available in the GPU."));
    gpu.def("GetRegCtrl", &UtlGpu::GetRegCtrl,
        "This function returns the Register controller associated with the Gpu.",
        py::return_value_policy::take_ownership);
    gpu.def("GetMemCtrl", &UtlGpu::GetMemCtrl,
        UTL_DOCSTRING("Gpu.GetMemCtrl", "This function returns the utl.MemCtrl object for the memory type specified."),
        py::return_value_policy::reference);
    gpu.def("GetSurfaces", &UtlGpu::GetSurfaces,
        "This function returns surfaces associated with the Gpu.  This can potentially include any common surface that is not tied to any particular test.  For now, this returns just FLA export surfaces.");
    gpu.def("Reset", &UtlGpu::Reset,
        UTL_DOCSTRING("Gpu.Reset", "This function resets the GPU. Please specify '-pcie_config_save 1' when doing a SECONDARY_BUS or FUNDAMENTAL reset."));
    gpu.def("RecoverFromReset", &UtlGpu::RecoverFromReset,
        "This functions tells the GPU to recover from a previous Reset.");
    gpu.def("SetGpuPartitionMode", &UtlGpu::SetGpuPartitionMode,
        UTL_DOCSTRING("Gpu.SetGpuPartitionMode", "This function can swich the GpuPartition between MAX_PERF and REDUCED_PERF."));
    gpu.def("GetAvailablePartitionTypeCount", &UtlGpu::GetAvailablePartitionTypeCount,
        UTL_DOCSTRING("Gpu.GetAvailablePartitionTypeCount", "This function will return a map of PartitionFlag : #Available Partitions. If partitionFlag is not specified it will return the map with entries for all the types of partition"));
    gpu.def("PreemptRunlist", &UtlGpu::PreemptRunlist,
        UTL_DOCSTRING("Gpu.PreemptRunlist", "This function preempts the specified engine's runlist. Requires exactly one of the following arguments: engine or smcEngine"));
    gpu.def("ResubmitRunlist", &UtlGpu::ResubmitRunlist,
        UTL_DOCSTRING("Gpu.ResubmitRunlist", "This function restarts the specified engine's runlist. Requires exactly one of the following arguments: engine or smcEngine."));
    gpu.def("GetSupportedClasses", &UtlGpu::GetSupportedClasses,
        UTL_DOCSTRING("Gpu.GetSupportedClasses", "This function returns a list of classes that the GPU supports.  Each entry is a HW class number, which can be used for functions like Channel.CreateSubchannel."));
    gpu.def_property_readonly("chipName", &UtlGpu::GetChipName,
        "A string representing the chip name of this GPU.");
    gpu.def("GetIrq", &UtlGpu::GetIrq,
        UTL_DOCSTRING("Gpu.GetIrq", "This function returns the first irq."));
    gpu.def("HasFeature", &UtlGpu::HasFeature,
        UTL_DOCSTRING("Gpu.HasFeature", "This function return status whether the feature is supported in this GPU."));
    gpu.def("TlbIlwalidate", &UtlGpu::TlbIlwalidatePy,
        UTL_DOCSTRING("Gpu.TlbIlwalidate", "Ilwalidate this tlb cache from given surface"));
    gpu.def("GetPdbConfigure", &UtlGpu::GetPdbConfigurePy,
        UTL_DOCSTRING("Gpu.GetPdbConfigure", "Returns a list of 2 items. First item is physical address and the second is aperture"));
    gpu.def("CheckConfigureLevel", &UtlGpu::CheckConfigureLevelPy,
        UTL_DOCSTRING("Gpu.CheckConfigureLevel", "Returns true if the passed level in the page diretory hierarchy for TLB Ilwalidate is valid"));
    gpu.def("L2Operation", &UtlGpu::L2Operation,
        UTL_DOCSTRING("Gpu.L2Operation", "This function will perform the specified L2 operation on the specified target memory. Lwrrently, Writeback (Flush cache but do not ilwalidate) operation is supported for All memory target only. Also, Evict (Flush cache and also ilwalidate) operation is only supported for Vidmem memory target."));
    gpu.def("GetFbInfo", &UtlGpu::GetFbInfo,
        UTL_DOCSTRING("Gpu.GetFbInfo", "This function returns the FB Info object for the Gpu. The returned object can then be used to retrieve various FB related info (see utl.FbInfo class)."),
        py::return_value_policy::reference);
    gpu.def("GetLwlinkEnabledMask", &UtlGpu::GetLwlinkEnabledMask,
        UTL_DOCSTRING("Gpu.GetLwlinkEnabledMask", "This function returns the value of LWLink mask."));
    gpu.def("FbFlush", &UtlGpu::FbFlush,
        UTL_DOCSTRING("Gpu.FbFlush", "This function flushes the FB to a point of coherence by forcing all writes to complete."));
    gpu.def("SetRcRecovery", &UtlGpu::SetRcRecovery,
        UTL_DOCSTRING("Gpu.SetRcRecovery", "This function enables/disables RC recovery."));
    gpu.def("GetRunlistPool", &UtlGpu::GetRunlistPool,
        UTL_DOCSTRING("Gpu.GetRunlistPool",
            "This function returns the location and size of the memory pool allocated for runlists."
            "  Returns a tuple with starting physical address and size as members."
            "  Intended to be used with the -alloc_contiguous_runlists command-line argument."));
    gpu.def("IsSmcEnabled", &UtlGpu::IsSmcEnabled,
        UTL_DOCSTRING("Gpu.IsSmcEnabled", "Returns true if SMC is enabled for this gpu."));
    gpu.def_property_readonly("mmuEngineIdStart", &UtlGpu::GetMmuEngineIdStart,
        UTL_DOCSTRING("Gpu.mmuEngineIdStart", "This property stores the GR0's MMU Engine Id before SMC partitioning."));

    py::enum_<UtlGpu::ResetType>(gpu, "ResetType")
        .value("SOFTWARE", UtlGpu::ResetType::Software)
        .value("SECONDARY_BUS", UtlGpu::ResetType::SecondaryBus)
        .value("FUNDAMENTAL", UtlGpu::ResetType::Fundamental);
    py::enum_<SmcResourceController::PartitionMode>(gpu, "PartitionMode")
        .value("MAX_PERF", SmcResourceController::PartitionMode::MAX_PERF)
        .value("REDUCED_PERF", SmcResourceController::PartitionMode::REDUCED_PERF);
    py::enum_<SmcResourceController::PartitionFlag>(gpu, "PartitionFlag")
        .value("FULL", SmcResourceController::PartitionFlag::FULL)
        .value("HALF", SmcResourceController::PartitionFlag::HALF)
        .value("QUARTER", SmcResourceController::PartitionFlag::QUARTER)
        .value("EIGHTH", SmcResourceController::PartitionFlag::EIGHTH);
    py::enum_<UtlGpu::L2OperationTargetMemory>(gpu, "L2OperationTargetMemory")
        .value("ALL", UtlGpu::L2OperationTargetMemory::All)
        .value("VIDMEM", UtlGpu::L2OperationTargetMemory::Vidmem);
    py::enum_<UtlGpu::L2OperationType>(gpu, "L2OperationType")
        .value("WRITEBACK", UtlGpu::L2OperationType::Writeback)
        .value("EVICT", UtlGpu::L2OperationType::Evict);

#define DEFINE_GPUSUB_FEATURE(feat, featid, desc) UTL_BIND_CONSTANT(gpu, Device::feat, #feat)
#define DEFINE_GPUDEV_FEATURE(feat, featid, desc) UTL_BIND_CONSTANT(gpu, Device::feat, #feat)
#define DEFINE_MCP_FEATURE(feat, featid, desc) UTL_BIND_CONSTANT(gpu, Device::feat, #feat)
#define DEFINE_SOC_FEATURE(feat, featid, desc) UTL_BIND_CONSTANT(gpu, Device::feat, #feat)

#include "core/include/featlist.h"

#undef DEFINE_GPUSUB_FEATURE
#undef DEFINE_GPUDEV_FEATURE
#undef DEFINE_MCP_FEATURE
#undef DEFINE_SOC_FEATURE
}

// This function is called by the Utl object to create all Gpu objects
// before any scripts are run.
//
void UtlGpu::CreateGpus()
{
    GpuDevMgr* gpuDevMgr = (GpuDevMgr*)(DevMgrMgr::d_GraphDevMgr);
    GpuSubdevice* gpuSubdevice;

    if (gpuDevMgr != nullptr)
    {
        for (GpuDevice* gpuDevice = gpuDevMgr->GetFirstGpuDevice();
             gpuDevice != nullptr;
             gpuDevice = gpuDevMgr->GetNextGpuDevice(gpuDevice))
        {
            for (UINT32 subdevIndex = 0;
                 subdevIndex < gpuDevice->GetNumSubdevices();
                 subdevIndex++)
            {
                gpuSubdevice = gpuDevice->GetSubdevice(subdevIndex);

                unique_ptr<UtlGpu> gpu(new UtlGpu(gpuDevice, gpuSubdevice));
                s_Gpus[gpuSubdevice] = move(gpu);
                s_PythonGpus.push_back(s_Gpus[gpuSubdevice].get());
            }
        }
    }
}

UtlGpu::UtlGpu
(
    GpuDevice* gpuDevice,
    GpuSubdevice* gpuSubdevice
) :
    m_GpuDevice(gpuDevice),
    m_GpuSubdevice(gpuSubdevice)
{
    MASSERT(gpuDevice != nullptr);
    MASSERT(gpuSubdevice != nullptr);
}

void UtlGpu::FreeGpus()
{
    s_PythonGpus.clear();
    s_Gpus.clear();
}

// This function can be called from a UTL script to get
// the list of Gpu objects.
//
py::list UtlGpu::GetGpusPy()
{
    return UtlUtility::ColwertPtrVectorToPyList(s_PythonGpus);
}

vector<std::pair<PHYSADDR, UINT64>> UtlGpu::GetUprRegions() const
{
    ProtectedRegionManager *protectedRegionManager;
    m_GpuDevice->GetProtectedRegionManager(&protectedRegionManager);
    MASSERT(protectedRegionManager);
    return protectedRegionManager->GetUprRegionList();
}

LWGpuResource* UtlGpu::GetGpuResource()
{
    LWGpuResource * pGpuResource = nullptr;
    Test *pTest = Test::FindTestForThread(Tasker::LwrrentThread());

    // UTL can't fetch right test since UTL can launch conlwrrent tests and
    // they own different threads with test. Just pick the first test in the map.
    pTest = pTest ? pTest : Test::GetFirstTestFromMap();

    // If tests have been created at this point (MODS is beyond InitEvent) try
    // to get the associated LWGpuResource
    if (pTest)
    {
        pGpuResource =  pTest->GetGpuResource();
    }

    // If Utl controls the tests or tests have not been created or tests have
    // been created but there is no associated LWGpuResource then acquire the
    // shared LWGpuResource.
    // Only Trace3DTest stores an LWGpuResource- for other tests GetGpuResource
    // will return a nullptr.
    if (Utl::ControlsTests() || !pGpuResource)
    {
        pGpuResource = LWGpuResource::GetGpuByDeviceInstance(
            GetGpudevice()->GetDeviceInst());
    }
    else if (Test::FindTestForThread(Tasker::LwrrentThread()) == nullptr)
    {
        DebugPrintf("%s: Cannot find test object for the thread, return lwgpu of the first test.\n",
            __FUNCTION__);
    }

    MASSERT(pGpuResource);
    return pGpuResource;
}

// Find the UtlGpu object that wraps the specified GpuSubdevice pointer.
//
UtlGpu* UtlGpu::GetFromGpuSubdevicePtr(GpuSubdevice* pGpuSubdevice)
{
    if (s_Gpus.count(pGpuSubdevice) > 0)
    {
        return s_Gpus[pGpuSubdevice].get();
    }

    return nullptr;
}

SmcResourceController * UtlGpu::FetchSmcResourceController()
{
    LWGpuResource * pGpuResource = LWGpuResource::GetGpuByDeviceInstance(
            GetGpudevice()->GetDeviceInst());
    MASSERT(pGpuResource != nullptr);
    SmcResourceController * pSmcResourceController = pGpuResource->GetSmcResourceController();
    return  pSmcResourceController;
}

// This function can be called from a UTL script to create
// a UTLGpuPartition.
//
py::object UtlGpu::CreateGpuPartitionPy
(
    py::kwargs kwargs
)
{
    // Parse arguments from Python script
    vector<UINT32> gpcCountList;
    vector<SmcResourceController::PartitionFlag> partitionFlagList;
    string partitionName;
    bool singleGpcCount = false;
    bool hasGpcCounts = false;
    bool hasPartitionFlags = false;

    // data can be a single integer or a list
    py::object pyGpcCount;
    hasGpcCounts = UtlUtility::GetOptionalKwarg<py::object>(&pyGpcCount, kwargs,
        "gpcCount", "Gpu.CreateGpuPartition");
    if (hasGpcCounts)
    {
        singleGpcCount = UtlUtility::CastPyObjectToListOrSingleElement(pyGpcCount, &gpcCountList);
    }

    hasPartitionFlags = UtlUtility::GetOptionalKwarg<vector<SmcResourceController::PartitionFlag>>(
                            &partitionFlagList, kwargs, "partitionFlag", "Gpu.CreateGpuPartition");
    UtlUtility::GetOptionalKwarg<string>(&partitionName, kwargs, "partitionName", "Gpu.CreateGpuPartition");

    if ((hasPartitionFlags && hasGpcCounts) ||
        (!hasPartitionFlags && !hasGpcCounts))
    {
        UtlUtility::PyError("Gpu.CreateGpuPartitionPy requires exactly one of the following arguments: gpcCount or partitionFlag.");
    }

    UtlGil::Release gil;

    // Fetch SmcResourceController
    SmcResourceController * pSmcResourceMgr = FetchSmcResourceController();
    MASSERT(pSmcResourceMgr != nullptr);

    if (hasPartitionFlags)
    {
        for (auto partitionFlag : partitionFlagList)
        {
            gpcCountList.push_back(pSmcResourceMgr->GetSmcParser()->GetGpcCountFromPartitionFlag(partitionFlag));
        }
    }

    if (gpcCountList.empty())
    {
        UtlUtility::PyError("Gpu.CreateGpuPartitionPy no valid gpcCount/partitionFlag passed.");
    }

    // Create GpuPartition through SmcResourceController.
    vector<GpuPartition*> allGpuPartitions;

    for (UINT32 idx = 0; idx < gpcCountList.size(); idx++)
    {
        SmcResourceController::PartitionFlag partitionFlag = SmcResourceController::PartitionFlag::INVALID;

        // If the user explicitly provided partitionFlags (probably with -smc_mem)
        // then send it along with GpcCount so that AllocGpuPartitions is able to
        // allocate partitions just based on partitionFlags (not GpcCount).
        // Users should not provide GpcCount here if using -smc_mem. GpcCount
        // should only be used with old SMC cmdline args (-smc_partitioning, ...).
        // But we need to send GpcCount each time here since all Hopper UTL+SMC
        // tests have not yet moved to using -smc_mem and -smc_partitioning alloc
        // still uses GpcCount. So there could be a test which specifies
        // partitionFlags here but with old SMC cmdline args.
        if (hasPartitionFlags)
        {
            partitionFlag = partitionFlagList[idx];
        }
        GpuPartition * pGpuPartition = pSmcResourceMgr->CreateOnePartition("", gpcCountList[0], 0, partitionFlag, true, partitionName);
        if (!pGpuPartition)
        {
            UtlUtility::PyError("GpuPartition creation failed.");
        }
        allGpuPartitions.push_back(pGpuPartition);
    }

    RC rc = pSmcResourceMgr->AllocGpuPartitions(allGpuPartitions);

    UtlUtility::PyErrorRC(rc, "Alloc GpuPartitions failed.");

    vector<UtlGpuPartition*> allUtlGpuPartitions;
    for (auto const & pGpuPartition : allGpuPartitions)
    {
        // Add GpuPartition
        pSmcResourceMgr->AddGpuPartition(pGpuPartition);

        // Set GpuPartition
        LWGpuResource * pLWGpuResource = pSmcResourceMgr->GetGpuResource();
        rc = pLWGpuResource->AllocRmClient(pGpuPartition);
        UtlUtility::PyErrorRC(rc, "Error while allocating RM client for GpuPartition with swizzId = %d.",
            pGpuPartition->GetSwizzId());

        pGpuPartition->SetSyspipes();
        allUtlGpuPartitions.push_back(UtlGpuPartition::GetFromGpuPartitionPtr(pGpuPartition));
    }

    pSmcResourceMgr->PrintSmcInfo();

    if (singleGpcCount)
    {
        MASSERT(allUtlGpuPartitions.size() == 1);

        // Acquire gil for any python memory allocation in cast operation
        UtlGil::Acquire gil;

        py::object pyUtlGpuPartition = py::cast(allUtlGpuPartitions[0], py::return_value_policy::reference);

        return pyUtlGpuPartition;
    }
    else
    {
        // Earlier a vector of UtlGpuPartition* was being returned to python with
        // return_value_policy::reference. But since the vector here is an rvalue
        // move policy was being enforced. In such cases, pybind enforces a move
        // policy on the elements of the stl containers as well leading to an error
        // "RuntimeError: return_value_policy = move, but the object is neither movable nor copyable!"
        // Send a list of python objects instead

        // Acquire gil for any python memory allocation in cast operation
        UtlGil::Acquire gil;

        py::list pyUtlGpuPartitionList;

        for (auto pUtlGpuPartition : allUtlGpuPartitions)
        {
            pyUtlGpuPartitionList.append(py::cast(pUtlGpuPartition, py::return_value_policy::reference));
        }

        return pyUtlGpuPartitionList;
    }
}

void UtlGpu::RemoveGpuPartition
(
    py::kwargs kwargs
)
{
    // Fetch SmcResourceController
    SmcResourceController * pSmcResourceMgr = FetchSmcResourceController();
    MASSERT(pSmcResourceMgr != nullptr);

    UINT32 sysPipe;
    UtlGpuPartition * pUtlGpuPartition = nullptr;
    string partitionName;
    bool hasSysPipe = UtlUtility::GetOptionalKwarg<UINT32>(
                            &sysPipe, kwargs, "sysPipe", "Gpu.RemoveGpuPartition");
    bool hasGpuPartition = UtlUtility::GetOptionalKwarg<UtlGpuPartition *>(
        &pUtlGpuPartition, kwargs, "gpuPartition", "Gpu.RemoveGpuPartition");
    bool hasPartitionName = UtlUtility::GetOptionalKwarg<string>(
        &partitionName, kwargs, "partitionName", "Gpu.RemoveGpuPartition");

    if ((hasSysPipe && hasGpuPartition) ||
        (hasSysPipe && hasPartitionName) ||
        (hasGpuPartition && hasPartitionName) ||
        (!hasSysPipe && !hasGpuPartition && !hasPartitionName))
    {
        UtlUtility::PyError("Gpu.RemoveGpuPartition requires exactly one of the following arguments: sysPipe or gpuPartition or name.");
    }

    UtlGil::Release gil;

    if (hasSysPipe)
    {
        SmcResourceController::SYS_PIPE sys = SmcResourceController::SYS_PIPE::NOT_SUPPORT;
        if (sysPipe >= 0 && sysPipe <= 7)
        {
            sys = static_cast<SmcResourceController::SYS_PIPE>(SmcResourceController::SYS_PIPE::SYS0 + sysPipe);
        }
        else
        {
            UtlUtility::PyError("Illegal sysPipe value %d; must be in range 0..7", sysPipe);
        }

        pUtlGpuPartition = GetGpuPartition(sys);

    }
    else if (hasPartitionName)
    {
        pUtlGpuPartition = GetGpuPartitionByName(partitionName);
    }
    if (pUtlGpuPartition == nullptr)
    {
        UtlUtility::PyError("Could not find GpuPartition / Illegal gpuPartition parameter passed in Gpu.RemoveGpuPartition");
    }

    LWGpuResource * pLWGpuResource = pSmcResourceMgr->GetGpuResource();

    vector<GpuPartition *> removeGpuPartitions;
    removeGpuPartitions.push_back(pUtlGpuPartition->GetGpuPartitionRawPtr());
    RC rc;

    pUtlGpuPartition->FreeSmcEngines();

    rc = pLWGpuResource->FreeRmClient(pUtlGpuPartition->GetGpuPartitionRawPtr());
    UtlUtility::PyErrorRC(rc, "Error while freeing RM client for GpuPartition with swizzId = %d.",
        pUtlGpuPartition->GetSwizzId());

    rc = pSmcResourceMgr->FreeGpuPartitions(removeGpuPartitions);
    UtlUtility::PyErrorRC(rc, "Free specified GpuPartition swizzId = %d failed.",
        pUtlGpuPartition->GetSwizzId());

    m_UtlGpuPartitions.erase(
        remove(m_UtlGpuPartitions.begin(), m_UtlGpuPartitions.end(), pUtlGpuPartition),
        m_UtlGpuPartitions.end());
    UtlGpuPartition::FreeGpuPartition(pUtlGpuPartition->GetGpuPartitionRawPtr());
    pSmcResourceMgr->FreeGpuPartition(removeGpuPartitions);
    pSmcResourceMgr->PrintSmcInfo();
}

UtlGpuPartition * UtlGpu::CreateGpuPartition
(
    GpuPartition *pGpuPartition
)
{
    UtlGpuPartition * pUtlGpuPartition = UtlGpuPartition::Create(
            pGpuPartition->GetGpcCount(), pGpuPartition, this);

    m_UtlGpuPartitions.push_back(pUtlGpuPartition);
    return pUtlGpuPartition;
}

// This function can be called from a UTL script to obtain
// a UtlGpuPartition.
//
UtlGpuPartition * UtlGpu::GetGpuPartitionPy
(
    py::kwargs kwargs
)
{
    // Parse arguments from Python script
    UINT32 tmp = ~0U;
    string partitionName;
    bool hasSysPipe = UtlUtility::GetOptionalKwarg<UINT32>(&tmp, kwargs, "sysPipe",
        "Gpu.GetGpuPartition");
    bool hasPartitionName = UtlUtility::GetOptionalKwarg<string>(&partitionName, kwargs, "partitionName",
        "Gpu.GetGpuPartition");

    if (FetchSmcResourceController())
    {
        if (FetchSmcResourceController()->IsSmcMemApi())
        {
            if (hasSysPipe)
            {
                UtlUtility::PyError("GetGpuPartition error: sysPipe cannot be mentioned along with -smc_eng_mem");
            }

            UtlGil::Release gil;

            return GetGpuPartitionByName(partitionName);
        }
        else
        {
            if (hasPartitionName)
            {
                UtlUtility::PyError("GetGpuPartition error: engName cannot be mentioned along with sys pipe value");
            }
            SmcResourceController::SYS_PIPE sys = SmcResourceController::SYS_PIPE::NOT_SUPPORT;
            if (tmp >= 0 && tmp <= 7)
            {
                sys = static_cast<SmcResourceController::SYS_PIPE>(SmcResourceController::SYS_PIPE::SYS0 + tmp);
            }
            else
            {
                UtlUtility::PyError("Illegal sysPipe value %d; must be in range 0..7", tmp);
            }

            UtlGil::Release gil;

            return GetGpuPartition(sys);
        }
    }

    return nullptr;
}

py::list UtlGpu::GetGpuPartitionListPy() const
{
    return UtlUtility::ColwertPtrVectorToPyList(m_UtlGpuPartitions);
}

UtlGpuPartition * UtlGpu::GetGpuPartition
(
    GpuPartition * pGpuPartition
)
{
    for (const auto & gpuPartition : m_UtlGpuPartitions)
    {
        if (gpuPartition->GetGpuPartitionRawPtr() == pGpuPartition)
            return gpuPartition;
    }

    return nullptr;
}

UtlGpuPartition * UtlGpu::GetGpuPartition
(
    SmcResourceController::SYS_PIPE syspipe
)
{
    for (const auto& gpuPartition : m_UtlGpuPartitions)
    {
        const vector<SmcResourceController::SYS_PIPE> sysPipes =
            gpuPartition->GetGpuPartitionRawPtr()->GetAllSyspipes();
        for (const auto& pipe : sysPipes)
        {
            if (pipe == syspipe)
            {
                return gpuPartition;
            }
        }
    }

    return nullptr;
}

UtlGpuPartition * UtlGpu::GetGpuPartitionByName
(
    string name
)
{
    for (const auto& gpuPartition : m_UtlGpuPartitions)
    {
        if (gpuPartition->GetName() == name)
        {
            return gpuPartition;
        }
    }

    return nullptr;
}

// This function associates a surface with the Gpu
void UtlGpu::AddSurface(UtlSurface* pUtlSurface)
{
    if (GetSurface(pUtlSurface->GetName()) == nullptr)
    {
        DebugPrintf("Surface %s is being added in UTL Gpu\n", pUtlSurface->GetName().c_str());
        m_Surfaces.push_back(pUtlSurface);
    }
    else
    {
        ErrPrintf("Surface %s already added in UTL Gpu\n", pUtlSurface->GetName().c_str());
        MASSERT(0);
    }
}

// This function returns the surface whose name matches with the string being passed
UtlSurface* UtlGpu::GetSurface(const string& name)
{
    for (const auto pSurface : m_Surfaces)
    {
        if (pSurface->GetName() == name)
        {
            return pSurface;
        }
    }
    return nullptr;
}

py::list UtlGpu::GetSurfaces()
{
    return UtlUtility::ColwertPtrVectorToPyList(m_Surfaces);
}

UINT32 UtlGpu::GetSmCount()
{
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        UtlUtility::PyError("Gpu.GetSmCount is not lwrrently supported on Amodel.");
    }

    UtlGil::Release gil;
    UINT32 smCount = GetSmPerTpc() * GetTpcCount();

    return smCount;
}

UINT32 UtlGpu::GetTpcCount()
{
    if (m_GpuSubdevice->IsSMCEnabled())
    {
        return GetGpuResource()->GetTpcCount(m_GpuSubdevice->GetSubdeviceInst());
    }
    else
    {
        return m_GpuSubdevice->GetTpcCount();
    }
}

UINT32 UtlGpu::GetSmPerTpc()
{
    if (m_GpuSubdevice->IsSMCEnabled())
    {
        return GetGpuResource()->GetSmPerTpcCount(m_GpuSubdevice->GetSubdeviceInst());
    }
    else
    {
        LW2080_CTRL_GR_INFO info = {0};
        info.index = LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_SM_PER_TPC;

        LW2080_CTRL_GR_GET_INFO_PARAMS params = {0};
        params.grInfoListSize = 1;
        params.grInfoList = (LwP64)&info;

        LwRmPtr lwRm;
        lwRm->Control(lwRm->GetSubdeviceHandle(m_GpuSubdevice),
            LW2080_CTRL_CMD_GR_GET_INFO,
            &params,
            sizeof(params));

        return info.data;
    }
}

UINT32 UtlGpu::GetTpcCountPy(py::kwargs kwargs)
{
    UINT32 virtualGpcNum = 0;

    if (UtlUtility::GetOptionalKwarg<UINT32>(
        &virtualGpcNum, kwargs, "virtualGpcNum", "Gpu.GetTpcCount"))
    {
        UtlGil::Release gil;
        return GetGpuSubdevice()->GetTpcCountOnGpc(virtualGpcNum);
    }
    else
    {
        UtlGil::Release gil;
        return GetTpcCount();
    }
}

bool UtlGpu::IsSmcEnabled()
{
    UtlGil::Release gil;
    return m_GpuSubdevice->IsSMCEnabled();
}

UINT32 UtlGpu::GetMmuEngineIdStart()
{
    UtlGil::Release gil;

    if (!IsSmcEnabled())
    {
        UtlUtility::PyError("gpu.mmuEngineIdStart property should be used in SMC mode only.");
    }

    return MDiagUtils::GetCachedMMUEngineIdStart(m_GpuSubdevice);
}

UINT32 UtlGpu::GetPesCountPy(py::kwargs kwargs)
{
    UINT32 virtualGpcNum = 0;
    if (UtlUtility::GetOptionalKwarg<UINT32>(
        &virtualGpcNum, kwargs, "virtualGpcNum", "Gpu.GetPesCount"))
    {
        UtlGil::Release gil;
        return GetGpuSubdevice()->GetPesCountOnGpc(virtualGpcNum);
    }
    else
    {
        UtlGil::Release gil;
        const UINT32 numGpcs = GetGpuSubdevice()->GetGpcCount();
        UINT32 totalPesCount = 0;
        for (UINT32 gpc = 0; gpc < numGpcs; ++gpc)
        {
            totalPesCount += GetGpuSubdevice()->GetPesCountOnGpc(gpc);
        }
        return totalPesCount;
    }
}

UINT32 UtlGpu::GetGpcCountPy()
{
    UtlGil::Release gil;

    if (m_GpuSubdevice->IsSMCEnabled())
    {
        return GetGpuResource()->GetGpcCount(m_GpuSubdevice->GetSubdeviceInst());
    }
    else
    {
        return m_GpuSubdevice->GetGpcCount();
    }
}

UINT32 UtlGpu::GetGpcMask()
{
    UtlGil::Release gil;

    if (m_GpuSubdevice->IsSMCEnabled())
    {
        return GetGpuResource()->GetGpcMask(m_GpuSubdevice->GetSubdeviceInst());
    }
    else
    {
        return m_GpuSubdevice->GetFsImpl()->GpcMask();
    }
}

py::list UtlGpu::GetEngines(py::kwargs kwargs)
{
    RC rc;
    // Parse arguments from Python script
    auto engineName = UtlUtility::GetRequiredKwarg<LWGpuClasses::Engine>(kwargs, "engineName",
                    "Gpu.GetEngines");
    UtlGpuPartition * pGpuPartition = nullptr;
    UtlUtility::GetOptionalKwarg<UtlGpuPartition *>(&pGpuPartition, kwargs, "gpuPartition", "Gpu.GetEngines");

    UtlGil::Release gil;

    if (m_GpuSubdevice->IsSMCEnabled() && pGpuPartition == nullptr)
    {
        UtlUtility::PyError("After SMC enable, gpuPartition = xx is required to querry Engine information.");
    }

    LwRm * pLwRm = LwRmPtr().Get();
    if (pGpuPartition)
    {
        SmcResourceController * pSmcResourceMgr = FetchSmcResourceController();
        MASSERT(pSmcResourceMgr);
        pLwRm = pSmcResourceMgr->GetGpuResource()->GetLwRmPtr(pGpuPartition->GetGpuPartitionRawPtr());
    }

    vector<UINT32> allEngines;
    vector<UtlEngine *> allMatchedEngines;

    // Loop through all matching engines
    //
    rc = m_GpuDevice->GetEngines(&allEngines, pLwRm);
    UtlUtility::PyErrorRC(rc, "Can't get the engine correctly.");

    for (vector<UINT32>::iterator pEngine = allEngines.begin();
            pEngine != allEngines.end(); ++pEngine)
    {
        if (UtlEngine::MatchedEngine(engineName, *pEngine))
        {
            UtlEngine::Create(*pEngine, this, pGpuPartition);
        }
    }

    vector<UtlEngine*> engineList =
        *UtlEngine::GetEngines(engineName, this, pGpuPartition);

    return UtlUtility::ColwertPtrVectorToPyList(engineList);
}

UtlMemCtrl* UtlGpu::GetMemCtrl(py::kwargs kwargs)
{
    auto memCtrlType = UtlUtility::GetRequiredKwarg<UtlMemCtrl::MemCtrlType>(
        kwargs, "memCtrlType", "Gpu.GetMemCtrl");

    UINT64 address = UtlMemCtrl::INVALID;
    UINT64 memSize = UtlMemCtrl::INVALID;

    if (memCtrlType == UtlMemCtrl::MemCtrlType::FBMEM)
    {
        if (!UtlUtility::GetOptionalKwarg<UINT64>(&address, kwargs, "address", "Gpu.GetMemCtrl") ||
            !UtlUtility::GetOptionalKwarg<UINT64>(&memSize, kwargs, "memSize", "Gpu.GetMemCtrl"))
        {
            UtlUtility::PyError("Gpu.GetMemCtrl requires address and memSize arguments when memCtrlType = FBMEM.");
        }
    }

    UtlGil::Release gil;

    return UtlMemCtrl::GetMemCtrl(memCtrlType, this, LwRmPtr(0).Get(), address, memSize);
}

UtlGpuRegCtrl* UtlGpu::GetRegCtrl()
{
    return new UtlGpuRegCtrl(this);
}

void UtlGpu::SetGpuPartitionMode(py::kwargs kwargs)
{
    // Parse arguments from Python script
    auto partitionMode = UtlUtility::GetRequiredKwarg<SmcResourceController::PartitionMode>
                         (kwargs, "partitionMode", "Gpu.SetGpuPartitionMode");

    UtlGil::Release gil;

    RC rc;
    SmcResourceController * pSmcResourceMgr = FetchSmcResourceController();

    rc = pSmcResourceMgr->SetGpuPartitionMode(partitionMode);

    UtlUtility::PyErrorRC(rc, "SetGpuPartition %s failed.",
        pSmcResourceMgr->GetGpuPartitionModeName(partitionMode));

    // User might provide -smc_partitioning {xxxxxxxx} and call SetGpuPartitionMode to
    // set up Partition mode and then create real partitions in UTL
    // In this case there will be no valid partition created initially in SmcResourceController::Init
    // and hence SW/HW will be in legacy mode
    // Setting up of Partition mode will be an indication that SMC mode is enabled
    m_GpuSubdevice->EnableSMC();
}

map<SmcResourceController::PartitionFlag, UINT32> UtlGpu::GetAvailablePartitionTypeCount(py::kwargs kwargs)
{
    vector<SmcResourceController::PartitionFlag> partitionFlagList;

    UtlUtility::GetOptionalKwarg<vector<SmcResourceController::PartitionFlag>>(
            &partitionFlagList, kwargs, "partitionFlag", "Gpu.GetAvailablePartitionTypeCount");

    UtlGil::Release gil;

    // Fetch SmcResourceController
    SmcResourceController * pSmcResourceMgr = FetchSmcResourceController();
    MASSERT(pSmcResourceMgr != nullptr);

    return pSmcResourceMgr->GetAvailablePartitionTypeCount(partitionFlagList);
}

void UtlGpu::Reset(py::kwargs kwargs)
{
    UINT32 resetType = static_cast<UINT32>(
        UtlUtility::GetRequiredKwarg<ResetType>(kwargs, "type", "Gpu.Reset"));

    UtlGil::Release gil;

    RC rc;

    if (resetType == static_cast<UINT32>(ResetType::SecondaryBus) ||
        resetType == static_cast<UINT32>(ResetType::Fundamental))
    {
        UINT32 pcieConfigSave = 0;
        rc = Registry::Read("ResourceManager", "RMForcePcieConfigSave", &pcieConfigSave);
        UtlUtility::PyErrorRC(rc, "Reading RM regkey RMForcePcieConfigSave failed. "
            "Please specify `-pcie_config_save 1` in the cmdline args when doing a Secondary Bus or Fundamental Reset");

        if (pcieConfigSave == 0)
        {
            UtlUtility::PyError("RM registry RMForcePcieConfigSave is set to 0. "
                "Please specify `-pcie_config_save 1` in the cmdline args when doing a Secondary Bus or Fundamental Reset\n");
        }
    }

    rc = m_GpuDevice->Reset(resetType);

    UtlUtility::PyErrorRC(rc, "Gpu.Reset failed.");
}

void UtlGpu::RecoverFromReset()
{
    UtlGil::Release gil;

    Utl::Instance()->FreePreResetRecoveryObjects();
    RC rc = m_GpuDevice->RecoverFromReset();

    UtlUtility::PyErrorRC(rc, "Gpu.RecoverFromReset failed.");
}

UINT32 UtlGpu::GetGpuId() const
{
    return m_GpuSubdevice->GetGpuId();
}

UINT32 UtlGpu::GetDeviceInst() const
{
    return m_GpuDevice->GetDeviceInst();
}

UINT32 UtlGpu::GetSubdeviceInst() const
{
    return m_GpuSubdevice->GetSubdeviceInst();
}

void UtlGpu::PreemptRunlist(py::kwargs kwargs)
{
    UtlEngine* pEngine = nullptr;
    UtlSmcEngine* pSmcEngine = nullptr;

    bool hasEngine = UtlUtility::GetOptionalKwarg<UtlEngine *>(
                        &pEngine, kwargs, "engine", "Gpu.PreemptRunlist");
    bool hasSmcEngine = UtlUtility::GetOptionalKwarg<UtlSmcEngine *>(
                        &pSmcEngine, kwargs, "smcEngine", "Gpu.PreemptRunlist");

    if ((hasEngine && hasSmcEngine) ||
        (!hasEngine && !hasSmcEngine))
    {
        UtlUtility::PyError("Gpu.PreemptRunlist requires exactly one of the following arguments: engine or smcEngine.");
    }

    UtlGil::Release gil;

    UINT32 engineId = ~0U;
    LwRm* pLwRm = nullptr;

    if (hasEngine)
    {
        if (pEngine == nullptr)
        {
            UtlUtility::PyError("Gpu.PreemptRunlist: The value passed via engine is not a valid engine object.");
        }
        engineId = pEngine->GetEngineId();
        pLwRm = pEngine->GetLwRmPtr();
    }
    else // hasSmcEngine
    {
        if (pSmcEngine == nullptr)
        {
            UtlUtility::PyError("Gpu.PreemptRunlist: The value passed via smcEngine is not a valid smcEngine object.");
        }
        engineId = MDiagUtils::GetGrEngineId(0);
        pLwRm = GetGpuResource()->GetLwRmPtr(pSmcEngine->GetSmcEngineRawPtr());
    }

    m_GpuDevice->StopEngineRunlist(engineId, pLwRm);
}

void UtlGpu::ResubmitRunlist(py::kwargs kwargs)
{
    UtlEngine* pEngine = nullptr;
    UtlSmcEngine* pSmcEngine = nullptr;

    bool hasEngine = UtlUtility::GetOptionalKwarg<UtlEngine *>(
                        &pEngine, kwargs, "engine", "Gpu.ResubmitRunlist");
    bool hasSmcEngine = UtlUtility::GetOptionalKwarg<UtlSmcEngine *>(
                        &pSmcEngine, kwargs, "smcEngine", "Gpu.ResubmitRunlist");

    if ((hasEngine && hasSmcEngine) ||
        (!hasEngine && !hasSmcEngine))
    {
        UtlUtility::PyError("Gpu.ResubmitRunlist requires exactly one of the following arguments: engine or smcEngine.");
    }

    UtlGil::Release gil;

    UINT32 engineId = ~0U;
    LwRm* pLwRm = nullptr;

    if (hasEngine)
    {
        if (pEngine == nullptr)
        {
            UtlUtility::PyError("Gpu.ResubmitRunlist: The value passed via engine is not a valid engine object.");
        }
        engineId = pEngine->GetEngineId();
        pLwRm = pEngine->GetLwRmPtr();
    }
    else // hasSmcEngine
    {
        if (pSmcEngine == nullptr)
        {
            UtlUtility::PyError("Gpu.ResubmitRunlist: The value passed via smcEngine is not a valid smcEngine object.");
        }
        engineId = MDiagUtils::GetGrEngineId(0);
        pLwRm = GetGpuResource()->GetLwRmPtr(pSmcEngine->GetSmcEngineRawPtr());
    }

    m_GpuDevice->StartEngineRunlist(engineId, pLwRm);
}

vector<UINT32> UtlGpu::GetSupportedClasses()
{
    vector<UINT32> classList;
    LwRm* pLwRm = LwRmPtr().Get();

    RC rc = pLwRm->GetSupportedClasses(&classList, m_GpuDevice);
    UtlUtility::PyErrorRC(rc, "Error in retrieving supported classes\n");

    return classList;
}

string UtlGpu::GetChipName() const
{
    return m_GpuSubdevice->DeviceIdString();
}

bool UtlGpu::HasFeature(py::kwargs kwargs)
{
    UINT32 feature = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "feature", "Gpu.HasFeature");
    return m_GpuSubdevice->HasFeature(static_cast<Device::Feature>(feature));
}

void UtlGpu::TlbIlwalidatePy(py::kwargs kwargs)
{
    UtlVaSpace * pVaSpace = UtlUtility::GetRequiredKwarg<UtlVaSpace*>(kwargs, "vaSpace", "Gpu.TlbIlwalidate");
    UtlGil::Release gil;

    if (pVaSpace == nullptr)
    {
        UtlUtility::PyError("Gpu.TlbIlwalidate requires a valid vaspace argument");
    }
    RC rc = TlbIlwalidate(pVaSpace->GetLwRm(), pVaSpace->GetHandle());

    if (rc != OK)
    {
        MASSERT(!"Can't ilwalidate corresponding vaspace's tlb.");
    }
}

// This tlb ilwalidate works for the mmu tlb flush
// To be compatible with other mmu entry actions
// Only out-of-band has defered tlbIlwalidate
//
RC UtlGpu::TlbIlwalidate
(
    LwRm * pLwRm,
    LwRm::Handle hVaSpace
)
{
    LW2080_CTRL_DMA_ILWALIDATE_TLB_PARAMS params = {};
    params.hVASpace = hVaSpace;

    RC rc = pLwRm->ControlBySubdevice(
            m_GpuSubdevice,
            LW2080_CTRL_CMD_DMA_ILWALIDATE_TLB,
            &params, sizeof(params));

    return rc;
}

py::object UtlGpu::GetPdbConfigurePy(py::kwargs kwargs)
{
    UtlSurface *pSurface        = nullptr;
    UtlChannel *pTargetChannel  = nullptr;
    bool shouldIlwalidateReplay = false;
    bool ilwalidateBar1         = false;
    bool ilwalidateBar2         = false;
    UINT64 pdbPhyAddrAlign      = ~0;
    UINT32 pdbAperture          = GMMU_APERTURE_ILWALID;

    UtlUtility::GetOptionalKwarg<UtlSurface *>  (&pSurface,                 kwargs, "surface",                  "Gpu.GetPdbConfigure");
    UtlUtility::GetOptionalKwarg<UtlChannel *>  (&pTargetChannel,           kwargs, "targetChannel",            "Gpu.GetPdbConfigure");
    UtlUtility::GetOptionalKwarg<bool>          (&shouldIlwalidateReplay,   kwargs, "shouldIlwalidateReplay",   "Gpu.GetPdbConfigure");
    UtlUtility::GetOptionalKwarg<bool>          (&ilwalidateBar1,           kwargs, "ilwalidateBar1",           "Gpu.GetPdbConfigure");
    UtlUtility::GetOptionalKwarg<bool>          (&ilwalidateBar2,           kwargs, "ilwalidateBar2",           "Gpu.GetPdbConfigure");

    LwRm* pLwRm = pTargetChannel ? pTargetChannel->GetLwRmPtr() : LwRmPtr().Get();

    RC rc = GetPdbConfigure(pTargetChannel, pSurface, shouldIlwalidateReplay, ilwalidateBar1, ilwalidateBar2, &pdbPhyAddrAlign, &pdbAperture, pLwRm);
    UtlUtility::PyErrorRC(rc, "Unable to get pdb configure.");

    return py::make_tuple(pdbPhyAddrAlign, static_cast<UtlMmu::APERTURE>(pdbAperture));
}

RC UtlGpu::GetPdbConfigure
(
    UtlChannel* pPdbTargetChannel,
    UtlSurface* pSurface,
    bool shouldIlwalidateReplay,
    bool ilwalidateBar1,
    bool ilwalidateBar2,
    UINT64* pdbPhyAddrAlign,
    UINT32* pdbAperture,
    LwRm* pLwRm
)
{
    RC rc;
    LwRm::Handle hVASpace;

    if (pPdbTargetChannel != NULL)
    {
        hVASpace = pPdbTargetChannel->GetVaSpaceHandle();
        LwRm* pLwRm = pPdbTargetChannel->GetLwRmPtr();

        if (shouldIlwalidateReplay)
        {
            MASSERT(pSurface);

            if(hVASpace != pSurface->GetVaSpace())
            {
                ErrPrintf("GetPdbConfigure: The ilwalidated channel which vaspace is not same as the TlbIlwalidateVa surface's "
                          "vaspace at the TLB_ILWALIDATE_REPLAY_CANCEL_VA_GLOBAL case.\n");
                return RC::SOFTWARE_ERROR;
            }
            if (pLwRm != pSurface->GetLwRm())
            {
                ErrPrintf("GetPdbConfigure: The ilwalidated channel's RM client is not same as the TlbIlwalidateVa surface's RM "
                          "client at the TLB_ILWALIDATE_REPLAY_CANCEL_VA_GLOBAL case.\n");
                return RC::SOFTWARE_ERROR;
            }
        }
    }
    else if (ilwalidateBar1)
    {
        CHECK_RC(VaSpaceUtils::GetFermiVASpaceHandle(
            LW_VASPACE_ALLOCATION_INDEX_GPU_HOST,
            pLwRm->GetSubdeviceHandle(m_GpuSubdevice),
            &hVASpace,
            LwRmPtr().Get())); // Since BAR1 is not partitioned, default RM client can be used
    }
    else if (ilwalidateBar2)
    {
        // Need rm support
        MASSERT(!"NOT Supported yet!");
        return RC::UNSUPPORTED_FUNCTION;
    }
    else if (pSurface)
    {
        hVASpace = pSurface->GetVaSpace();
    }
    else
    {
        *pdbAperture = GMMU_APERTURE_ILWALID;
        *pdbPhyAddrAlign = ~0;
        WarnPrintf("Ilwalidate all pdb.\n");
        return rc;
    }

    if (hVASpace == 0)
    {
        CHECK_RC(VaSpaceUtils::GetFermiVASpaceHandle(
                    LW_VASPACE_ALLOCATION_INDEX_GPU_DEVICE,
                    pLwRm->GetDeviceHandle(m_GpuSubdevice->GetParentDevice()),
                    &hVASpace,
                    pLwRm));
    }

    CHECK_RC(VaSpaceUtils::GetVASpacePdbAddress(hVASpace,
         pLwRm->GetSubdeviceHandle(m_GpuSubdevice), pdbPhyAddrAlign, pdbAperture, pLwRm, m_GpuDevice));

    *pdbPhyAddrAlign = (*pdbPhyAddrAlign) >> LW_PFB_PRI_MMU_ILWALIDATE_PDB_ADDR_ALIGNMENT; // 4K align

    return rc;  
}

bool UtlGpu::CheckConfigureLevelPy(py::kwargs kwargs)
{
    UtlChannel *pTargetChannel = nullptr;
    bool isValid = false;

    UtlUtility::GetOptionalKwarg<UtlChannel *>(&pTargetChannel, kwargs, "targetChannel", "Gpu.CheckConfigureLevel");
    UINT32 ilwalidateLevelNum = UtlUtility::GetRequiredKwarg<UINT32>(kwargs, "ilwalidateLevelNum", "Gpu.CheckConfigureLevel");

    LwRm* pLwRm = pTargetChannel ? pTargetChannel->GetLwRmPtr() : LwRmPtr().Get();

    RC rc = CheckConfigureLevel(ilwalidateLevelNum, &isValid, pLwRm);
    UtlUtility::PyErrorRC(rc, "Unable to check configure level.");

    return isValid;
}

RC UtlGpu::CheckConfigureLevel
(
    UINT32 ilwalidateLevelNum,
    bool * isValid,
    LwRm* pLwRm
) const
{
    RC rc;
    LwRm::Handle hVASpace;

    CHECK_RC(VaSpaceUtils::GetFermiVASpaceHandle(
                LW_VASPACE_ALLOCATION_INDEX_GPU_DEVICE,
                pLwRm->GetDeviceHandle(m_GpuDevice),
                &hVASpace,
                pLwRm));

    // Get top level mmu format
    const struct GMMU_FMT * pGmmuFmt = NULL;
    CHECK_RC(VaSpaceUtils::GetVASpaceGmmuFmt(hVASpace,
                pLwRm->GetSubdeviceHandle(m_GpuSubdevice),
                &pGmmuFmt,
                pLwRm));

    if (NULL == pGmmuFmt)
    {
        WarnPrintf("CheckConfigureLevel: vmm is not enabled! RM API GET_PAGE_LEVEL_INFO can't work! Trying old API\n");
        return RC::SOFTWARE_ERROR;
    }

    UINT32 maxLevel = 0;;
    switch (pGmmuFmt->version)
    {
        case GMMU_FMT_VERSION_1:
            maxLevel = 2;
            break;
        case GMMU_FMT_VERSION_2:
            maxLevel = 5;
            break;
#if LWCFG(GLOBAL_ARCH_HOPPER)
        case GMMU_FMT_VERSION_3:
            maxLevel = 6;
            break;
#endif
        default:
            ErrPrintf("CheckConfigureLevel: Unsupported GMMU format version %d! Trying old API\n", pGmmuFmt->version);
            return RC::SOFTWARE_ERROR;
    }

    if (ilwalidateLevelNum > maxLevel)
    {
        WarnPrintf("CheckConfigureLevel: Invalid level number %d, since the max level RM provide is %d.\n", ilwalidateLevelNum, maxLevel);
        return RC::BAD_PARAMETER;
    }

    *isValid = true;
    return rc;
}

void UtlGpu::L2Operation(py::kwargs kwargs)
{
    RC rc;
    L2OperationTargetMemory targetMem = UtlUtility::GetRequiredKwarg<L2OperationTargetMemory>(kwargs, "targetMem", "Gpu.L2Operation");
    L2OperationType operationType = UtlUtility::GetRequiredKwarg<L2OperationType>(kwargs, "operationType", "Gpu.L2Operation");

    UtlGil::Release gil;

    // RM is going to combine two ctrl calls (LW2080_CTRL_CMD_FB_FLUSH_GPU_CACHE
    // and LW0080_CTRL_CMD_DMA_FLUSH)- http://lwbugs/3020210
    // Once that is done we can just call one ctrl call with the right
    // paramters for all L2 operations.
    if (operationType == L2OperationType::Writeback)
    {
        if (targetMem == L2OperationTargetMemory::All)
        {
            LwRmPtr pLwRm;
            LW0080_CTRL_DMA_FLUSH_PARAMS params = {};
            params.targetUnit = DRF_DEF(0080, _CTRL_DMA_FLUSH_TARGET_UNIT, _L2, _ENABLE);
            rc = pLwRm->ControlByDevice(m_GpuDevice, LW0080_CTRL_CMD_DMA_FLUSH,
                &params, sizeof(params));
            UtlUtility::PyErrorRC(rc, "Gpu.L2Operation failed for operationType = WRITEBACK and targetMem = ALL.");
        }
        else
        {
            UtlUtility::PyError("Lwrrently in Gpu.L2Operation, with operationType = WRITEBACK, only ALL is the supported targetMem.");
        }
    }
    else if (operationType == L2OperationType::Evict)
    {
        if (targetMem == L2OperationTargetMemory::Vidmem)
        {
            rc = m_GpuSubdevice->IlwalidateL2Cache(GpuSubdevice::L2_ILWALIDATE_CLEAN);
            UtlUtility::PyErrorRC(rc, "Gpu.L2Operation failed for operationType = EVICT and targetMem = VIDMEM.");
        }
        else
        {
            UtlUtility::PyError("Lwrrently in Gpu.L2Operation, with operationType = EVICT, only VIDMEM is the supported targetMem.");
        }
    }
    else
    {
        UtlUtility::PyError("Unsupported operationType specified in Gpu.L2Operation. Lwrrently, only WRITEBACK and EVICT are supported.");
    }
}

UtlFbInfo* UtlGpu::GetFbInfo()
{
     if (m_pUtlFbInfo == nullptr)
     {
         m_pUtlFbInfo = make_unique<UtlFbInfo>(m_GpuSubdevice, LwRmPtr().Get(), nullptr);
     }
 
     return m_pUtlFbInfo.get();
}

UINT32 UtlGpu::GetLwlinkEnabledMask()
{
    RC rc;
    UtlGil::Release gil;
    
    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS_PARAMS capsParams = {};
    LwRmPtr pLwRm;
    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(m_GpuSubdevice),
                                  LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS,
                                  &capsParams,
                                  sizeof(capsParams));
    UtlUtility::PyErrorRC(rc, "Gpu.GetLwlinkEnabledMask: Couldn't get LwLink Enabled mask!\n");
    
    return (capsParams.enabledLinkMask);
}

void UtlGpu::FbFlush()
{
    RC rc;
    UtlGil::Release gil;

    rc = m_GpuSubdevice->FbFlush(Tasker::NO_TIMEOUT);

    UtlUtility::PyErrorRC(rc, "Gpu.FbFlush failed!\n");
}

void UtlGpu::SetRcRecovery(py::kwargs kwargs)
{
    bool enable = UtlUtility::GetRequiredKwarg<bool>(kwargs, "enable",
        "Gpu.SetRcRecovery");

    UtlGil::Release gil;
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_CMD_RC_RECOVERY_PARAMS params = {};

    if (enable)
        params.rcEnable = LW2080_CTRL_CMD_RC_RECOVERY_ENABLED;
    else
        params.rcEnable = LW2080_CTRL_CMD_RC_RECOVERY_DISABLED;

    rc = pLwRm->ControlBySubdevice(m_GpuSubdevice,
            LW2080_CTRL_CMD_SET_RC_RECOVERY,
            &params, sizeof(params));

    UtlUtility::PyErrorRC(rc, "Gpu.SetRcRecovery failed!\n");
}

UINT32 UtlGpu::GetIrq()
{
    return m_GpuSubdevice->GetIrq();
}

std::pair<PHYSADDR, UINT64> UtlGpu::GetRunlistPool()
{
    UtlGil::Release gil;

    UINT32 allocContigRunlists = 0;
    RC rc = Registry::Read("ResourceManager", "RMVerifAllocContiguousRunlists",
        &allocContigRunlists);

    if ((rc != OK) || (allocContigRunlists == 0))
    {
        UtlUtility::PyError("The Gpu.GetRunlistPool function requires MODS to be run with the command-line argument -alloc_contiguous_runlists.");
    }

    LwRmPtr pLwRm;
    LW208F_CTRL_FIFO_GET_CONTIG_RUNLIST_POOL_PARAMS params = {};

    rc = pLwRm->Control(m_GpuSubdevice->GetSubdeviceDiag(),
        LW208F_CTRL_CMD_FIFO_GET_CONTIG_RUNLIST_POOL,
        &params, sizeof(params));

    UtlUtility::PyErrorRC(rc, "Gpu.GetRunlistPool failed.");

    return std::make_pair(params.physAddr, params.size);
}
