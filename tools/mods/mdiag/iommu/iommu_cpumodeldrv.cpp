/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "iommu_cpumodeldrv.h"
#include "core/include/coreutility.h"
#include "core/include/platform.h"
#include "mdiag/sysspec.h"
#include "mdiag/utils/cpumodel.h"
#include "mdiag/vmiopmods/vmiopmdiagelwmgr.h"

IommuCpuModelDrv::IommuCpuModelDrv()
{
    if (!Platform::IsDefaultMode())
    {
        VmiopMdiagElwManager * pVmiopMdiagElwMgr = VmiopMdiagElwManager::Instance();
        m_Gfid = pVmiopMdiagElwMgr->GetGfidAttachedToProcess();
    }
    else
    {
        m_Gfid = 0;
    }
}

IommuCpuModelDrv::~IommuCpuModelDrv()
{
}

RC IommuCpuModelDrv::CreateAtsMapping
(
    const string & surfaceName,
    const string & addressSpaceName,
    const string & aperture,
    const Pcie * pcie,
    UINT32 testId,
    UINT32 pasid,
    UINT64 atsPageSize,
    UINT64 virtAddr,
    UINT64 physAddr,
    bool atsReadPermission,
    bool atsWritePermission
)
{
    if (!CPUModel::Enabled())
        return OK;

    if (surfaceName.empty())
    {
        DebugPrintf("%s: not support surface without surface name.\n", __FUNCTION__);
        return OK;
    }

    string escapeString;
    escapeString = Utility::StrPrintf("CPU_MODEL|CM_ATS_MAPPING|GFID:=%u|surface_name:=%s|addr_space_name:=%s|test_id:=%u|GVA:=0x%llx|GPA:=0x%llx|PASID:=%u|ats_page_size:=%u|read_permission:=%u|write_permission:=%u|aperture:=%s|action:=initialize",
        m_Gfid,
        surfaceName.c_str(),
        addressSpaceName.c_str(),
        testId,
        virtAddr,
        physAddr,
        pasid,
        static_cast<UINT32>(atsPageSize),
        atsReadPermission,
        atsWritePermission,
        aperture.c_str());

    DebugPrintf("CPU: %s\n", escapeString.c_str());
    Platform::EscapeWrite(escapeString.c_str(), 0, 4, 0);

    return OK;
}

RC IommuCpuModelDrv::UpdateAtsMapping
(
    const string & addressSpaceName,
    const string & aperture,
    const Pcie * pcie,
    UINT32 pasid,
    UINT64 virtAddr,
    UINT64 physAddr,
    IommuDrv::MappingType updateType,
    bool atsReadPermission,
    bool atsWritePermission
)
{
    if (!CPUModel::Enabled())
        return OK;

    string escapeString;

    if (updateType == IommuDrv::UnmapPage)
    {
        escapeString = Utility::StrPrintf("CPU_MODEL|CM_ATS_MAPPING|GFID:=%u|addr_space_name:=%s|GVA:=0x%llx|action:=unmap",
            m_Gfid,
            addressSpaceName.c_str(),
            virtAddr);
    }
    else if (updateType == IommuDrv::RemapPage)
    {
        escapeString = Utility::StrPrintf("CPU_MODEL|CM_ATS_MAPPING|GFID:=%u|addr_space_name:=%s|GVA:=0x%llx|GPA:=0x%llx|aperture:=%s|action:=remap",
            m_Gfid,
            addressSpaceName.c_str(),
            virtAddr,
            physAddr,
            aperture.c_str());
    }
    else
    {
        MASSERT(!"Invalid MappingType.\n");
    }

    DebugPrintf("CPU: %s\n", escapeString.c_str());
    Platform::EscapeWrite(escapeString.c_str(), 0, 4, 0);

    return OK;
}

RC IommuCpuModelDrv::UpdateAtsPermission
(
    const string & addressSpaceName,
    const string & permissionType,
    const Pcie * pcie,
    UINT32 pasid,
    UINT64 virtAddr,
    UINT64 physAddr,
    bool permissiolwalue,
    bool atsReadPermission,
    bool atsWritePermission
)
{
    if (!CPUModel::Enabled())
        return OK;

    string escapeString;

    escapeString = Utility::StrPrintf("CPU_MODEL|CM_ATS_MAPPING|GFID:=%u|addr_space_name:=%s|GVA:=0x%llx|%s_permission:=%u|action:=update_permission",
        m_Gfid,
        addressSpaceName.c_str(),
        virtAddr,
        permissionType.c_str(),
        permissiolwalue);

    DebugPrintf("CPU: %s\n", escapeString.c_str());
    Platform::EscapeWrite(escapeString.c_str(), 0, 4, 0);

    return OK;
}

RC IommuCpuModelDrv::SendGmmuMapping
(
    const string & surfaceName,
    const string & addressSpaceName,
    const string & aperture,
    UINT32 testId,
    UINT32 pasid,
    UINT32 pageSize,
    UINT64 virtAddr,
    UINT64 physAddr,
    bool doUnmap
)
{
    if (!CPUModel::Enabled())
        return OK;

    string escapeString;
    escapeString = Utility::StrPrintf("CPU_MODEL|CM_GMMU_MAPPING|GFID:=%u|surface_name:=%s|addr_space_name:=%s|test_id:=%u|GVA:=0x%llx|GPA:=0x%llx|PASID:=%u|gmmu_page_size:=%u|aperture:=%s|action:=%s",
        m_Gfid,
        surfaceName.c_str(),
        addressSpaceName.c_str(),
        testId,
        virtAddr,
        physAddr,
        pasid,
        pageSize,
        aperture.c_str(),
        doUnmap ? "unmap":"map");

    DebugPrintf("CPU: %s\n", escapeString.c_str());
    Platform::EscapeWrite(escapeString.c_str(), 0, 4, 0);

    return OK;
}

