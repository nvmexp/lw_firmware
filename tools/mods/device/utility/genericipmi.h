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

/**
 * @file  genericipmi.h
 * @brief Generic object for communicating over IPMI
 */

#pragma once
#ifndef INCLUDED_GENERICIPMI_H
#define INCLUDED_GENERICIPMI_H

#include "core/include/xp.h"

class IpmiDevice
{
    public:
        IpmiDevice();
        ~IpmiDevice();
        RC Initialize();
        RC SendToBMC(UINT08 netfn, UINT08 cmd, vector<UINT08>& requestData,
                     vector<UINT08>& responseData, UINT32 *pRspDataSize = nullptr);
        RC WriteSMBPBI(UINT08 boardAddr, UINT08 reg, vector<UINT08>& regData);
        RC ReadSMBPBI(UINT08 boardAddr, UINT08 reg, vector<UINT08>& regData);
        RC RawAccess(UINT08 netfn, UINT08 cmd, vector<UINT08>& sendData, vector<UINT08>& recvData);
#ifdef LINUX_MFG
    private:
        INT32 m_IpmiFd;
#endif
};

#endif // INCLUDED_GENERICIPMI_H
