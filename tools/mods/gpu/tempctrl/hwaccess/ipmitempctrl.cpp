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

#include "ipmitempctrl.h"

#include "core/include/jscript.h"
#include "core/include/script.h"
#include "jsapi.h"

IpmiTempCtrl::IpmiTempCtrl
(
    UINT32 id,
    string name,
    UINT32 min,
    UINT32 max,
    UINT32 numInst,
    string units
)
    : TempCtrl(id, name, min, max, numInst, units)
{
}

RC IpmiTempCtrl::InitGetSetParams(JSObject *pParams)
{
    JavaScriptPtr pJs;
    RC rc;

    if (OK != pJs->GetProperty(pParams, "NetFn", &m_NetFn))
    {
        Printf(Tee::PriError, "Couldn't parse netfn \n");
        return RC::BAD_PARAMETER;
    }
    if (OK != pJs->GetProperty(pParams, "MasterCmd", &m_MasterCmd))
    {
        Printf(Tee::PriError, "Couldn't parse master read/write cmd\n");
        return RC::BAD_PARAMETER;
    }
    if (OK != pJs->GetProperty(pParams, "BusId", &m_BusId))
    {
        Printf(Tee::PriError, "Couldn't parse bus id\n");
        return RC::BAD_PARAMETER;
    }
    if (OK != pJs->GetProperty(pParams, "SlaveAddr", &m_SlaveAddr))
    {
        Printf(Tee::PriError, "Couldn't parse slave address\n");
        return RC::BAD_PARAMETER;
    }

    JSObject *pGetParams, *pSetParams;
    if (OK != pJs->GetProperty(pParams, "Get", &pGetParams))
    {
        Printf(Tee::PriDebug, "Get not supported \n");
    }
    else
    {
        CHECK_RC(InitGetParams(pGetParams));
        m_IsGetSupported = true;
    }

    if (OK != pJs->GetProperty(pParams, "Set", &pSetParams))
    {
        Printf(Tee::PriDebug, "Set not supported \n");
    }
    else
    {
        CHECK_RC(InitSetParams(pSetParams));
        m_IsSetSupported = true;
    }

    return OK;
}

RC IpmiTempCtrl::InitTempController()
{
    return InitIpmiDevice();
}

RC IpmiTempCtrl::InitGetParams(JSObject *pGetParams)
{
    JavaScriptPtr pJs;
    RC rc;

    CHECK_RC(pJs->GetProperty(pGetParams, "CmdCode", &m_GetCmdCode));
    CHECK_RC(pJs->GetProperty(pGetParams, "ByteCount", &m_GetByteCount));

    JsArray tempRange;
    CHECK_RC(pJs->GetProperty(pGetParams, "ValRange", &tempRange));
    for (UINT32 i = 0; i < tempRange.size(); i++)
    {
        JsArray range;
        UINT32 low, high;
        CHECK_RC(pJs->FromJsval(tempRange[i], &range));
        CHECK_RC(pJs->FromJsval(range[0], &low));
        CHECK_RC(pJs->FromJsval(range[1], &high));
        m_GetValRange.push_back(std::make_pair(low, high));
    }

    return OK;
}

RC IpmiTempCtrl::InitSetParams(JSObject *pSetParams)
{
    JavaScriptPtr pJs;
    RC rc;

    CHECK_RC(pJs->GetProperty(pSetParams, "ByteCount", &m_SetByteCount));
    CHECK_RC(pJs->GetProperty(pSetParams, "CmdCode", &m_SetCmdCode));

    return OK;
}

RC IpmiTempCtrl::GetVal(UINT32 *pVal, UINT32 index)
{
    RC rc;
    MASSERT(pVal);

    if (!m_IsGetSupported)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    vector<UINT08> requestVal(IPMI_READ_REQUEST_BYTES, 0);
    vector<UINT08> responseVal(m_GetByteCount + IPMI_RESPONSE_METADATA_BYTES, 0);

    CHECK_RC(GetTempCtrlValViaIpmi(requestVal, responseVal));

    UINT32 low = m_GetValRange[index].first + IPMI_RESPONSE_METADATA_BYTES;
    UINT32 high = m_GetValRange[index].second + IPMI_RESPONSE_METADATA_BYTES;

    // Concatenating the bytes into a single value based on the range [low:high]
    UINT32 temp = 0;
    for (UINT32 i = 0; i <= (high-low); i++)
    {
        temp |= ((UINT32)(responseVal[low + i]) << (8*i));
    }

    *pVal = temp;
    return OK;
}

RC IpmiTempCtrl::SetVal(UINT32 data, UINT32 index)
{
    RC rc;

    if (!m_IsSetSupported)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // +1 for number of data bytes to be written
    vector<UINT08> requestVal(IPMI_READ_REQUEST_BYTES + 1 + m_SetByteCount, 0);
    vector<UINT08> responseVal(IPMI_RESPONSE_METADATA_BYTES, 0);
    vector<UINT08> writeVal;

    // ASSUMPTION :
    // If there are multiple sensors, format for writes is :
    //   <index, <data bytes to be written - low to high>>
    // In case there is only a single sensor, format :
    //   <data bytes to be written - low to high>
    UINT32 numValBytes = m_SetByteCount;
    if (GetNumInst() > 1)
    {
        // Current implementations expect indices starting from 1
        writeVal.push_back((index + 1) & 0xFF);
        numValBytes = m_SetByteCount - 1;
    }
    for (UINT32 i = 0; i < numValBytes; i++)
    {
        UINT08 temp = (UINT08)(data & 0xFF);
        writeVal.push_back(temp);
        data = data >> 8;
    }

    CHECK_RC(SetTempCtrlValViaIpmi(requestVal, responseVal, writeVal));
    return OK;
}

