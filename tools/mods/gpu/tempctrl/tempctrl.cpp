/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "tempctrl.h"
#include <string>

TempCtrl::TempCtrl
(
    UINT32 id,
    string name,
    UINT32 min,
    UINT32 max,
    UINT32 numInst,
    string units
)
    : m_Id(id),
      m_Name(name),
      m_Min(min),
      m_Max(max),
      m_NumInst(numInst),
      m_Units(units),
      m_IsInitialized(false)
{
}

RC TempCtrl::Initialize()
{
    RC rc;
    if (!m_IsInitialized)
    {
        CHECK_RC(InitTempController());
        m_IsInitialized = true;
    }
    return OK;
}

RC TempCtrl::GetTempCtrlVal(UINT32 *pData, UINT32 index)
{
    RC rc;
    MASSERT(pData);

    if (index >= m_NumInst)
    {
        Printf(Tee::PriError, "Index %u is invalid for this sensor\n",
                               index);
        return RC::BAD_PARAMETER;
    }

    if (!m_IsInitialized)
    {
        CHECK_RC(Initialize());
    }
    CHECK_RC(GetVal(pData, index));

    if ((*pData < m_Min) || (*pData > m_Max))
    {
        Printf(Tee::PriWarn, "Sensor value read %u out of allowed range [%u, %u]\n",
                              *pData, m_Min, m_Max);
    }

    return OK;
}

RC TempCtrl::SetTempCtrlVal(UINT32 data, UINT32 index)
{
    RC rc;

    if (data < m_Min || data > m_Max)
    {
        Printf(Tee::PriError, "Value %u to be set out of allowed range [%u, %u]\n",
                               data, m_Min, m_Max);
        return RC::BAD_PARAMETER;
    }
    if (index >= m_NumInst)
    {
        Printf(Tee::PriError, "Index %u is invalid for this sensor\n",
                               index);
        return RC::BAD_PARAMETER;
    }

    if (!m_IsInitialized)
    {
        CHECK_RC(Initialize());
    }
    CHECK_RC(SetVal(data, index));

    return OK;
}

RC TempCtrl::SetTempCtrlPercent(FLOAT32 percent, UINT32 index)
{
    RC rc;
    UINT32 valToSet = (UINT32)(percent * m_Max);  // Round off to integer
    CHECK_RC(SetTempCtrlVal(valToSet, index));
    return OK;
}
