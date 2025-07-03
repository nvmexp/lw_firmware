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

#include "gpu/tempctrl/hwaccess/ipmitempctrl.h"
#include "core/include/xp.h"

//-----------------------------------------------------------------------------
//! \brief Get the IPMI value
//!
//! \return OK if successful, not OK otherwise
RC IpmiTempCtrl::GetTempCtrlValViaIpmi
(
    vector<UINT08> &requestData,
    vector<UINT08> &responseData
)
{
    RC rc;

    requestData[0] = m_BusId;
    requestData[1] = m_SlaveAddr;
    requestData[2] = m_GetByteCount + 1;
    requestData[3] = m_GetCmdCode;
    rc = m_IpmiDevice.SendToBMC(m_NetFn, m_MasterCmd,
                                requestData, responseData);
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Set the IPMI value
//!
//! \return OK if successful, not OK otherwise
RC IpmiTempCtrl::SetTempCtrlValViaIpmi
(
    vector<UINT08> &requestData,
    vector<UINT08> &responseData,
    vector<UINT08> &writeData
)
{
    RC rc;

    requestData[0] = m_BusId;
    requestData[1] = m_SlaveAddr;
    requestData[2] = 1; // For RC
    requestData[3] = m_SetCmdCode;
    requestData[4] = m_SetByteCount;
    for (UINT32 i = 0; i < m_SetByteCount; i++)
    {
        requestData[i + 5] = writeData[i];
    }

    rc = m_IpmiDevice.SendToBMC(m_NetFn, m_MasterCmd,
                                requestData, responseData);

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Initialize the IPMI device
//!
//! \return OK if successful, not OK otherwise
RC IpmiTempCtrl::InitIpmiDevice()
{
    return m_IpmiDevice.Initialize();
}
