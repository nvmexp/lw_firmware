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

//! \file  pm_smcengine.h
//! \brief Defines a PmSmcEngine subclass that wraps around a SmcEngine

#ifndef INCLUDED_PM_SMCENGINE_H
#define INCLUDED_PM_SMCENGINE_H

#ifndef INCLUDED_PMSMCENGINE_H
#include "mdiag/advschd/pmsmcengine.h"
#endif

#ifndef INCLUDED_SMCENGINE_H
#include "smc/smcengine.h"
#endif

class SmcEngine;

class PmSmcEngine_Trace3D : public PmSmcEngine
{
public:
    PmSmcEngine_Trace3D(PolicyManager *pPolicyManager,
                        PmTest *pTest,
                        SmcEngine *pSmcEngine);
    virtual ~PmSmcEngine_Trace3D() {};
    virtual UINT32 GetSysPipe() const { return m_pSmcEngine->GetSysPipe(); }
    virtual LwRm*   GetLwRmPtr() const;
    SmcEngine*    GetSmcEngine() const { return m_pSmcEngine; }
    virtual UINT32          GetSwizzId() const;
    virtual string GetName() const { return m_pSmcEngine->GetName(); }
private:
    virtual GpuPartition*   GetGpuPartition() const { return m_pSmcEngine->GetGpuPartition(); }
    SmcEngine *m_pSmcEngine;
};

#endif
