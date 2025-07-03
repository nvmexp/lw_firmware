/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "vmiopmdiagelwmgr.h"

#include "core/include/mgrmgr.h"
#include "core/include/rc.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "core/utility/sharedsysmem.h"
#include "ctrl/ctrl2080/ctrl2080gpu.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "mdiag/advschd/pmvftest.h"
#include "mdiag/advschd/policymn.h"
#include "mdiag/lwgpu.h"
#include "mdiag/smc/gpupartition.h"
#include "mdiag/smc/smcresourcecontroller.h"
#include "mdiag/sysspec.h"
#include "mdiag/utl/utl.h"
#include "Lwcm.h"
#include "lwmisc.h"
#include "lwRmReg.h"
#include "vgpu/mods/vmiop-external-mods.h"
#include "vmiopmdiagelw.h"

#include <map>
#include <set>

// Define command line arguments
#define ARG_FB_RESERVED_PTE_SIZE        "-fb_reserved_pte_size"
#define ARG_UNLOAD_VPLUGIN_BEFORE_FLR   "-unload_vplugin_before_flr"

//-----------------------------------------
//  Define at the plugin_resource_manager.h
//-----------------------------------------
VmiopMdiagElwManager * VmiopMdiagElwManager::Instance()
{
    if (s_pInstance == nullptr)
    {
        s_pInstance = make_unique<VmiopMdiagElwManager>(ConstructorTag());
    }

    return static_cast<VmiopMdiagElwManager*>(s_pInstance.get());
}

VmiopMdiagElwManager * VmiopMdiagElwManager::GetInstance()
{
    return static_cast<VmiopMdiagElwManager*>(s_pInstance.get());
}

VmiopElwManager* VmiopElwManager::Instance()
{
    return VmiopMdiagElwManager::Instance();
}

RC VmiopMdiagElwManager::Init()
{
    RC rc;

    if (IsInitialized())
    {
        return OK;
    }

    CHECK_RC(VmiopElwManager::Init());

    if (Platform::IsPhysFunMode())
    {
        VFSetting vfSetting = {};

        // A block of memory used by host RM to put PTE/PDEs for a VF.
        // In production sw, the size of this block memory is callwlated
        // to be 10% of memory assigned to that VF. But in mods verification,
        // less memory is used than its in production elw.
        // According to the advice from RM team, 128M sounds a
        // good maximum. Initialize it to 128m and then update it to
        // min(10%*vf_fblength, 128m) later after vf_fblength is callwlated.
        vfSetting.fbReserveSizePdePte = 0x8000000ull;

        // Reserved vmmu segments:
        // Host RM internal vidmem buffers are allocated from Top.
        // Mods internal vidmem buffers(e.g. USERD) are allocated from Bottom.
        // RM internal vidmem buffers size has been considered in HeapFreeKb.
        // So Mods internal memory size should be reserved seperately from RM
        // in VMMU if it's allocated from Bottom
        //
        // Now in PF mods, the known mods internal vidmem buffers are forced to be
        // allocated from TOT in same way as RM. No need to reserve a seperate vmmu segment.
        vfSetting.fbReserveSize = 0;

        vfSetting.debugLevel = 0x4;
        vfSetting.debugMask = 0x1;

        // PARTITIONID_ILWALID indicates that SMC is not active
        vfSetting.swizzid = PARTITIONID_ILWALID;

        for (VfConfig& vfConfig: m_VfConfigs)
        {
            vfSetting.pConfig = &vfConfig;
            m_VFSettings.push_back(vfSetting);
        }
    }

    return OK;
}

void VmiopMdiagElwManager::ShutDown()
{
    if (s_pInstance != nullptr)
    {
        for (auto& it : m_GfidToVmiopElw)
        {
            const UINT32 gfid = it.first;
            m_TestSync.RemoveRemoteData(gfid);
        }
        m_TestSync.CleanUp();
    }
    VmiopElwManager::ShutDown();
}

static const ParamDecl sriov_params[] = {
    STRING_PARAM("-sriov_gfid", "all vfs' gfid list. Example: -sriov_gfid <VF1 GFID>,<VF2 GFID>,..."),
    STRING_PARAM("-vmmu_seg_bitmask", "all vfs' vmmu table setting. Example: "
                 "-vmmu_seg_bitmask <GFID>:<bitmask0_bitmask1_bitmask2_bitmask3>,"
                 "<GFID>:<bitmask0_bitmask1_bitmask2_bitmask3>,..."),
    STRING_PARAM("-vgpu_plugin_debug", "Configure the loglevel and debug mask for each plugin. Example: "
            "-vgpu_plugin_debug <GFID>:<debuglevel>:<debugmask>,<GFID>:<debuglevel>:<debugmask>,.."
            "Request from bug 200325255"),
    SIMPLE_PARAM("-disable_sriov_block", "By default it syncs VF tests to wait for run before all VF tests ready, "
            "this option disable the default behavior."),
    SIMPLE_PARAM("-enable_sriov_block", "Explicitly enable the VF test sync function to wait for run before all VF "
            "tests ready, "),
    UNSIGNED_PARAM(ARG_FB_RESERVED_PTE_SIZE, "SRIOV config guest fb reserved PDE/PTE size (in MB), 128 (MB) by default."),
    STRING_PARAM("-gfid_partition_map", "Explicitly set the mapping between GFID and GpuPartitioning. "),
    SIMPLE_PARAM(ARG_UNLOAD_VPLUGIN_BEFORE_FLR, "Unload vGpu plugin before reset in SRIOV FLR tests."),
    LAST_PARAM
};

