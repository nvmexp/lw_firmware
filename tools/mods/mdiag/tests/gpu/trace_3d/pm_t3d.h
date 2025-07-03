/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2017 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file
//! \brief

#ifndef INCLUDED_PM_T3D_H
#define INCLUDED_PM_T3D_H

#ifndef INCLUDED_PMTEST_H
#include "mdiag/advschd/pmtest.h"
#endif
#include "trace_3d.h"

class PmChannel;
class PmSmcEngine;

//--------------------------------------------------------------------
//! \brief (Concrete) class for a Trace3DTest
//!
class PmTest_Trace3D : public PmTest
{
public:
    PmTest_Trace3D(PolicyManager *pPolicyManager,
                   Trace3DTest   *pTest);
    virtual ~PmTest_Trace3D();
    virtual string     GetName()          const;
    virtual string     GetTypeName()      const;
    virtual void      *GetUniquePtr()     const;
    virtual GpuDevice *GetGpuDevice()     const;

    virtual RC         Abort();
    virtual bool       IsAborting() const;

    virtual void SendTraceEvent(const char *eventName, const char *surfaceName)   const;

    virtual RC RegisterPmSurface(const char *Name,
                                 PmSurface  *pSurf) const;
    virtual RC UnRegisterPmSurface(const char *Name) const;

    virtual UINT32 GetTestId() const { return m_Ptr->GetTestId(); }
    virtual const char* GetTestName() const { return m_Ptr->GetTestName(); }
    virtual PmSmcEngine * GetSmcEngine(); 

    virtual RC GetUniqueVAS(PmVaSpace ** pVaSpace);
    virtual int GetDeviceStatus() { return m_Ptr->GetDeviceStatus(); }
private:
    Trace3DTest *m_Ptr;
    PmSmcEngine * m_pSmcEngine;
};

#endif // INCLUDED_PM_T3D_H
