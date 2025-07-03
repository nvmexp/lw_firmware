/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "vaspace.h"

#include "class/cl90f1.h" // FERMI_VASPACE_A
#include "core/include/lwrm.h"
#include "core/include/tee.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpudev.h"
#include "gpu/utility/fakeproc.h"
#include "mdiag/iommu/iommudrv.h"
#include "lwgpu.h"
#include "lwos.h"

VaSpace::VaSpace
(
    LWGpuResource * pGpuRes,
    LwRm* pLwRm,
    UINT32 testId,
    string name
):
    m_pGpuResource(pGpuRes),
    m_pLwRm(pLwRm),
    m_TestId(testId),
    m_Name(name),
    m_Handle(0),
    m_AtsEnabled(false)
{
    MASSERT(m_pGpuResource != NULL);
    const ArgReader *params = m_pGpuResource->GetLwgpuArgs();

    // By default, the PASID of this address space will be the same as the
    // fake process ID, but a command-line argument can override this.

    m_Pasid = RMFakeProcess::GetFakeProcessID();
    UINT32 argCount = params->ParamPresent("-set_pasid");

    // Loop through all of the -set_pasid arguments to see if any of them
    // refer to the this address space.
    for (UINT32 i = 0; i < argCount; ++i)
    {
        if (m_Name == params->ParamNStr("-set_pasid", i, 0))
        {
            m_Pasid = params->ParamNUnsigned("-set_pasid", i, 1);
            break;
        }
    }

    argCount = params->ParamPresent("-ats_address_space");

    for (UINT32 i = 0; i < argCount; ++i)
    {
        if (m_Name == params->ParamNStr("-ats_address_space", i, 0))
        {
            m_AtsEnabled = true;

            // Initialize smmu stage1 translation table for PASID context
            IommuDrv * iommu = IommuDrv::GetIommuDrvPtr();
            MASSERT(iommu);
            auto pGpuPcie = m_pGpuResource->GetGpuSubdevice()->GetInterface<Pcie>();
            if (OK != iommu->AllocProcessVaSpace(pGpuPcie, m_Pasid))
            {
                ErrPrintf("Failed to create ats address space %s \n", m_Name.c_str());
                MASSERT(0);
            }
        }
    }
}

VaSpace::~VaSpace()
{
    // no need to free default VaSpace which handle is fixed at 0
    if (m_Handle)
    {
        m_pLwRm->Free(m_Handle);

        if (m_Index == Index::FLA)
        {
            RC rc = ControlFlaRange(LW2080_CTRL_FLA_RANGE_PARAMS_MODE_DESTROY);
            MASSERT(rc == OK && "Failed to destruct FLA VAS\n");
        }
    }
}

LwRm::Handle VaSpace::GetHandle()
{
    const ArgReader *params = m_pGpuResource->GetLwgpuArgs();
    if(m_Name == "default" && !params->ParamPresent("-create_separate_address_space"))
    {
        return 0;
    }

    UINT32 rmClientHandle = m_pLwRm->GetClientHandle();

    if(m_Handle)
    {
        Printf(Tee::PriDebug, "%s: find  the desired name %s in testId %u which handle is "
                "0x%08x in RMclientHandle %u.\n", __FUNCTION__, m_Name.c_str(), m_TestId, m_Handle, rmClientHandle);
        return m_Handle;
    }

    RC rc;
    if (m_Index == Index::FLA)
    {
        rc = ControlFlaRange(LW2080_CTRL_FLA_RANGE_PARAMS_MODE_INITIALIZE);
        MASSERT(rc == OK && "Failed to initialize FLA VAS\n");
    }

    LW_VASPACE_ALLOCATION_PARAMETERS vaParams = { 0 };
    if (m_Index == Index::FLA)
    {
        vaParams.index = LW_VASPACE_ALLOCATION_INDEX_GPU_FLA;
    }
    else
    {
        vaParams.index = LW_VASPACE_ALLOCATION_INDEX_GPU_NEW;
    }

    if (m_AtsEnabled)
    {
        DebugPrintf("Address space %s has ATS enabled.\n", m_Name.c_str());
        vaParams.flags |= LW_VASPACE_ALLOCATION_FLAGS_ENABLE_LWLINK_ATS;
    }

    // When running in simulation, RM uses the fake process ID of a MODS thread
    // as the PASID of an address space.  However, there are tests that use
    // multiple address spaces within the same MODS thread.  In order to allow
    // such tests to have multiple different PASIDs, MODS will temporarily
    // change the fake process ID of this thread while allocating the address
    // space in RM, then restore it afterward.

    UINT32 oldProcessId = RMFakeProcess::GetFakeProcessID();
    RMFakeProcess::SetFakeProcessID(m_Pasid);

    rc = m_pLwRm->Alloc(
                m_pLwRm->GetDeviceHandle(m_pGpuResource->GetGpuDevice()),
                &m_Handle,
                FERMI_VASPACE_A,
                &vaParams);
    MASSERT((rc == OK) && "register handle error!\n");

    // Restore the fake process ID of this thread.
    RMFakeProcess::SetFakeProcessID(oldProcessId);

    Printf(Tee::PriDebug, "%s: find the desired name %s in testId %u which handle is "
            "0x%08x in RMclientHandle %u.\n", __FUNCTION__, m_Name.c_str(), m_TestId, m_Handle, rmClientHandle);

    return m_Handle;
}

void VaSpace::SetRange(UINT64 base, UINT64 size)
{
    m_Base = base;
    m_Size = size;
}

RC VaSpace::ControlFlaRange(UINT32 mode)
{
    RC rc;

    DebugPrintf("%s: base=0x%llx size=0x%llx mode=%u\n",
        __FUNCTION__, m_Base, m_Size, mode);

    LW2080_CTRL_FLA_RANGE_PARAMS params = { 0 };
    params.base = m_Base;
    params.size = m_Size;
    params.mode = mode;

    CHECK_RC(m_pLwRm->ControlBySubdevice(m_pGpuResource->GetGpuSubdevice(),
            LW2080_CTRL_CMD_FLA_RANGE,
            &params,
            sizeof(params)));

    return rc;
}
