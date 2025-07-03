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

//! \file
//! \brief Define a PmVaSpace wrapper around a vaSpace

#ifndef INCLUDED_PMVASPACE_H
#define INCLUDED_PMVASPACE_H

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

#ifndef INCLUDED_POLICYMN_H
#include "policymn.h"
#endif

class VaSpace;
//--------------------------------------------------------------------
//! \brief Abstract wrapper around a VaSpace
//!
//! This abstract class is designed to let the PolicyManager
//! interface to a VaSpace (from mdiag)
//!  without having to deal directly with the subclasses.
//!
class PmVaSpace
{
public:
    PmVaSpace(PolicyManager * pPolicyManager, PmTest * pTest,
             GpuDevice * pGpuDevice, bool isShared, LwRm* pLwRm);
    virtual ~PmVaSpace() {};

    PolicyManager   *GetPolicyManager() const { return m_pPolicyManager; }
    PmTest          *GetTest()  const { return m_pTest; }
    GpuDevice       *GetGpuDevice() const { return m_pGpuDevice; }
    virtual string           GetName()   const = 0;
    virtual VaSpace         *GetVaSpace()   const = 0;
    virtual LwRm::Handle     GetHandle()     = 0;
    bool                     IsGlobal() const { return m_IsGlobal; }
    virtual RC               EndTest() = 0;
    bool                     IsDefaultVaSpace() { return GetHandle() == 0; }
    LwRm*                    GetLwRmPtr() const { return m_pLwRm; }
private:
    PmTest          *m_pTest;
    bool             m_IsGlobal;
protected:
    PolicyManager   *m_pPolicyManager;
    GpuDevice       *m_pGpuDevice;
    LwRm            *m_pLwRm;
};

//--------------------------------------------------------------------
//! \brief Abstract wrapper around a VaSpace
//!
//! This abstract class is designed to let the PolicyManager
//! interface to a VaSpace (from PolicyManager only)
//!  without having to deal directly with the subclasses.
//! It will have some default value
//! The m_pTest is NULL
//! The m_IsGlobal is true
class PmVaSpace_Mods : public PmVaSpace
{
public:
    PmVaSpace_Mods(PolicyManager * pPolicyManager,
                    GpuDevice * pGpuDevice, const string & name, LwRm* pLwRm);
    virtual ~PmVaSpace_Mods();

    virtual string          GetName() const { return m_Name; }
    virtual VaSpace         * GetVaSpace() const { return NULL; }
    virtual LwRm::Handle    GetHandle() ;
    virtual RC              EndTest();

private:
    string       m_Name;
    LwRm::Handle m_Handle;
};
#endif
