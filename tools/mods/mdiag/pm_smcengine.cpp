/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#include "pm_smcengine.h"
#include "mdiag/advschd/pmsmcengine.h"
#include "mdiag/lwgpu.h"
#include "mdiag/smc/gpupartition.h"

//--------------------------------------------------------------------
//!\brief Constructor
//!
PmSmcEngine_Trace3D::PmSmcEngine_Trace3D
(
    PolicyManager *pPolicyManager,
    PmTest        *pTest,
    SmcEngine     *pSmcEngine
) :
    PmSmcEngine(pPolicyManager, pTest),
    m_pSmcEngine(pSmcEngine)
{
    MASSERT(pSmcEngine);
    MASSERT(pPolicyManager);
}

LwRm* PmSmcEngine_Trace3D::GetLwRmPtr() const
{
    LWGpuResources lwgpuResources = GetPolicyManager()->GetLWGpuResources();

    for (const auto& lwgpu : lwgpuResources)
    {
        LwRm* pLwRm = lwgpu->GetLwRmPtr(m_pSmcEngine);
        if (pLwRm)
            return pLwRm;
    }

    ErrPrintf("Could not find LwRm* for this SmcEngine (SysPipe = %d)\n", GetSysPipe());
    MASSERT(0);

    return nullptr;
}

UINT32 PmSmcEngine_Trace3D::GetSwizzId() const
{
    return m_pSmcEngine->GetGpuPartition()->GetSwizzId();
}
