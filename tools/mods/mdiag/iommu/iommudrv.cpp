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

#include "iommudrv.h"
#include "iommu_cpumodeldrv.h"
#include "smmudrv.h"
#include "core/include/platform.h"
#include "core/include/script.h"
#include "core/include/xp.h"
#include "mdiag/sysspec.h"
#include "mdiag/mdiag.h"

unique_ptr<IommuDrv> IommuDrv::s_IommuDrv;

IommuDrv::~IommuDrv()
{
}

RC IommuDrv::SendGmmuMapping
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
    return OK;
}

RC IommuDrv::AllocDevVaSpace(const Pcie * pcie)
{
    return OK;
}

RC IommuDrv::AllocProcessVaSpace(const Pcie * pcie, UINT32 pasid)
{
    return OK;
}

IommuDrv * IommuDrv::GetIommuDrvPtr()
{
    static bool s_FirstTimeCall = true;

    if (s_FirstTimeCall)
    {
        if (Platform::IsSelfHosted())
        {
            if (Mdiag::IsSmmuEnabled())
            {
                if (Xp::GetOperatingSystem() == Xp::OS_LINUXSIM)
                {
                    // SmmuDrv is protected by SIM_BUILD,
                    // because mfg mods include mdiag but not include sim platform.
                    // It is ok to remove SIM_BUILD after mfg mods removes the dependency with mdiag
#if defined(SIM_BUILD)
                    s_IommuDrv.reset(new SmmuDrv());
#else
                    MASSERT(!"IommuDrv is not supported in non-sim build.\n");
#endif
                }
                else
                {
                    MASSERT(!"IommuDrv/SmmuDrv is not supported in windows.\n");
                }
            }
        }
        else
        {
            s_IommuDrv.reset(new IommuCpuModelDrv());
        }

        s_FirstTimeCall = false;
    }

    return s_IommuDrv.get();
}

void IommuDrv::Destroy()
{
    s_IommuDrv.reset(nullptr);
}

