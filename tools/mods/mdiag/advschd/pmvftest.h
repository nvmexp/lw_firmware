/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2016-2017 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file
//! \brief Define a PmVfTest wrapper around a vf

#ifndef INCLUDED_PMVFTEST_H
#define INCLUDED_PMVFTEST_H

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

#ifndef INCLUDED_POLICYMN_H
#include "policymn.h"
#endif

#include "mdiag/vmiopmods/vmiopmdiagelwmgr.h"
class VmiopMdiagElw;

//--------------------------------------------------------------------
//! \brief Abstract wrapper around a vf
//!
//! This abstract class is designed to let the PolicyManager
//! interface to a vf (from mdiag)
//!  without having to deal directly with the subclasses.
//!
class PmVfTest
{
public:
    PmVfTest(PolicyManager * pPolicyManager,
        PmTest * pTest,
        const VmiopMdiagElwManager::VFSetting * pVfSetting);
    ~PmVfTest() {};

    PolicyManager                       *GetPolicyManager() const { return m_pPolicyManager; }
    UINT32                              GetGFID() const  {  return m_GFID;  }
    UINT32                              GetSeqId() const  {  return m_SeqId;  }
    PmTest*                             GetTest() const  {  return m_pTest;  }
    LwRm::Handle                        GetHandle();
    UINT32                              GetBus();
    UINT32                              GetDomain();
    UINT32                              GetFunction();
    UINT32                              GetDevice();
    const VmiopMdiagElwManager::VFSetting * GetVfSetting()
    { return m_pVfSetting; }
    VmiopMdiagElw*                      GetVmiopMdiagElw();
private:
    PolicyManager                           *m_pPolicyManager;
    PmTest                                  *m_pTest;
    const VmiopMdiagElwManager::VFSetting   *m_pVfSetting;
    UINT32                                  m_SeqId;
    UINT32                                  m_GFID;
    VmiopMdiagElw*                          m_pVmiopMdiagElw;
};

#endif
