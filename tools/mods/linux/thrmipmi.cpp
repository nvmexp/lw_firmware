/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <asm/errno.h>
#include <linux/ipmi.h>
#include <errno.h>

#include "gpu/perf/thermsub.h"
#include "core/include/xp.h"

//! Defines for sending and retrieving IPMI sensor commands
#define IPMI_SENSOR_CMD_REQ     4
#define IPMI_SENSOR_READ_CMD    0x2d
#define IPMI_TEMP_MSG_IDX       1    //!< Index in IPMI sensor message for the
                                     //!< temperature reading

//-----------------------------------------------------------------------------
//! \brief Set the offset to apply to the returned IPMI temperature to get the
//!        actual temperature value (set via the command line based on a
//!        calibration procedure performed by the board solutions team)
//!
//! \param Offset : Offset in degrees C to apply to the temperature
//!
//! \return OK if successful, not OK otherwise
RC Thermal::SetIpmiTempOffset(INT32 Offset)
{
    m_IpmiOffset = Offset;
    m_IpmiOffsetSet = true;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Return the offset to apply to the IPMI temperature
//!
//! \param pOffset : Pointer to returned offset in degrees C to apply to the
//!                  temperature
//!
//! \return OK if successful, not OK otherwise
RC Thermal::GetIpmiTempOffset(INT32 *pOffset)
{
    MASSERT(pOffset);
    *pOffset = m_IpmiOffset;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Set the IPMI sensor to read
//!
//! \param Sensor : IPMI sensor to read
//!
//! \return OK if successful, not OK otherwise
RC Thermal::SetIpmiTempSensor(INT32 Sensor)
{
    m_IpmiSensor = Sensor;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Return the IPMI sensor being read
//!
//! \param pSensor : Pointer to returned sensor being read
//!
//! \return OK if successful, not OK otherwise
RC Thermal::GetIpmiTempSensor(INT32 *pSensor)
{
    MASSERT(pSensor);
    *pSensor = m_IpmiSensor;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Set the IPMI temperature device
//!
//! \param Name : Name of the device to set
//!
//! \return OK if successful, not OK otherwise
RC Thermal::SetIpmiTempDevice(string Name)
{
    m_IpmiDevName = Name;
    if (Name == "HS22")
    {
        // TODO : automatically determine IPMI temperature offset
        m_IpmiMeasurementOffset = HS22_ADT7473_TEMP_MEAS_OFFSET;
        return OK;
    }
    else
    {
        Printf(Tee::PriHigh,"%s device not found\n", Name.c_str());
        return RC::DEVICE_NOT_FOUND;
    }
}

//-----------------------------------------------------------------------------
//! \brief Get the IPMI temperature device
//!
//! \param pName : Pointer to returned name of the device in use
//!
//! \return OK if successful, not OK otherwise
RC Thermal::GetIpmiTempDevice(string *pName)
{
    MASSERT(pName);
    *pName = m_IpmiDevName;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Get the IPMI temperature
//!
//! NOTE : The behavior of the current supported blade server (IBM HS22) is
//! that the real temperature is cached and it only updates the temperature
//! every 12-15s (determined empirically)
//!
//! \param pTemp : Pointer to returned temperature
//!
//! \return OK if successful, not OK otherwise
RC Thermal::GetChipTempViaIpmi(INT32 *pTemp)
{
    RC rc = OK;
    vector<UINT08> requestData(1, 0);
    vector<UINT08> responseData(16, 0);

    MASSERT(pTemp);

    requestData[0] = m_IpmiSensor;
    rc = m_IpmiDevice.SendToBMC(IPMI_SENSOR_CMD_REQ, IPMI_SENSOR_READ_CMD,
                                requestData, responseData);

    *pTemp = (INT32)responseData[IPMI_TEMP_MSG_IDX] + m_IpmiOffset +
                    m_IpmiMeasurementOffset;
    CHECK_RC(m_OverTempCounter.Update(OverTempCounter::IPMI_TEMP, *pTemp));

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Initialize the IPMI device
//!
//! \return OK if successful, not OK otherwise
RC Thermal::InitializeIpmiDevice()
{

    if ((m_IpmiDevName == "") || (m_IpmiSensor == -1))
    {
        Printf(Tee::PriHigh,"Ipmi device name or sensor not set\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }
    return m_IpmiDevice.Initialize();
}
