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

#include "nbsensors.h"
#include "core/include/tasker.h"
#include "core/include/platform.h"

RC NbSensors::SetFanPolicyAuto()
{
    RC rc;

    const UINT08 FAN_EC_DATA_SET_AUTO_FAN_CTRL  = 0x84;
    vector<UINT08> data{FAN_EC_DATA_SET_AUTO_FAN_CTRL};
    CHECK_RC(FanEcReadWrite(data));

    return rc;
}

RC NbSensors::SetFanPolicyManual()
{
    // Nothing needs to be done
    return RC::OK;
}

RC NbSensors::GetFanRpm(UINT32 fanIdx, UINT32 *pSpeed)
{
    RC rc;
    MASSERT(pSpeed);

    if (fanIdx >= m_NumFans)
        return RC::BAD_PARAMETER;

    const UINT08 FAN_EC_DATA_GET_FAN0_RPM = 0x81;
    UINT08 ret;
    vector<UINT08> data{static_cast<UINT08>(FAN_EC_DATA_GET_FAN0_RPM + fanIdx)};
    CHECK_RC(FanEcReadWrite(data, &ret));
    *pSpeed = ret * m_FanRpmAclwracy;

    return rc;
}

RC NbSensors::SetFanRpm(UINT32 fanIdx, UINT32 speed)
{
    RC rc;

    if (fanIdx >= m_NumFans)
        return RC::BAD_PARAMETER;

    const UINT08 FAN_EC_DATA_SET_FAN0_RPM   = 0x85;
    vector<UINT08> data{static_cast<UINT08>(FAN_EC_DATA_SET_FAN0_RPM + fanIdx),
                        static_cast<UINT08>(speed / m_FanRpmAclwracy)};
    CHECK_RC(FanEcReadWrite(data));

    return rc;
}

RC NbSensors::GetCpuVrTempC(INT32 *pTemp)
{
    RC rc;
    MASSERT(pTemp);

    UINT08 temp;
    const UINT08 TEMP_EC_CPU_VR_OFFSET  = 0xB7;
    CHECK_RC(TempEcRead(TEMP_EC_CPU_VR_OFFSET, &temp));
    *pTemp = static_cast<INT32>(temp);

    return rc;
}

RC NbSensors::GetGpuVrTempC(INT32 *pTemp)
{
    RC rc;
    MASSERT(pTemp);

    const UINT08 TEMP_EC_GPU_VR_OFFSET  = 0xB2;
    UINT08 temp;
    CHECK_RC(TempEcRead(TEMP_EC_GPU_VR_OFFSET, &temp));
    *pTemp = static_cast<INT32>(temp);

    return rc;
}

RC NbSensors::GetDdrTempC(INT32 *pTemp)
{
    RC rc;
    MASSERT(pTemp);

    const UINT08 TEMP_EC_DDR_OFFSET = 0xB6;
    UINT08 temp;
    CHECK_RC(TempEcRead(TEMP_EC_DDR_OFFSET, &temp));
    *pTemp = static_cast<INT32>(temp);

    return rc;
}

#define POLL_FAN_EC_COMMAND_STATUS(data, bitMask, val) do {           \
    CHECK_RC(Tasker::PollHw(m_EcTimeoutMs, [&]()->bool                \
    {                                                                 \
        if (Platform::PioRead08(FAN_EC_COMMAND_IO_PORT, &data) != OK) \
            return false;                                             \
        return ((data & bitMask) == val);                             \
    }, __FUNCTION__));                                                \
} while (0)

