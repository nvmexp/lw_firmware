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

#include "device/utility/genericipmi.h"
#include "core/include/xp.h"
#include "core/include/tasker.h"

namespace
{
    //! Values for retrieving IPMI sensor commands
    const UINT08  IPMI_NETFN_APP_REQ  = 0x06;
    const UINT08  IPMI_CMD_MASTER_RW  = 0x52;

    //! Polling data structure used to wait for IPMI response data
    struct ReadyPollData
    {
        INT32  ipmiFd;   // Open file descriptor to the IPMI device
        INT32  errnum;   // errno returned if the select call fails
    };

    //! Polling function for determining if the IPMI device has read
    //! data ready
    bool PollIpmiReady(void *pvArgs)
    {
        ReadyPollData *pIpmiData = static_cast<ReadyPollData *>(pvArgs);
        struct timeval waitTime;
        INT32 retval;
        fd_set readFds;

        FD_ZERO(&readFds);
        FD_SET(pIpmiData->ipmiFd, &readFds);

        // Set the timeval structure to 0, this causes select to immediately
        // return if the data is not ready (a blocking select call for a
        // temperature read empirically takes ~5ms)
        memset(&waitTime, 0, sizeof(struct timeval));

        retval = select(pIpmiData->ipmiFd + 1, &readFds, NULL, NULL, &waitTime);

        // Done polling if retval is non-zero.
        //    < 0 indicates that select failed
        //    > 0 indicates that the Ipmi file descriptor is ready for reading
        if (retval != 0)
        {
            pIpmiData->errnum = (retval < 0) ? errno : 0;
            return true;
        }
        return false;
    }
}

IpmiDevice::IpmiDevice()
: m_IpmiFd(-1) {}

IpmiDevice::~IpmiDevice()
{
    if (m_IpmiFd != -1)
    {
        close(m_IpmiFd);
        m_IpmiFd = -1;
    }
}

