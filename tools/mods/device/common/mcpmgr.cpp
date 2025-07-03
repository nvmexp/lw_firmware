/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2011 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "device/include/mcpmgr.h"
#include "core/include/massert.h"
#include "device/include/mcpmacro.h"

McpDeviceManager::McpDeviceManager()
{
    m_LwrrentIndex = 0;
    m_FindCount = 0;
    m_vpDevices.clear();
}

McpDeviceManager::~McpDeviceManager()
{
}

RC McpDeviceManager::Find(bool bFindMoreDevices)
{
    RC rc;
    m_FindCount++;
    if ((m_vpDevices.size()) && (bFindMoreDevices == false))
    {
        LOG_INFO("Devices already found!");
        return OK;
    }

    CHECK_RC(PrivateFindDevices());

    if(!m_vpDevices.size())
    {
        LOG_INFO("Didn't find any device!");
        return RC::DEVICE_NOT_FOUND;
    }

    Printf(Tee::PriLow, "Found %d devices\n", (UINT32)m_vpDevices.size());
    return OK;
}

RC McpDeviceManager::Forget()
{
    if (--m_FindCount)
    {
        Printf(Tee::PriLow,"Skipping forget, FindCount = %d\n", m_FindCount);
        return OK;
    }
    for (UINT32 i = 0; i < m_vpDevices.size(); i++)
    {
        if (m_vpDevices[i] != NULL)
        {
            delete m_vpDevices[i];
            m_vpDevices[i] = NULL;
        }
    }

    m_vpDevices.clear();
    m_LwrrentIndex = 0;

    return OK;
}

RC McpDeviceManager::ForcePageRange(UINT32 ForceAddrBits)
{
    RC rc = OK;

    for (UINT32 i=0; i<m_vpDevices.size(); i++)
    {
        RECORD_FIRST_FAILURE(rc, m_vpDevices[i]->ForcePageRange(ForceAddrBits));
    }

    return rc;
}

RC McpDeviceManager::Validate(UINT32 Index)
{
    if (!m_vpDevices.size())
    {
        LOG_ERR("Not Initialize or NO device found");
        return RC::DEVICE_NOT_FOUND;
    }

    if (Index >= m_vpDevices.size())
    {
        LOG_ERR("Index %d is too high! Total Dev %d", Index, (UINT32)m_vpDevices.size());
        return RC::DEVICE_NOT_FOUND;
    }

    return OK;
}

RC McpDeviceManager::GetNumDevices(UINT32 *pNumDev)
{
    MASSERT(pNumDev);
    *pNumDev = m_vpDevices.size();
    return OK;
}

RC McpDeviceManager::GetLwrrent(McpDev** ppDev)
{
    RC rc;
    MASSERT(ppDev);
    CHECK_RC(Validate(m_LwrrentIndex));

    *ppDev = m_vpDevices[m_LwrrentIndex];
    return OK;
}

RC McpDeviceManager::GetLwrrent(UINT32* pIndex)
{
    RC rc;
    MASSERT(pIndex);
    CHECK_RC(Validate(m_LwrrentIndex));
    *pIndex = m_LwrrentIndex;
    return OK;
}

RC McpDeviceManager::SetLwrrent(UINT32 Index)
{
    RC rc;
    CHECK_RC(Validate(Index));

    m_LwrrentIndex = Index;
    return OK;
}

RC McpDeviceManager::GetByIndex(UINT32 Index, McpDev** ppDev)
{
    RC rc;
    MASSERT(ppDev);

    CHECK_RC(Validate(Index));

    *ppDev = m_vpDevices[Index];
    return OK;
}

RC McpDeviceManager::InitializeByDevIndex(UINT32 index)
{
    RC rc = OK;
    CHECK_RC(Validate(index));
    CHECK_RC(SetupDevice(index));
    CHECK_RC(m_vpDevices[index]->Initialize(McpDev::INIT_DONE));

    return rc;
}

RC McpDeviceManager::Initialize(McpDev::InitStates State)
{
    RC rc = OK;
    CHECK_RC(Validate(m_LwrrentIndex));
    CHECK_RC(SetupDevice(m_LwrrentIndex));
    CHECK_RC(m_vpDevices[m_LwrrentIndex]->Initialize(State));

    return rc;
}

RC McpDeviceManager::InitializeAll(McpDev::InitStates State)
{
    RC rc = OK;
    CHECK_RC( Validate(0) );

    for (UINT32 i=0; i<m_vpDevices.size(); i++)
    {
        RECORD_FIRST_FAILURE(rc, SetupDevice(i));
        RECORD_FIRST_FAILURE(rc, m_vpDevices[i]->Initialize(State));
    }
    return rc;
}

RC McpDeviceManager::UninitializeByDevIndex(UINT32 index)
{
    RC rc = OK;

    CHECK_RC(Validate(index));
    CHECK_RC(m_vpDevices[index]->Uninitialize(McpDev::INIT_CREATED));

    return rc;
}

RC McpDeviceManager::Uninitialize(McpDev::InitStates State)
{
    RC rc = OK;

    CHECK_RC(Validate(m_LwrrentIndex));
    CHECK_RC(m_vpDevices[m_LwrrentIndex]->Uninitialize(State));

    return rc;
}

RC McpDeviceManager::UninitializeAll(McpDev::InitStates State)
{
    RC rc = OK;
    CHECK_RC( Validate(0) );

    for (UINT32 i=0; i<m_vpDevices.size(); i++)
    {
        RECORD_FIRST_FAILURE(rc, m_vpDevices[i]->Uninitialize(State));
    }
    return rc;
}

RC McpDeviceManager::PrintInfo(Tee::Priority Pri,
                               bool IsPrintBsc,
                               bool IsPrintCtrl,
                               bool IsPrintExt)
{
    RC rc = OK;

    Printf(Pri, "\nInformation for current devices, index = %d (total %d)\n",
           m_LwrrentIndex, (UINT32)(m_vpDevices.size()));
    CHECK_RC( Validate(m_LwrrentIndex) );
    CHECK_RC(m_vpDevices[m_LwrrentIndex]->PrintInfo(Pri,
                                                    IsPrintBsc,
                                                    IsPrintCtrl,
                                                    IsPrintExt));
    Printf(Pri, "End\n");
    return rc;
}

RC McpDeviceManager::PrintInfoAll(Tee::Priority Pri,
                               bool IsPrintBsc,
                               bool IsPrintCtrl,
                               bool IsPrintExt)
{
    RC rc = OK;

    CHECK_RC( Validate(0) );
    UINT32 totalNum = m_vpDevices.size();
    Printf(Pri, "The number of devices detected: %d\n", totalNum);

    for (UINT32 i=0; i<totalNum; i++)
    {
        if (IsPrintBsc || IsPrintCtrl || IsPrintExt)
            Printf(Pri, "\nDevice Index = %d (out of %d):\n", i, totalNum);

        RECORD_FIRST_FAILURE(rc, m_vpDevices[i]->PrintInfo(Pri,
                                                           IsPrintBsc,
                                                           IsPrintCtrl,
                                                           IsPrintExt));
    }
    return rc;
}

