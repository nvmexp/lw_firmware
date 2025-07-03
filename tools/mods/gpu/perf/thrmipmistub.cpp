/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/perf/thermsub.h"

//! \file    thrmipmistub.cpp
//! \brief   IPMI thermal stub functions (IPMI temperature reading is only
//!          supported on Linux (the linux implementation lives at
//!          mods/linux/thrmipmi.cpp)

RC Thermal::SetIpmiTempOffset(INT32 Offset)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Thermal::GetIpmiTempOffset(INT32 *pOffset)
{
    MASSERT(pOffset);
    *pOffset = 0;
    return RC::UNSUPPORTED_FUNCTION;
}

RC Thermal::SetIpmiTempSensor(INT32 Sensor)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Thermal::GetIpmiTempSensor(INT32 *pSensor)
{
    MASSERT(pSensor);
    *pSensor = 0;
    return RC::UNSUPPORTED_FUNCTION;
}

RC Thermal::SetIpmiTempDevice(string Name)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Thermal::GetIpmiTempDevice(string *pName)
{
    MASSERT(pName);
    *pName = "";
    return RC::UNSUPPORTED_FUNCTION;
}

RC Thermal::GetChipTempViaIpmi(INT32 *pTemp)
{
    MASSERT(pTemp);
    *pTemp = 0;
    return RC::UNSUPPORTED_FUNCTION;
}

RC Thermal::InitializeIpmiDevice()
{
    return RC::UNSUPPORTED_FUNCTION;
}
