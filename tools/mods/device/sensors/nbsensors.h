/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/setget.h"

class NbSensors
{
public:
    NbSensors() {};
    virtual ~NbSensors() = default;

    SETGET_PROP(NumFans, UINT32);
    SETGET_PROP(FanRpmAclwracy, UINT32);
    SETGET_PROP(EcTimeoutMs, UINT32);

    RC SetFanPolicyAuto();
    RC SetFanPolicyManual();
    RC GetFanRpm(UINT32 fanIdx, UINT32 *pSpeed);
    RC SetFanRpm(UINT32 fanIdx, UINT32 speed);

    RC GetCpuVrTempC(INT32 *pTemp);
    RC GetGpuVrTempC(INT32 *pTemp);
    RC GetDdrTempC(INT32 *pTemp);

private:
    UINT32 m_NumFans              = 2;
    UINT32 m_FanRpmAclwracy       = 100;
    UINT32 m_EcTimeoutMs          = 1000;

    RC FanEcReadWrite(const vector<UINT08>& data, UINT08 *pRet = nullptr);
    RC TempEcRead(UINT08 data, UINT08 *pRet);
};
