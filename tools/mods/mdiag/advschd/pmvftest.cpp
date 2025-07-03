/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2017-2018 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#include "core/include/lwrm.h"
#include "pmvftest.h"
#include "mdiag/vmiopmods/vmiopmdiagelw.h"

//----------------------------------------------------------------------
//! \brief constructor
//!
PmVfTest::PmVfTest
(
    PolicyManager * pPolicyManager,
    PmTest * pTest,
    const VmiopMdiagElwManager::VFSetting * pVfSetting
):
    m_pPolicyManager(pPolicyManager),
    m_pTest(pTest),
    m_pVfSetting(pVfSetting),
    m_pVmiopMdiagElw(nullptr)
{
    MASSERT(pPolicyManager);
    m_GFID = m_pVfSetting->pConfig->gfid;
    m_SeqId = m_pVfSetting->pConfig->seqId;
}

UINT32 PmVfTest::GetDomain()
{
    return GetVmiopMdiagElw()->GetPcieDomain();
}

UINT32 PmVfTest::GetFunction()
{
    return GetVmiopMdiagElw()->GetPcieFunction();
}

UINT32 PmVfTest::GetDevice()
{
    return GetVmiopMdiagElw()->GetPcieDevice();
}

UINT32 PmVfTest::GetBus()
{
    return GetVmiopMdiagElw()->GetPcieBus();
}

LwRm::Handle PmVfTest::GetHandle()
{
    return GetVmiopMdiagElw()->GetPluginHandle();
}

VmiopMdiagElw* PmVfTest::GetVmiopMdiagElw()
{
    if (m_pVmiopMdiagElw == nullptr)
    {
        VmiopMdiagElwManager * pVmiopMgr = VmiopMdiagElwManager::Instance();
        m_pVmiopMdiagElw = pVmiopMgr->GetVmiopMdiagElw(m_GFID);
    }

    return m_pVmiopMdiagElw;
}