static const ParamConstraints sriov_params_constraints[] =
{
    MUTUAL_EXCLUSIVE_PARAM("-disable_sriov_block", "-enable_sriov_block")
};

RC VmiopMdiagElwManager::RegisterToUtl()
{
    RC rc;
    Utl::CreateUtlVfTests(m_VFSettings);
    return rc;
}

RC VmiopMdiagElwManager::RegisterToPolicyManager()
{
    RC rc;

    PolicyManager * pPolicyMgr = PolicyManager::Instance();
    for (const auto &vfSetting : m_VFSettings)
    {
        PmVfTest * pPmVfTest = new PmVfTest(pPolicyMgr,
                nullptr,
                &vfSetting);
        CHECK_RC(pPolicyMgr->AddVf(pPmVfTest));
    }

    return rc;
}

RC VmiopMdiagElwManager::SetupAllVfTests(ArgDatabase * pArgs)
{
    RC rc;

    MASSERT(Platform::IsPhysFunMode());

    CHECK_RC(ParseCommandLine(pArgs));
    CHECK_RC(VmiopElwManager::SetupAllVfTests(pArgs));
    CHECK_RC(ConfigVf());
    CHECK_RC(RegisterToPolicyManager());
    if (Utl::HasInstance())
    {
        CHECK_RC(RegisterToUtl());
    }

    return OK;
}

RC VmiopMdiagElwManager::RulwfTests()
{
    RC rc;

    for (auto it = m_VFSettings.begin(); it != m_VFSettings.end(); ++it)
    {
        ConfigFrameBufferLength(&(*it));
    }

    CHECK_RC(VmiopElwManager::RulwfTests());

    if (Utl::HasInstance())
    {
        for (const auto & vfSetting : m_VFSettings)
        {
            UINT32 gfid = vfSetting.pConfig->gfid;
            Utl::Instance()->TriggerVfLaunchedEvent(gfid);
        }
    }

    return rc;
}

RC VmiopMdiagElwManager::RulwfTests
(
    const vector<VfConfig>& vfConfigs,
    vector<VFSetting*>& vfSettings
)
{
    RC rc;

    for (auto it = vfSettings.begin(); it != vfSettings.end(); ++it)
    {
        ConfigFrameBufferLength(*it);
    }

    CHECK_RC(VmiopElwManager::RulwfTests(vfConfigs));

    if (Utl::HasInstance())
    {
        for (const auto & vfSetting : vfSettings)
        {
            UINT32 gfid = vfSetting->pConfig->gfid;
            Utl::Instance()->TriggerVfLaunchedEvent(gfid);
        }
    }

    return rc;
}

unique_ptr<VmiopElw> VmiopMdiagElwManager::AllocVmiopElwPhysFun
(
    UINT32 gfid,
    UINT32 seqId
)
{
    MASSERT(Platform::IsPhysFunMode());
    vector<VFSetting *> vfSettings = GetVFSetting(gfid);
    VFSetting * vfSetting = vfSettings[0];
    return make_unique<VmiopMdiagElwPhysFun>(this, gfid,
                                             vfSetting->pConfig->seqId,
                                             vfSetting->debugLevel,
                                             vfSetting->debugMask,
                                             vfSetting->fbReserveSizePdePte,
                                             vfSetting->fbLength,
                                             vfSetting->swizzid,
                                             vfSetting->vmmuSetting);
}

unique_ptr<VmiopElw> VmiopMdiagElwManager::AllocVmiopElwVirtFun
(
    UINT32 gfid,
    UINT32 seqId
)
{
    MASSERT(Platform::IsVirtFunMode());
    return make_unique<VmiopMdiagElwVirtFun>(this, gfid, seqId);
}

