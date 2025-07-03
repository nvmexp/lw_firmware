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

//! \file
//! \brief Define a PmSmcEngine wrapper around a SmcEngine

#ifndef INCLUDED_PMSMCENGINE_H
#define INCLUDED_PMSMCENGINE_H

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

#ifndef INCLUDED_POLICYMN_H
#include "policymn.h"
#endif

class GpuPartition;
class SmcEngine;

//--------------------------------------------------------------------
//! \brief Abstract wrapper around an SmcEngine 
//!
//! This abstract class is designed to let the PolicyManager
//! interface to an SmcEngine (from mdiag)
//!  without having to deal directly with the subclasses.
//!
class PmSmcEngine
{
public:
    PmSmcEngine(PolicyManager * pPolicyManager,
                PmTest * pTest);
    virtual ~PmSmcEngine() {};

    PolicyManager*      GetPolicyManager() const { return m_pPolicyManager; }
    PmTest*             GetTest() const  {  return m_pTest;  }
    virtual UINT32      GetSysPipe() const = 0;
    virtual LwRm*       GetLwRmPtr() const = 0;
    virtual SmcEngine*  GetSmcEngine() const = 0;
    virtual UINT32      GetSwizzId() const = 0;
    virtual string      GetName() const = 0;
private:
    virtual GpuPartition*       GetGpuPartition() const = 0;
    PolicyManager*      m_pPolicyManager;
    PmTest*             m_pTest;
};

#endif
