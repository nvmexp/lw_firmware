/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2018 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file
//! \brief Defines the PmSurface decoupling all the concrete surface MODS types

#ifndef INCLUDED_PMTEST_H
#define INCLUDED_PMSURF_H

#ifndef INCLUDED_POLICYMN_H
#include "policymn.h"
#endif

class LWGpuResource;
class PmSmcEngine;

//--------------------------------------------------------------------
//! \brief Abstract class for test management in PolicyManager
//!
//! Encapsulates all the info for any underlying test, no matter what
//! kind of object it is.
//!
class PmTest
{
public:
    PmTest(PolicyManager *pPolicyManager, LwRm* pLwRm);
    virtual ~PmTest();

    PolicyManager     *GetPolicyManager() const { return m_pPolicyManager; }
    virtual string     GetName()          const = 0;
    virtual string     GetTypeName()      const = 0;
    virtual void      *GetUniquePtr()     const = 0;
    virtual GpuDevice *GetGpuDevice()     const = 0;

    virtual RC         Abort()                  = 0;
    virtual bool       IsAborting()       const = 0;

    virtual void SendTraceEvent(const char *eventName, const char *surfaceName)   const = 0;

    virtual RC RegisterPmSurface(const char *Name,
                                 PmSurface  *pSurf)  const { return OK; }
    virtual RC UnRegisterPmSurface(const char *Name) const { return OK; }

    virtual UINT32 GetTestId() const { return ~0; }
    virtual const char *GetTestName() const { return NULL; }

    virtual RC GetUniqueVAS(PmVaSpace ** pVaSpace) { return OK;}
    virtual LwRm* GetLwRmPtr() { return m_pLwRm; }
    virtual PmSmcEngine * GetSmcEngine() = 0;
    virtual int GetDeviceStatus() { return ~0; }
private:
    PmTest(const PmSurface &surf);            // Not implemented
    PmTest &operator=(const PmSurface &surf); // Not implemented

    PolicyManager *m_pPolicyManager;
    LwRm* m_pLwRm;
};

#endif // INCLUDED_PMTEST_H
