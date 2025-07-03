/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#include "pm_vaspace.h"
#include "vaspace.h"
#include "sysspec.h"

//--------------------------------------------------------------------
//!\brief Constructor
//!
PmVaSpace_Trace3D::PmVaSpace_Trace3D
(
    PolicyManager *pPolicyManager,
    PmTest        *pTest,
    GpuDevice     *pGpuDevice,
    shared_ptr<VaSpace> pVaSpace,
    bool          isGlobal,
    LwRm          *pLwRm
) :
    PmVaSpace(pPolicyManager, pTest, pGpuDevice, isGlobal, pLwRm),
    m_pVaSpace(pVaSpace)
{
    MASSERT(m_pVaSpace);
    MASSERT(pPolicyManager);
    MASSERT(pGpuDevice);
    MASSERT(pLwRm);
}