RC VmiopMdiagElwManager::ParseCommandLine(ArgDatabase * pArgs)
{
    RC rc;
    unique_ptr<ArgReader> pReader(new ArgReader(sriov_params));

    if (!pReader.get() || !pReader->ParseArgs(pArgs))
    {
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (!pReader->ValidateConstraints(sriov_params_constraints))
    {
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (pReader->ParamPresent("-sriov_gfid"))
    {
        m_SriovGfids = pReader->ParamStr("-sriov_gfid");
    }

    if (pReader->ParamPresent("-vmmu_seg_bitmask"))
    {
        m_SriovVmmus = pReader->ParamStr("-vmmu_seg_bitmask");
    }

    if (pReader->ParamPresent("-vgpu_plugin_debug"))
    {
        m_PluginDebugSettings = pReader->ParamStr("-vgpu_plugin_debug");
    }

    if (Platform::IsPhysFunMode())
    {
        m_OverrideFbReservedPteSizeMB =
                pReader->ParamUnsigned(ARG_FB_RESERVED_PTE_SIZE, 0);
    }

    if (Platform::IsPhysFunMode() && pReader->ParamPresent("-gfid_partition_map"))
    {
        m_GFIDGpuPartitioningMappings = pReader->ParamStr("-gfid_partition_map");
    }

    if (pReader->ParamPresent(ARG_UNLOAD_VPLUGIN_BEFORE_FLR) &&
        (MDiagUtils::IsChipTu10x() || MDiagUtils::IsChipTu11x()))
    {
        InfoPrintf("SRIOIV %s to set early vplugin unload required for TU10x/11x.\n",
            __FUNCTION__);
        SetEarlyPluginUnload();
    }
    else
    {
        InfoPrintf("SRIOIV %s to delay vplugin unload after VmiopElw exit.\n",
            __FUNCTION__);
    }

    // Make both PF&VF no SRIOV test sync-up as the default behavior ahead of
    // a fix for setup stages.
    //if (Platform::IsPhysFunMode())
    {
        if (!pReader->ParamPresent("-enable_sriov_block") ||
            pReader->ParamPresent("-disable_sriov_block"))
        {
            m_TestSync.Disable();
            m_TestSync.MakeLocalData();
        }
    }

    return rc;
}

RC VmiopMdiagElwManager::ConfigPluginDebugSettings()
{
    RC rc;
    string pluginDebugSettings = m_PluginDebugSettings;
    vector<string> pluginDebugPairs;
    CHECK_RC(Utility::Tokenizer(pluginDebugSettings, ",", &pluginDebugPairs));

    for (const auto &pluginDebugPair : pluginDebugPairs)
    {
        vector<string> debugPairs;

        CHECK_RC(Utility::Tokenizer(pluginDebugPair, ":", &debugPairs));
        MASSERT(debugPairs.size() == 3);

        string gfid = debugPairs[0];
        vector<VFSetting *> vfSettings = GetVFSetting(std::atoi(gfid.c_str()));

        for (auto pVfSetting : vfSettings)
        {
            pVfSetting->debugLevel = Utility::Strtoull(debugPairs[1].c_str(), 0, 0);
            pVfSetting->debugMask = Utility::Strtoull(debugPairs[2].c_str(), 0, 0);
        }
    }

    return rc;
}

RC VmiopMdiagElwManager::ConfigGfids()
{
    RC rc;
    size_t pos;
    string sriovGfids = m_SriovGfids;
    vector<UINT32> gfids;

    while ((pos = sriovGfids.find(",", 0)) != string::npos)
    {
        string gfidStr = sriovGfids.substr(0, pos);
        sriovGfids = sriovGfids.substr(pos + 1);
        gfids.push_back(std::atoi(gfidStr.c_str()));
    }

    if (!sriovGfids.empty())
    {
        gfids.push_back(std::atoi(sriovGfids.c_str()));
    }

    if (gfids.empty() || gfids.size() < m_VFSettings.size())
    {
        UINT32 GfidStart = 1;
        for (auto i = gfids.size(); i < m_VFSettings.size(); ++i)
        {
            while (1)
            {
                UINT32 gfid = GfidStart++;
                if (find(gfids.begin(), gfids.end(), gfid) == gfids.end())
                {
                    DebugPrintf("SRIOV: no -sriov_gfid in the PF command line or -sriov_gfid is "
                            "less than the exelwtable file in the yml file."
                            "Mods help to assgin the GFID=%d.\n", gfid);
                    gfids.push_back(gfid);
                    break;
                }
            }
        }
    }

    for (UINT32 i = 0; i < static_cast<UINT32>(m_VFSettings.size()); i++)
    {
        m_VFSettings[i].pConfig->gfid = gfids[i];
    }
    CHECK_RC(VmiopElwManager::ConfigGfids());

    return rc;
}

RC VmiopMdiagElwManager::ConfigSwizzid()
{
    RC rc;

    if (Platform::IsVirtFunMode() || m_GFIDGpuPartitioningMappings.empty())
    {
        return OK;
    }

    vector<string> gfidMappings;
    CHECK_RC(Utility::Tokenizer(m_GFIDGpuPartitioningMappings, "}", &gfidMappings));
    vector<string> strGfids;

    for (auto & gfidConfig : gfidMappings)
    {
        // skip {
        size_t pos;
        if ((pos = gfidConfig.find_first_of("{")) != 0)
        {
            ErrPrintf("%s: invalid command line %s. Please double check.\n",
                    __FUNCTION__, gfidConfig.c_str());
            return RC::SOFTWARE_ERROR;
        }

        string strGfid = "";
        if (pos + 1 != string::npos)
        {
            strGfid = gfidConfig.substr(++pos, string::npos);
        }
        strGfids.push_back(strGfid);        
    }

    LWGpuResource * pRes = LWGpuResource::FindFirstResource();
    SmcResourceController *pGpuResourceCtrller= pRes->GetSmcResourceController();
    const vector<GpuPartition*> &gpuPartitions = pGpuResourceCtrller->GetActiveGpuPartition();
    vector<GpuPartition*> availablePartitions;
    UINT32 index = 0;

    if (strGfids.size() > gpuPartitions.size())
    {
        ErrPrintf("Too many partitions in -gfid_partition_map as compared to -smc_partitioning or -smc_mem.\n");
        return RC::SOFTWARE_ERROR;
    }

    for (;  index < strGfids.size(); ++index)
    {
        string strGfid = strGfids[index];
        // handle {} in {4}{}{8,9}
        if (strGfid.empty())
            continue;
        GpuPartition * pGpuPartition = gpuPartitions[index];
        vector<string> gfids;
        CHECK_RC(Utility::Tokenizer(strGfid, ",", &gfids));
        for (auto & gfidstr : gfids)
        {
            UINT32 gfid = std::atoi(gfidstr.c_str());
            vector<VFSetting *> vfsettings = GetVFSetting(gfid);
            for (auto & vfsetting : vfsettings)
            {
                vfsetting->swizzid = pGpuPartition->GetSwizzId();
            }
        }
        availablePartitions.push_back(pGpuPartition);
    }
    availablePartitions.insert(availablePartitions.end(),
                gpuPartitions.begin() + index, gpuPartitions.end());

    // Looping the vfsetting to find the unseted GpuPartition
    UINT32 latestIndex = 0;
    for (auto & vfsetting : m_VFSettings)
    {
        if (vfsetting.swizzid == PARTITIONID_ILWALID)
        {
            if (latestIndex == availablePartitions.size())
                latestIndex = 0;

            vfsetting.swizzid = availablePartitions[latestIndex++]->GetSwizzId();
        }
    }
    
    return rc;
}

RC VmiopMdiagElwManager::ConfigVmmu()
{
    RC rc;
    string sriovVmmus = m_SriovVmmus;
    vector<string> vmmuPairs;
    CHECK_RC(Utility::Tokenizer(sriovVmmus, ",", &vmmuPairs));

    for (auto it = vmmuPairs.begin(); it != vmmuPairs.end(); ++it)
    {
        vector<string> vmmuGfidPairs;
        CHECK_RC(Utility::Tokenizer(*it, ":", &vmmuGfidPairs));
        MASSERT(vmmuGfidPairs.size() == 2);

        string gfid = vmmuGfidPairs[0];
        vector<VFSetting *> vfSettings = GetVFSetting(std::atoi(gfid.c_str()));

        string vmmuSettings = vmmuGfidPairs[1];
        vector<string> maskPairs;
        CHECK_RC(Utility::Tokenizer(vmmuSettings, "_", &maskPairs));
        // Just support hex base
        for (UINT32 i = 0; i < maskPairs.size(); ++i)
        {
            UINT64 mask = Utility::Strtoull(maskPairs[i].c_str(), 0, 16);
            for (auto pVfSetting : vfSettings)
            {
                pVfSetting->vmmuSetting.push_back(mask);
            }
        }
    }

    return rc;
}

RC VmiopMdiagElwManager::SanityCheckVmmu()
{
    RC rc;

    PolicyManager *pPolicyMgr = PolicyManager::Instance();
    if (pPolicyMgr->GetStartVfTestInPolicyManager())
    {
        // Disable the vmmu sanity check if the vf is launched by policy manager
        return rc;
    }

    for (auto it = m_VFSettings.begin(); it != m_VFSettings.end(); ++it)
    {
        if (it->vmmuSetting.size() == 0)
        {
            continue;
        }
       
        for (auto jt = next(it); jt != m_VFSettings.end(); ++jt)
        {
            if (jt->vmmuSetting.size() == 0)
            {
                continue;
            }

            UINT32 count = 0;

            if ((it->pConfig->gfid != jt->pConfig->gfid) &&
                (it->swizzid == jt->swizzid))
            {
                MASSERT (it->vmmuSetting.size() == jt->vmmuSetting.size());

                while (count < jt->vmmuSetting.size())
                {
                    if ((it->vmmuSetting[count] & jt->vmmuSetting[count]) != 0)
                    {
                        ErrPrintf("SRIOV: VMMU setting has overlap. Please double check -vmmu_seg_bitmask.\n");
                        return RC::SOFTWARE_ERROR;
                    }
                    count++;
                }
            }
        }
    }

    return rc;
}

RC VmiopMdiagElwManager::SendVmmuInfo()
{
    RC rc;

    map<UINT32, string> gfidVmmuBitMaskMap; 

    auto lwgpu = LWGpuResource::FindFirstResource();

    MASSERT(lwgpu);

    for (auto & vfSetting : m_VFSettings)
    {
        LwRmPtr pLwRm;
        LW2080_CTRL_GPU_GET_VMMU_SPA_BITMASK_PARAMS params = {};
        params.gfid = vfSetting.pConfig->gfid;
        rc = pLwRm->ControlBySubdevice(lwgpu->GetGpuDevice()->GetSubdevice(0),
                                       LW2080_CTRL_CMD_GPU_GET_VMMU_SPA_BITMASK,
                                       &params, sizeof(params));

        if (rc != OK)
        {
            ErrPrintf("%s, gfid %d failed to get vmmu bit mask, RC 0x%x",
                       __FUNCTION__, vfSetting.pConfig->gfid, rc.Get());
            MASSERT(0);
        }

        vector<UINT32> vmmuBitmask(begin(params.vmmuBitmask), end(params.vmmuBitmask));
        string str = Utility::StrPrintf("0x%x", vmmuBitmask[0]);
        for(UINT32 i = 1; i < params.numDwords; ++i)
        {
            str += Utility::StrPrintf(",0x%x", vmmuBitmask[i]);
        }
        gfidVmmuBitMaskMap.insert(make_pair(vfSetting.pConfig->gfid, str));
    }

    string escapeString = Utility::StrPrintf("CPU_MODEL|CM_VMMU|num_vf:=%u|segment_size:=0x%llx",
        GetVfCount(),
        GetVmmuSegmentSize());

    for (auto vmmuBitMaskEntry : gfidVmmuBitMaskMap)
    {
        escapeString += Utility::StrPrintf("|gfid%u:=%u|vmmu_mask%u:=%s",
            vmmuBitMaskEntry.first,
            vmmuBitMaskEntry.first,
            vmmuBitMaskEntry.first,
            vmmuBitMaskEntry.second.c_str());
    }

    DebugPrintf("CPU: %s\n", escapeString.c_str());
    Platform::EscapeWrite(escapeString.c_str(), 0, 4, 0);

    return rc;
}

vector<VmiopMdiagElwManager::VFSetting *> VmiopMdiagElwManager::GetVFSetting(UINT32 GFID)
{
    if (Platform::IsVirtFunMode())
        return vector<VmiopMdiagElwManager::VFSetting *>{};

    vector <VFSetting *> vfSettings;
    for (auto it = m_VFSettings.begin(); it != m_VFSettings.end();
            ++it)
    {
        if (it->pConfig->gfid == GFID)
        {
            vfSettings.push_back(&(*it));
        }
    }

    return vfSettings;
}

RC VmiopMdiagElwManager::ConfigVf()
{
    RC rc;

    CHECK_RC(ConfigVmmu());
    CHECK_RC(ConfigSwizzid());
    CHECK_RC(SanityCheckVmmu());
    CHECK_RC(ConfigPluginDebugSettings());

    return rc;
}

//!\ brief Create or Get VmiopMdiagElw object by GFID
//!\ Parameter:
//!\      gfid: GFID the vgpu plugin works for
VmiopMdiagElw* VmiopMdiagElwManager::GetVmiopMdiagElw(UINT32 gfid) const
{
    return dynamic_cast<VmiopMdiagElw*>(GetVmiopElw(gfid));
}

RC VmiopMdiagElwManager::RemoveVmiopElw(UINT32 gfid)
{
    RC rc;
    VmiopMdiagElw* pMdiagElw = GetVmiopMdiagElw(gfid);
    if (pMdiagElw != nullptr)
    {
        m_TestSync.RemoveRemoteData(gfid);
    }
    CHECK_RC(VmiopElwManager::RemoveVmiopElw(gfid));

    return rc;
}

void VmiopMdiagElwManager::ConfigFrameBufferLength(VFSetting * pVfSetting)
{
    UINT32 vfPfCount = GetVfCount() + 1;
        
    UINT64 fbLength = GetFBLength(pVfSetting->swizzid);
    if (pVfSetting->vmmuSetting.size() > 0)
    {
        UINT32 vmmuBitsCount = 0;
        for_each(pVfSetting->vmmuSetting.begin(), pVfSetting->vmmuSetting.end(),
                [& vmmuBitsCount](UINT64 & vmmuMask)
                {
                vmmuBitsCount += Utility::CountBits(vmmuMask);
                });

        if (m_OverrideFbReservedPteSizeMB)
        {
            pVfSetting->fbReserveSizePdePte = UINT64(m_OverrideFbReservedPteSizeMB) << 20;
        }
        else
        {
            // Production sw behavior
            // reserve 10% of vf size for PDE/PTE, maximum to 128m (init value)
            pVfSetting->fbReserveSizePdePte =
                min(pVfSetting->fbReserveSizePdePte,
                        (vmmuBitsCount * GetVmmuSegmentSize() * 10)/100);
        }
        // Need align fbReserveSizePdePte to Platform page (at least 4k page),
        // otherwise assert in vmiop-vgpu.c
        // vmiop_assert_return((dps->guest_fb_length & VMIOPD_PAGE_MASK) == 0, vmiop_error_range);
        pVfSetting->fbReserveSizePdePte = (pVfSetting->fbReserveSizePdePte + Platform::GetPageSize() - 1) &
            ~(Platform::GetPageSize() - 1);

        // Don't aligning fbLength to vmmu segment page, RM will align.
        pVfSetting->fbLength = (vmmuBitsCount * GetVmmuSegmentSize() - pVfSetting->fbReserveSizePdePte);
        // Don't aligning fbLength to vmmu segment page, RM will align.
        pVfSetting->fbLength = (vmmuBitsCount * GetVmmuSegmentSize() - pVfSetting->fbReserveSizePdePte);
    }
    else
    {
        if (m_OverrideFbReservedPteSizeMB)
        {
            pVfSetting->fbReserveSizePdePte = UINT64(m_OverrideFbReservedPteSizeMB) << 20;
        }
        else
        {
            // Production sw behavior
            // reserve 10% of available vf size for PDE/PTE, maximum to 128m (init value)
            pVfSetting->fbReserveSizePdePte = min(pVfSetting->fbReserveSizePdePte,
                    (fbLength - pVfSetting->fbReserveSize) * 10/(vfPfCount * 100));
        }
        // Need align fbReserveSizePdePte to Platform page (at least 4k page),
        // otherwise assert in vmiop-vgpu.c
        // vmiop_assert_return((dps->guest_fb_length & VMIOPD_PAGE_MASK) == 0, vmiop_error_range);
        pVfSetting->fbReserveSizePdePte = (pVfSetting->fbReserveSizePdePte + Platform::GetPageSize() - 1) &
            ~(Platform::GetPageSize() - 1);

        // Don't aligning fbLength to vmmu segment page, RM will align.
        pVfSetting->fbLength = ((fbLength - pVfSetting->fbReserveSize - vfPfCount * pVfSetting->fbReserveSizePdePte)/vfPfCount);
    }
}

UINT64 VmiopMdiagElwManager::GetVmmuSegmentSize()
{
    static UINT64 vmmuSegmentSize = 0;

    if (vmmuSegmentSize)
    {
        return vmmuSegmentSize;
    }

    auto lwgpu = LWGpuResource::FindFirstResource();

    MASSERT(lwgpu);

    LwRmPtr pLwRm;
    LW2080_CTRL_GPU_GET_VMMU_SEGMENT_SIZE_PARAMS params = {};
    RC rc = pLwRm->ControlBySubdevice(lwgpu->GetGpuDevice()->GetSubdevice(0),
                                      LW2080_CTRL_CMD_GPU_GET_VMMU_SEGMENT_SIZE,
                                      &params, sizeof(params));
    if (rc != OK)
    {
        ErrPrintf("%s, failed to get vmmu segment size, RC 0x%x", __FUNCTION__, rc.Get());
        MASSERT(0);
    }

    vmmuSegmentSize = params.vmmuSegmentSize;

    InfoPrintf("%s, vmmu segment size 0x%llx\n", __FUNCTION__, vmmuSegmentSize);

    return vmmuSegmentSize;
}

UINT64 VmiopMdiagElwManager::GetFBLength(UINT32 swizzid)
{
    if (DevMgrMgr::d_GraphDevMgr->NumDevices() != 1)
    {
        ErrPrintf("SRIOV: Could not get FB size because multiple GPU is not supported!\n");
        MASSERT(0);
    }

    auto lwgpu = LWGpuResource::FindFirstResource();

    MASSERT(lwgpu);

    LW2080_CTRL_FB_INFO            fbInfo = {};
    LW2080_CTRL_FB_GET_INFO_PARAMS params = {};
    fbInfo.index          = LW2080_CTRL_FB_INFO_INDEX_RAM_SIZE;
    params.fbInfoListSize = 1;
    params.fbInfoList     = LW_PTR_TO_LwP64(&fbInfo);

    RC rc;

    if (lwgpu->IsSMCEnabled())
    {
        MASSERT(swizzid != PARTITIONID_ILWALID);
        LwRm* pLwRm = lwgpu->GetLwRmPtr(swizzid);
        MASSERT(pLwRm);

        rc = pLwRm->ControlBySubdevice(lwgpu->GetGpuDevice()->GetSubdevice(0),
                                       LW2080_CTRL_CMD_FB_GET_INFO,
                                       &params, sizeof(params));
    }
    else
    {
        LwRmPtr pLwRm;

        rc = pLwRm->ControlBySubdevice(lwgpu->GetGpuDevice()->GetSubdevice(0),
                                       LW2080_CTRL_CMD_FB_GET_INFO,
                                       &params, sizeof(params));
    }

    if (rc != OK)
    {
        ErrPrintf("%s, failed to get FB size, RC 0x%x", __FUNCTION__, rc.Get());
        MASSERT(0);
    }

    return static_cast<UINT64>(fbInfo.data) * 1024ull;
}

///////////////////////////TestSync//////////////////////////////

void VmiopMdiagElwManager::TestSyncData::AddTest(const MDiagUtils::TestStage stage)
{
    m_Tests[stage] = m_Tests[stage] + 1;
    MASSERT(m_pDataBuf->maxNumTests > 0 && m_Tests[stage] <= m_pDataBuf->maxNumTests);
}

bool VmiopMdiagElwManager::TestSyncData::ReadyForRun(const MDiagUtils::TestStage stage) const
{
    if (!IsPending(stage) || m_pDataBuf->maxNumTests < 1)
    {
        return true;
    }
    if (m_Tests[stage] < m_pDataBuf->maxNumTests)
    {
        return false;
    }
    return true;
}

void VmiopMdiagElwManager::TestSyncRemoteData::ReceiveStage()
{
    MASSERT(Platform::IsPhysFunMode());

    UINT32 testID = 0;
    MDiagUtils::TestStage stage = MDiagUtils::TestRunStart;
    VmiopMdiagElw* pElw = VmiopMdiagElwManager::Instance()->GetVmiopMdiagElw(GetGFID());
    if (0 == pElw)
    {
        // Maybe disconnected, skip it.
        return;
    }
    bool ok = pElw->SendReceiveTestStage(&testID, &stage);
    if (ok)
    {
        DebugPrintf(SRIOVTestSyncTag "TestSync::%s: VF 0x%x test ID %d stage %d (%s).\n",
            __FUNCTION__,
            GetGFID(),
            testID,
            stage,
            MDiagUtils::GetTestStageName(stage));
        if (IsPending(stage))
        {
            AddTest(stage);
        }
    }
}

void VmiopMdiagElwManager::TestSyncRemoteData::SendStage(UINT32 testID,
    const MDiagUtils::TestStage stage)
{
    MASSERT(Platform::IsVirtFunMode());

    VmiopMdiagElw* pElw = VmiopMdiagElwManager::Instance()->GetVmiopMdiagElw(GetGFID());
    if (0 == pElw)
    {
        // Maybe disconnected, skip it.
        return;
    }
    bool ok = pElw->SendReceiveTestStage(&testID, const_cast<MDiagUtils::TestStage*>(&stage));
    if (ok)
    {
        DebugPrintf(SRIOVTestSyncTag "TestSync::%s: VF 0x%x, test ID %d stage %d (%s).\n",
            __FUNCTION__,
            GetGFID(),
            testID,
            stage,
            MDiagUtils::GetTestStageName(stage));
    }
}

bool VmiopMdiagElwManager::TestSyncRemoteData::ReadyForRun(const MDiagUtils::TestStage stage) const
{
    VmiopMdiagElw* pElw = VmiopMdiagElwManager::Instance()->GetVmiopMdiagElw(GetGFID());
    if (0 == pElw ||
        !pElw->IsRemoteProcessRunning())
    {
         // Maybe disconnected, skip it.
        return true;
    }

    return TestSyncData::ReadyForRun(stage);
}

VmiopMdiagElwManager::TestSync::TestSync()
{
    m_StageList.push_back(MDiagUtils::TestSetupStart);
    m_StageList.push_back(MDiagUtils::TestSetupEnd);
    m_StageList.push_back(MDiagUtils::TestBeforeCrcCheck);
    m_StageList.push_back(MDiagUtils::TestRunEnd);
    m_LocalDataBuf.maxNumTests = 1;
    m_LocalDataBuf.needSync = true;
    GetLocalData()->LinkUpDataBuf(&m_LocalDataBuf);
    MakeLocalData();
}

void VmiopMdiagElwManager::TestSync::MakeLocalData()
{
    GetLocalData()->InitStages(m_NeedSync, m_StageList);
}

VmiopMdiagElwManager::TestSyncData* VmiopMdiagElwManager::TestSync::GetLocalData()
{
    return &m_LocalData;
}

VmiopMdiagElwManager::TestSyncRemoteData*
VmiopMdiagElwManager::TestSync::MakeRemoteData(UINT32 gfid, TestSyncDataBuf* pBuf)
{
    TestSyncRemoteData* pData = 0;
    auto it = m_RemoteData.find(gfid);
    if (it != m_RemoteData.end())
    {
        pData = &it->second;
    }
    else
    {
        TestSyncRemoteData data;
        auto result = m_RemoteData.insert(make_pair(gfid, data));
        if (result.second)
        {
            it = result.first;
            pData = &it->second;
        }
    }
    if (pData != 0)
    {
        pData->SetGFID(gfid);
        pData->LinkUpDataBuf(pBuf);
        if (Platform::IsVirtFunMode() && 1 == m_RemoteData.size())
        {
            // If VF MODS, needs overwrite needSync flag updated from PF MODS.
            m_NeedSync = pBuf->needSync;
            MakeLocalData();
        }
        if (Platform::IsPhysFunMode())
        {
            // Only local data PF need InitStages().
            // VF does not call InitStages().
            pData->InitStages(m_NeedSync, m_StageList);
        }
    }

    return pData;
}

void VmiopMdiagElwManager::TestSync::SetRemoteMaxNumTests(size_t num)
{
    if (Platform::IsVirtFunMode())
    {
        for (auto& it : m_RemoteData)
        {
            it.second.SetMaxNumTests(num);
        }
    }
}

bool VmiopMdiagElwManager::TestSync::ReadyForRun(const MDiagUtils::TestStage stage)
{
    // Need check remote data ahead of local data.
    for (auto& it : m_RemoteData)
    {
        if (Platform::IsPhysFunMode())
        {
            it.second.ReceiveStage();
        }
        if (!it.second.ReadyForRun(stage))
        {
            return false;
        }
        // else fall through.
    }

    return GetLocalData()->ReadyForRun(stage);
}

void VmiopMdiagElwManager::TestSync::Unblock(const MDiagUtils::TestStage stage)
{
    if (Platform::IsPhysFunMode())
    {
        if (GetLocalData()->IsPending(stage))
        {
            DebugPrintf(SRIOVTestSyncTag "TestSync::%s: Unblocking test stage %d (%s).\n",
                __FUNCTION__,
                int(stage),
                MDiagUtils::GetTestStageName(stage));
        }
        for (auto& it : m_RemoteData)
        {
            it.second.Unblock(stage);
        }
        GetLocalData()->Unblock(stage);
    }
}

void VmiopMdiagElwManager::TestSync::UpdateLocalData(UINT32 testID, const MDiagUtils::TestStage stage)
{
    MASSERT(Platform::IsPhysFunMode() || Platform::IsVirtFunMode());

    TestSyncData* pData = GetLocalData();
    bool isPending = pData->IsPending(stage);
    if (isPending && Platform::IsVirtFunMode())
    {
        // VF side need keep local data updated from remote data.
        bool unblocked = true;
        for (const auto& it : m_RemoteData)
        {
            if (it.second.IsPending(stage))
            {
                unblocked = false;
                break;
            }
        }
        if (unblocked)
        {
            pData->Unblock(stage);
            isPending = false;
        }
    }
    DebugPrintf(SRIOVTestSyncTag "TestSync::%s: Local tests: test ID %d stage %d (%s) %s.\n",
        __FUNCTION__,
        testID,
        stage,
        MDiagUtils::GetTestStageName(stage),
        isPending ? "Pending, waiting for exelwting" : "Not Blocked");
    if (isPending)
    {
        pData->AddTest(stage);
    }
}

void VmiopMdiagElwManager::TestSync::UpdateRemoteData(UINT32 testID, const MDiagUtils::TestStage stage)
{
    if (Platform::IsVirtFunMode())
    {
        // Send over to PF by shared buffer.
        for (auto& it : m_RemoteData)
        {
            it.second.SendStage(testID, stage);
        }
    }
}

void VmiopMdiagElwManager::TestSync::Synlwp(UINT32 testID, const MDiagUtils::TestStage stage)
{
    if (Platform::IsPhysFunMode() || Platform::IsVirtFunMode())
    {
        DebugPrintf(SRIOVTestSyncTag "TestSync::%s: test ID %d stage %d (%s).\n",
            __FUNCTION__,
            testID,
            stage,
            MDiagUtils::GetTestStageName(stage));

        UpdateLocalData(testID, stage);
        UpdateRemoteData(testID, stage);

        SynlwpAll(stage);
    }
}

void VmiopMdiagElwManager::TestSync::SynlwpAll(const MDiagUtils::TestStage stage)
{
    bool isPending = GetLocalData()->IsPending(stage);
    if (isPending)
    {
        DebugPrintf(SRIOVTestSyncTag "TestSync::%s: Checking for test stage %d (%s) sync-up...\n",
            __FUNCTION__,
            int(stage),
            MDiagUtils::GetTestStageName(stage));
    }

    while (!ReadyForRun(stage))
    {
        Tasker::Yield();
    }
    Unblock(stage);
    if (isPending)
    {
        DebugPrintf(SRIOVTestSyncTag "TestSync::%s: Test stage %d (%s) unblocked.\n",
            __FUNCTION__,
            int(stage),
            MDiagUtils::GetTestStageName(stage));
    }
}

void VmiopMdiagElwManager::TestSync::Stop()
{
    GetLocalData()->Clear();
    for (auto& it : m_RemoteData)
    {
        it.second.Clear();
    }
}

void VmiopMdiagElwManager::TestSync::RemoveRemoteData(UINT32 gfid)
{
    auto it = m_RemoteData.find(gfid);
    if (it != m_RemoteData.end())
    {
        m_RemoteData.erase(it);
    }
}

void VmiopMdiagElwManager::StopTestSync()
{
    if (Platform::IsPhysFunMode() || Platform::IsVirtFunMode())
    {
        DebugPrintf(SRIOVTestSyncTag "MdiagElwMgr::%s: Stop test sync-up.\n",
            __FUNCTION__);
        m_TestSync.Stop();
    }
}

void VmiopMdiagElwManager::LinkUpTestSyncBuf(VmiopMdiagElw* pElw, TestSyncDataBuf* pBuf)
{
    MASSERT(pElw != 0);
    m_TestSync.MakeRemoteData(pElw->GetGfid(), pBuf);
}

void VmiopMdiagElwManager::TestSynlwp(UINT32 testID, const MDiagUtils::TestStage stage)
{
    m_TestSync.Synlwp(testID, stage);
}

void VmiopMdiagElwManager::SetNumT3dTests(size_t num)
{
    DebugPrintf(SRIOVTestSyncTag "MdiagElwMgr::%s: number tests %d.\n",
        __FUNCTION__, int(num));

    if (Platform::IsPhysFunMode() || Platform::IsVirtFunMode())
    {
        m_TestSync.GetLocalData()->SetMaxNumTests(num);
        if (num < 1)
        {
            StopTestSync();
        }
        m_TestSync.SetRemoteMaxNumTests(num);
    }
    // else do nothing
}

///////////////////////////TestSynlwp End////////////////////////////