//-----------------------------------------------------------------------------
//! \brief Initialize the IPMI device
//!
//! \return OK if successful, not OK otherwise
RC IpmiDevice::Initialize()
{
    RC rc = OK;
    const char* const IpmiDev = "/dev/ipmi0";

    if (m_IpmiFd != -1)
    {
        Printf(Tee::PriHigh,"Ipmi already initialized\n");
        return rc;
    }

    // Check if ipmi device exists
    struct stat buf;
    if (0 != stat(IpmiDev, &buf))
    {
        Printf(Tee::PriError, "%s does not exist.\n", IpmiDev);
        return RC::FILE_DOES_NOT_EXIST;
    }

    // Check access permissions
    if (0 != access(IpmiDev, R_OK | W_OK))
    {
        Printf(Tee::PriError, "Insufficient access rights for %s.\n",
               IpmiDev);
        return RC::FILE_PERM;
    }

    // Open ipmi device
    m_IpmiFd = open(IpmiDev, O_RDWR);
    if (m_IpmiFd == -1)
    {
        Printf(Tee::PriError,
               "Unable to open %s for IPMI access.\n",
                IpmiDev);
        return RC::FILE_UNKNOWN_ERROR;
    }

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Send a message to the IPMI Baseboard Management Controller (BMC)
//!
//! \param netfn        : IPMI netfn
//! \param cmd          : IPMI cmd
//! \param requestData  : Bytes to send to the BMC
//! \param responseData : Bytes to store the BMC response, the IPMI
//!                       completion code is always returned as the first byte
//!
//! \return OK if successful, not OK otherwise
RC IpmiDevice::SendToBMC(UINT08 netfn, UINT08 cmd, vector<UINT08>& requestData,
                         vector<UINT08>& responseData, UINT32 *pResponseDataSize)
{

    RC rc = OK;
    struct ipmi_req  request;
    struct ipmi_recv response;
    struct ipmi_addr address;
    struct ipmi_system_interface_addr systemAddress;

    if (m_IpmiFd == -1)
    {
        CHECK_RC(Initialize());
    }

    systemAddress.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
    systemAddress.channel   = IPMI_BMC_CHANNEL;

    request.addr         = (unsigned char *)&systemAddress;
    request.addr_len     = sizeof(systemAddress);
    request.msgid        = 0;
    request.msg.netfn    = netfn;
    request.msg.cmd      = cmd;

    request.msg.data     = requestData.data();
    request.msg.data_len = requestData.size();

    // Send command to the IPMI device
    if (ioctl(m_IpmiFd, IPMICTL_SEND_COMMAND, &request) < 0)
    {
        Printf(Tee::PriError,
               "IpmiDevice::SendToBMC : IPMI send error - %s",
               strerror(errno));
        return RC::SOFTWARE_ERROR;
    }

    // Wait for the response data to be ready
    ReadyPollData pollData = { 0 };
    pollData.ipmiFd = m_IpmiFd;
    CHECK_RC(POLLWRAP(PollIpmiReady, &pollData, Tasker::GetDefaultTimeoutMs()));
    if (pollData.errnum)
    {
        Printf(Tee::PriError,
               "IpmiDevice::SendToBMC : select error - %s",
               strerror(pollData.errnum));
        return RC::SOFTWARE_ERROR;
    }

    // Retrieve the response data
    response.addr         = (unsigned char *)&address;
    response.addr_len     = sizeof(address);
    response.msg.data     = responseData.data();
    response.msg.data_len = responseData.size();
    if (ioctl(m_IpmiFd, IPMICTL_RECEIVE_MSG_TRUNC, &response) < 0)
    {
        Printf(Tee::PriError,
               "IpmiDevice::SendToBMC : IPMI receive error - %s",
               strerror(errno));
        return RC::SOFTWARE_ERROR;
    }

    // The ioctl will change the data_len based on tha actual amount of valid data
    // that was received
    if (pResponseDataSize)
        *pResponseDataSize = static_cast<UINT32>(response.msg.data_len);

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Write to a register of the SMBus Post Box Interface (SMBPBI)
//!
//! \param boardAddr    : SMBus slave address of GPU
//! \param reg          : SMBus register (command) to write to
//! \param regData      : Vector containing the 4 bytes to write
//!
//! \return OK if successful, not OK otherwise
RC IpmiDevice::WriteSMBPBI(UINT08 boardAddr, UINT08 reg, vector<UINT08>& regData)
{
    MASSERT(regData.size() >= 4);
    RC rc = OK;

    vector<UINT08> requestData(9, 0);
    // IPMI completion code is always returned, so we need to allocate space
    vector<UINT08> responseData(1, 0);

    requestData[0] = 0x03;           // bus ID (0x03 is private bus #1)
    requestData[1] = boardAddr << 1; // [7:1] slave address
                                     //   [0] reserved (write as zero)
    requestData[2] = 0x00;           // bytes to read
    requestData[3] = reg;            // command (register)
    requestData[4] = 0x04;           // bytes to write
    requestData[5] = regData[0];
    requestData[6] = regData[1];
    requestData[7] = regData[2];
    requestData[8] = regData[3];

    CHECK_RC(SendToBMC(IPMI_NETFN_APP_REQ, IPMI_CMD_MASTER_RW,
                       requestData, responseData));
    return rc;
}

//! \brief Read to a register of the SMBus Post Box Interface (SMBPBI)
//!
//! \param boardAddr    : SMBus slave address of GPU
//! \param reg          : SMBus register (command) to read from
//! \param regData      : Vector containing 4 bytes to read to
//!
//! \return OK if successful, not OK otherwise
RC IpmiDevice::ReadSMBPBI(UINT08 boardAddr, UINT08 reg, vector<UINT08>& regData)
{
    MASSERT(regData.size() >= 4);
    RC rc = OK;

    vector<UINT08> requestData(4, 0);
    vector<UINT08> responseData(6, 0);

    requestData[0] = 0x03;           // bus ID (0x03 is private bus #1)
    requestData[1] = boardAddr << 1; // [7:1] slave address
                                     //   [0] reserved (write as zero)
    requestData[2] = 0x05;           // bytes to read
    requestData[3] = reg;            // command (register)

    CHECK_RC(SendToBMC(IPMI_NETFN_APP_REQ, IPMI_CMD_MASTER_RW,
                       requestData, responseData));

    // IPMI completion code   responseData[0]
    // Byte Count             responseData[1]
    regData[0]              = responseData[2];
    regData[1]              = responseData[3];
    regData[2]              = responseData[4];
    regData[3]              = responseData[5];

    return rc;
}


//-----------------------------------------------------------------------------
//! \brief Perform a raw ipmi access (similar to a very basic "ipmitool raw" command
RC IpmiDevice::RawAccess
(
    UINT08 netfn,
    UINT08 cmd,
    vector<UINT08>& sendData,
    vector<UINT08>& recvData
)
{
    RC rc;

    // with a raw request the response data can be up to 1k 
    recvData.resize(1024, 0);
    UINT32 recvDataSize = 0;
    CHECK_RC(SendToBMC(netfn, cmd, sendData, recvData, &recvDataSize));
    recvData.resize(recvDataSize);
    return rc;
}
