/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpioimpl.h"
#include "gpu/include/testdevice.h"

//-----------------------------------------------------------------------------
GpioImpl::GpioChecker::GpioChecker(Gpio* pGpio)
: ErrCounter()
, m_pGpio(pGpio)
{
}

//-----------------------------------------------------------------------------
RC GpioImpl::GpioChecker::Initialize(const map<UINT32, GpioCheckInfo>& checks)
{
    RC rc;
    m_KeyToIdx.clear();
    m_Count.clear();
    m_MaxAllowed.clear();
    m_ToDisable.clear();
    for (auto checksIter = checks.begin(); checksIter != checks.end(); checksIter++)
    {
        m_KeyToIdx[checksIter->first] = static_cast<UINT32>(m_Count.size());
        m_Count.push_back(0);
        m_MaxAllowed.push_back(checksIter->second.maxAllowed);
        m_ToDisable.push_back(checksIter->second.disableWhenMaxReached);
    }
    CHECK_RC(ErrCounter::Initialize("GpioChecker",
                                    static_cast<UINT32>(m_KeyToIdx.size()), 0,
                                    nullptr, MODS_TEST, GPIO_ACTIVITY_PRI));
    m_Active = true;
    return rc;
}

//-----------------------------------------------------------------------------
RC GpioImpl::GpioChecker::Update(UINT32 key)
{
    if (!m_Active || m_KeyToIdx.find(key) == m_KeyToIdx.end())
    {
        return RC::OK;
    }

    UINT32 Idx = m_KeyToIdx[key];
    m_Count[Idx]++;

    // once the Count reacheds a limit, tell RM to stop bothering us
    RC rc;
    if (m_ToDisable[Idx] && (m_Count[Idx] >= m_MaxAllowed[Idx]))
    {
        GpioActivity gpioAction;
        KeyToActivity(key, &gpioAction);
        CHECK_RC(m_pGpio->SetInterruptNotification(gpioAction.pinNum,
                                                   gpioAction.dir,
                                                   false));
        Printf(Tee::PriNormal,
               "Turning off Gpio Notification @ Pin %d. %d already caught\n",
               gpioAction.pinNum, m_Count[Idx]);
    }
    return rc;
}

//------------------------------------------------------------------------------
RC GpioImpl::GpioChecker::ReadErrorCount(UINT64 * pCount)
{
    MASSERT(m_KeyToIdx.size() == m_Count.size());
    for (UINT32 i = 0; i < m_Count.size(); i++)
    {
        pCount[i] = m_Count[i];
    }
    return OK;
}

//------------------------------------------------------------------------------
RC GpioImpl::GpioChecker::OnError(const ErrorData *pData)
{
    for (auto violationIter = m_KeyToIdx.begin(); violationIter != m_KeyToIdx.end(); ++violationIter)
    {
        UINT32 key = violationIter->first;
        UINT32 idx = violationIter->second;
        UINT64 overLimitCount = pData->pNewCount[idx];
        if (0 < overLimitCount)
        {
            Printf(Tee::PriNormal, "On %s ",
                   static_cast<GpioImpl*>(m_pGpio)->GetDevice()->GetName().c_str());

            GpioActivity gpioAction;
            KeyToActivity(key, &gpioAction);
            Printf(Tee::PriNormal, "Gpio %d (%s edge) detected %lld times\n",
                   gpioAction.pinNum,
                   (gpioAction.dir == Gpio::RISING)? "rising":"falling",
                   overLimitCount);
        }
    }
    return RC::EXCEEDED_EXPECTED_THRESHOLD;
}

//-----------------------------------------------------------------------------
/* static */ UINT32 GpioImpl::ActivityToKey(UINT32 pinNum, Direction dir)
{
    UINT32 retVal = pinNum;
    if (RISING == dir)
    {
        retVal |= RISING;
    }
    return retVal;
}

//-----------------------------------------------------------------------------
/* static */ void GpioImpl::KeyToActivity(UINT32 key, GpioActivity* pActivity)
{
    MASSERT(pActivity);
    if (key & RISING)
        pActivity->dir = RISING;
    else
        pActivity->dir = FALLING;

    // remove the direction bits on key
    pActivity->pinNum = key & (~RISING);
}
