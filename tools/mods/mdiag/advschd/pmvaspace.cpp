/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2016, 2018 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#include "pmvaspace.h"
#include "pmtest.h"
#include "class/cl90f1.h" // FERMI_VASPACE_A
#include "core/include/lwrm.h"

//----------------------------------------------------------------------
//! \brief constructor
//!
PmVaSpace::PmVaSpace
(
    PolicyManager * pPolicyManager,
    PmTest * pTest,
    GpuDevice * pGpuDevice,
    bool    isGlobal,
    LwRm* pLwRm
):
    m_pTest(pTest),
    m_IsGlobal(isGlobal),
    m_pPolicyManager(pPolicyManager),
    m_pGpuDevice(pGpuDevice),
    m_pLwRm(pLwRm)
{
    MASSERT(pPolicyManager);
    MASSERT(pTest == NULL || pTest->GetPolicyManager() == pPolicyManager);
    MASSERT(pGpuDevice);
    MASSERT(m_pLwRm);
}

PmVaSpace_Mods::PmVaSpace_Mods
(
    PolicyManager * pPolicyManager,
    GpuDevice * pGpuDevice,
    const string & name,
    LwRm* pLwRm
) :
    PmVaSpace(pPolicyManager, NULL, pGpuDevice, true, pLwRm),
    m_Name(name),
    m_Handle(0)
{
}

LwRm::Handle PmVaSpace_Mods::GetHandle()
{
    RC rc;

    if(m_Handle || m_Name == "default")
        return m_Handle;

    MASSERT(m_pLwRm);
    LW_VASPACE_ALLOCATION_PARAMETERS vaParams = { 0 };
    vaParams.index = LW_VASPACE_ALLOCATION_INDEX_GPU_NEW;

    rc = m_pLwRm->Alloc(
                m_pLwRm->GetDeviceHandle(m_pGpuDevice),
                &m_Handle,
                FERMI_VASPACE_A,
                (void *)&vaParams);
    MASSERT((rc == OK) && "register handle error!\n");

    return m_Handle;
}

PmVaSpace_Mods::~PmVaSpace_Mods()
{
    EndTest();
}

RC PmVaSpace_Mods::EndTest()
{
    RC rc;
    // no need to free default VaSpace which handle is fixed at 0
    if (m_Handle)
    {
        MASSERT(m_pLwRm);
        m_pLwRm->Free(m_Handle);
    }
    return rc;
}