RC NbSensors::FanEcReadWrite(const vector<UINT08> &data, UINT08 *pRet)
{
    RC rc;
    UINT08 temp;

    const UINT32 FAN_EC_COMMAND_IO_PORT     = 0x6C;
    const UINT32 FAN_EC_DATA_IO_PORT        = 0x68;
    const UINT32 FAN_EC_COMMAND_STATUS_BUSY = 7;
    const UINT32 FAN_EC_COMMAND_STATUS_IBF  = 1;
    const UINT32 FAN_EC_COMMAND_STATUS_OBF  = 0;
    const UINT08 FAN_EC_COMMAND             = 0x46;
    const UINT08 FAN_EC_COMMAND_END         = 0xFF;

    const UINT08 busyMask = 1 << FAN_EC_COMMAND_STATUS_BUSY;
    const UINT08 ibfMask  = 1 << FAN_EC_COMMAND_STATUS_IBF;
    const UINT08 obfMask  = 1 << FAN_EC_COMMAND_STATUS_OBF;

    POLL_FAN_EC_COMMAND_STATUS(temp, busyMask, 0);                 // Wait until busy
    POLL_FAN_EC_COMMAND_STATUS(temp, ibfMask, 0);                  // Poll input buffer status
    CHECK_RC(Platform::PioWrite08(FAN_EC_COMMAND_IO_PORT,
                                  FAN_EC_COMMAND));                // Send ec command
    for (auto d : data)
    {
        POLL_FAN_EC_COMMAND_STATUS(temp, ibfMask, 0);              // Poll input buffer status
        CHECK_RC(Platform::PioWrite08(FAN_EC_DATA_IO_PORT, d));    // Send data to be read
    }
    if (pRet)
    {
        POLL_FAN_EC_COMMAND_STATUS(temp, obfMask, 1);              // Poll output buffer status
        CHECK_RC(Platform::PioRead08(FAN_EC_DATA_IO_PORT, pRet));  // Read return data
    }
    POLL_FAN_EC_COMMAND_STATUS(temp, ibfMask, 0);                  // Poll input buffer status
    CHECK_RC(Platform::PioWrite08(FAN_EC_COMMAND_IO_PORT,
                                  FAN_EC_COMMAND_END));            // Signal end

    return rc;
}

#define POLL_TEMP_EC_COMMAND_STATUS(data, bitMask, val) do {            \
    CHECK_RC(Tasker::PollHw(m_EcTimeoutMs, [&]()->bool                  \
    {                                                                   \
        if (Platform::PioRead08(TEMP_EC_COMMAND_IO_PORT, &data) != OK)  \
            return false;                                               \
        return ((data & bitMask) == val);                               \
    }, __FUNCTION__));                                                  \
} while (0)

RC NbSensors::TempEcRead(UINT08 Offset, UINT08 *pRet)
{
    RC rc;
    UINT08 temp;
    MASSERT(pRet);

    const UINT32 TEMP_EC_COMMAND_IO_PORT    = 0x66;
    const UINT32 TEMP_EC_DATA_IO_PORT       = 0x62;
    const UINT32 TEMP_EC_COMMAND_STATUS_IBF = 1;
    const UINT32 TEMP_EC_COMMAND_STATUS_OBF = 0;
    const UINT08 TEMP_EC_COMMAND_READ       = 0x80;

    const UINT08 ibfMask  = 1 << TEMP_EC_COMMAND_STATUS_IBF;
    const UINT08 obfMask  = 1 << TEMP_EC_COMMAND_STATUS_OBF;

    POLL_TEMP_EC_COMMAND_STATUS(temp, ibfMask, 0);                // Poll input buffer status
    CHECK_RC(Platform::PioWrite08(TEMP_EC_COMMAND_IO_PORT,
                                  TEMP_EC_COMMAND_READ));         // Send read command
    POLL_TEMP_EC_COMMAND_STATUS(temp, ibfMask, 0);                // Poll input buffer status
    CHECK_RC(Platform::PioWrite08(TEMP_EC_DATA_IO_PORT, Offset)); // Send ram offset
    POLL_TEMP_EC_COMMAND_STATUS(temp, obfMask, 1);                // Poll output buffer status
    CHECK_RC(Platform::PioRead08(TEMP_EC_DATA_IO_PORT, pRet));    // Read return data

    return rc;
}
