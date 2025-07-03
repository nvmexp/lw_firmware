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
#include "gpu/tempctrl/tempctrl.h"
#include "device/utility/genericipmi.h"
#include <vector>

#define IPMI_READ_REQUEST_BYTES 4
#define IPMI_RESPONSE_METADATA_BYTES 2  // For RC and number of bytes returned

class TempCtrl;

class IpmiTempCtrl : public TempCtrl
{
public:
    IpmiTempCtrl(UINT32 id, string name, UINT32 min, UINT32 max, UINT32 numInst, string units);
    virtual ~IpmiTempCtrl() = default;

    virtual RC InitGetSetParams(JSObject *pParams) override;

protected:
    virtual RC GetVal(UINT32 *data, UINT32 index) override;
    virtual RC SetVal(UINT32 data, UINT32 index) override;
    virtual RC InitTempController() override;

private:

    bool m_IsGetSupported = false;
    bool m_IsSetSupported = false;

    IpmiDevice m_IpmiDevice;

    UINT08 m_NetFn     = 0;
    UINT08 m_MasterCmd = 0;
    UINT08 m_BusId     = 0;
    UINT08 m_SlaveAddr = 0;

    UINT32 m_GetCmdCode   = 0;
    UINT32 m_GetByteCount = 0;
    vector<std::pair<UINT32, UINT32>> m_GetValRange;

    UINT32 m_SetCmdCode   = 0;
    UINT32 m_SetByteCount = 0;

    RC InitGetParams(JSObject *pGetParams);
    RC InitSetParams(JSObject *pSetParams);

    RC InitIpmiDevice();
    RC GetTempCtrlValViaIpmi(vector<UINT08> &requestVal, vector<UINT08> &responseVal);
    RC SetTempCtrlValViaIpmi(vector<UINT08> &requestVal, vector<UINT08> &responseVal,
                             vector<UINT08> &writeVal);
};

