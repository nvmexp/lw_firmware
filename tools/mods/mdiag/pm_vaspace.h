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

//! \file  pm_lwchn.h
//! \brief Defines a PmChannel subclass that wraps around a LWGpuChannel

#ifndef INCLUDED_PM_VASPACE_H
#define INCLUDED_PM_VASPACE_H

#ifndef INCLUDED_PMVASPACE_H
#include "mdiag/advschd/pmvaspace.h"
#endif

#ifndef INCLUDED_VASPACE_H
#include "vaspace.h"
#endif

#ifndef INCLUDED_STL_STACK
#define INCLUDED_STL_STACK
#include <stack>
#endif

class VaSpace;
class GpuDevice;

//!---------------------------------------------------------------
//! \brief PmChannel subclass that wraps around a mdiag LWGpuChannel
//!
class PmVaSpace_Trace3D : public PmVaSpace
{
public:
    PmVaSpace_Trace3D(PolicyManager *pPolicyManager, PmTest *pTest,
                    GpuDevice * pGpuDevice, shared_ptr<VaSpace> pVaSpace,
                    bool IsShared, LwRm* pLwRm);
    virtual ~PmVaSpace_Trace3D() { m_pVaSpace.reset(); };
    virtual VaSpace   * GetVaSpace() const { return m_pVaSpace.get(); }
    virtual string      GetName() const { return m_pVaSpace->GetName(); }
    virtual LwRm::Handle GetHandle() { return m_pVaSpace->GetHandle(); }
    virtual RC          EndTest()   { m_pVaSpace.reset(); return OK; }
private:
    shared_ptr<VaSpace>         m_pVaSpace;
};

#endif
