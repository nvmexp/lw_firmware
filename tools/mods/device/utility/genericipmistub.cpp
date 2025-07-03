/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "genericipmi.h"

//! \file  genericipmistub.cpp
//! \brief IPMI is only supported on Linux (the linux implementation lives at
//!        mods/linux/genericipmi.cpp

IpmiDevice::IpmiDevice() {}
IpmiDevice::~IpmiDevice() {}

RC IpmiDevice::Initialize()
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC IpmiDevice::SendToBMC(UINT08 netfn, UINT08 cmd, vector<UINT08>& requestData,
                         vector<UINT08>& responseData, UINT32 *pResponseDataSize)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC IpmiDevice::WriteSMBPBI(UINT08 boardAddr, UINT08 reg, vector<UINT08>& regData)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC IpmiDevice::ReadSMBPBI(UINT08 boardAddr, UINT08 reg, vector<UINT08>& regData)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC IpmiDevice::RawAccess
(
    UINT08 netfn,
    UINT08 cmd,
    vector<UINT08>& sendData,
    vector<UINT08>& recvData
)
{
    return RC::UNSUPPORTED_FUNCTION;
}
