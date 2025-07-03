/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwswitchi2c.h"
#include "core/include/tasker.h"
#include "ctrl_dev_lwswitch.h"
#include "ctrl_dev_internal_lwswitch.h"
#include "gpu/include/testdevice.h"

namespace
{
    const UINT32 I2C_DELAY_MS = 10;
    const UINT32 I2C_RETRIES  = 10;
    const UINT32 I2C_POLL_RETRIES = 50;
};

//--------------------------------------------------------------------
//! \brief Print a table of data read from the I2C device
//!
//! This method calls Read() on all offsets from BeginOffset <= offset
//! < EndOffset, and pretty-prints the result in tabular form.
//!
RC LwSwitchI2c::DoDump(const Dev& dev, UINT32 beginOffset, UINT32 endOffset)
{
    RC rc;

    string header = "";
    UINT32 numColumns = 0;
    UINT32 columnWidth = 2 * dev.GetMessageLength();

    // Decide how many columns the table should have
    //
    switch (dev.GetMessageLength())
    {
        case 1:
            numColumns = 16;
            break;
        case 2:
        case 3:
            numColumns = 8;
            break;
        case 4:
            numColumns = 4;
            break;
        default:
            Printf(Tee::PriHigh, "Invalid I2C message length in Dump()\n");
            return RC::BAD_PARAMETER;
    }

    // Print the header row, highlighted, underlined
    //
    string hdr = Utility::StrPrintf("%*s ", columnWidth, "");
    for (UINT32 ii = 0; ii < numColumns; ++ii)
    {
        const char* spaces = (ii % 4) ? " " : "  ";
        hdr += Utility::StrPrintf("%s%0*x", spaces, columnWidth, ii);
    }
    Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(), Tee::SPS_HIGHLIGHT,
           "%s", hdr.c_str());
    Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(), Tee::SPS_NORMAL, "\n");
    for (UINT32 ii = 0; ii < hdr.size(); ++ii)
    {
        Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(), Tee::SPS_HIGHLIGHT,
               hdr[ii] == ' ' ? " " : "-");
    }
    Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(), Tee::SPS_NORMAL, "\n");

    // Print the data read from the I2C device
    //
    StickyRC firstReadRc;
    bool allReadsFailed = true;
    UINT32 beginRow = ALIGN_DOWN(beginOffset, numColumns);
    UINT32 endRow = ALIGN_UP(endOffset, numColumns);
    for (UINT32 rowOffset = beginRow; rowOffset < endRow;
         rowOffset += numColumns)
    {
        Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(), Tee::SPS_HIGHLIGHT,
               "%0*x:", columnWidth, rowOffset);
        for (UINT32 colOffset = rowOffset;
             colOffset < rowOffset + numColumns; ++colOffset)
        {
            string messageString = "";
            if ((colOffset >= beginOffset) && (colOffset < endOffset))
            {
                UINT32 message = 0;
                RC readRc = DoRead(dev, colOffset, &message);
                if (readRc == OK)
                {
                    messageString =
                        Utility::StrPrintf("%0*x", columnWidth, message);
                    allReadsFailed = false;
                }
                else
                {
                    messageString = string(columnWidth, '*');
                    firstReadRc = readRc;
                }
            }
            else
            {
                messageString = string(columnWidth, ' ');
            }
            Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(), Tee::SPS_NORMAL, " %s%s",
                   ((colOffset % 4) ? "" : " "), messageString.c_str());
        }
        Printf(Tee::PriNormal, Tee::GetTeeModuleCoreCode(), Tee::SPS_NORMAL, "\n");
    }

    if (allReadsFailed)
    {
        CHECK_RC(firstReadRc);
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LwSwitchI2c::DoGetI2cDcbDevInfo(I2cDcbDevInfo* pInfo)
{
    MASSERT(pInfo);

    RC rc;

    LWSWITCH_CTRL_I2C_GET_DEV_INFO_PARAMS params = {};
    CHECK_RC(GetDevice()->GetLibIf()->Control(m_LibHandle,
                                              LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_I2C_TABLE_GET_DEV_INFO,
                                              &params,
                                              sizeof(params)));

    pInfo->resize(static_cast<UINT32>(params.i2cDevCount));

    for (UINT32 dev = 0; dev < pInfo->size(); dev++)
    {
        I2cDcbDevInfoEntry& entry = pInfo->at(dev);
        LWSWITCH_CTRL_I2C_DEVICE_INFO driverInfo = params.i2cDevInfo[dev];
        entry.Type           = driverInfo.type;
        entry.I2CAddress     = driverInfo.i2cAddress;
        entry.I2CLogicalPort = driverInfo.i2cPortLogical;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Get info about the I2C ports.
//!
//! Query the RM for info about the I2C ports on this Gpu.
//!
//! \param pPortInfo On exit, contains an array of info about the
//!     I2C ports.  Element 0 describes port 0, etc.
//
RC LwSwitchI2c::DoGetPortInfo(PortInfo *pPortInfo)
{
    MASSERT(pPortInfo);

    RC rc;

    LWSWITCH_CTRL_I2C_GET_PORT_INFO_PARAMS params = {};
    CHECK_RC(GetDevice()->GetLibIf()->Control(m_LibHandle,
                                              LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_I2C_GET_PORT_INFO,
                                              &params,
                                              sizeof(params)));

    pPortInfo->resize(LWSWITCH_CTRL_NUM_I2C_PORTS);
    for (UINT32 port = 0; port < pPortInfo->size(); port++)
    {
        PortInfoEntry& entry = pPortInfo->at(port);
        LwU8 driverInfo = params.info[port];
        entry.Implemented = FLD_TEST_DRF(SWITCH_CTRL, _I2C_GET_PORT_INFO,
                                         _IMPLEMENTED, _YES, driverInfo);
        // Only IMPLEMENTED is implemented for LwSwitch
        entry.DcbDeclared = false;
        entry.DdcChannel  = false;
        entry.CrtcMapped  = false;
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LwSwitchI2c::DoInitialize(LwLinkDevIf::LibInterface::LibDeviceHandle libHandle)
{
    m_LibHandle = libHandle;
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Helps to ping a I2C device and confirm it's availability.
//!
RC LwSwitchI2c::DoPing(const Dev& dev)
{
    RC rc;

    LWSWITCH_CTRL_I2C_INDEXED_PARAMS params = {};
    params.port     = dev.GetPort();
    params.bIsRead  = false;
    params.address  = dev.GetAddr();
    CHECK_RC(GetFlags(dev, true, false, &params.flags));
    params.flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _TRANSACTION_MODE, _PING, params.flags);
    params.flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _START,            _SEND, params.flags);
    params.flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _RESTART,          _NONE, params.flags);
    params.flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _STOP,             _SEND, params.flags);
    params.flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _INDEX_LENGTH,     _ZERO, params.flags);
    params.messageLength = 0;

    UINT32 retries = 0;
    rc = GetDevice()->GetLibIf()->Control(m_LibHandle,
                                          LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_I2C_INDEXED,
                                          &params,
                                          sizeof(params));
    while ((rc == RC::LWRM_IN_USE) && (retries++ < I2C_RETRIES))
    {
        Tasker::Sleep(I2C_DELAY_MS);
        rc.Clear();
        rc = GetDevice()->GetLibIf()->Control(m_LibHandle,
                                              LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_I2C_INDEXED,
                                              &params,
                                              sizeof(params));
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LwSwitchI2c::DoRead(const Dev& dev, UINT32 offset, UINT32* pData)
{
    RC rc;

    vector<UINT08> message(dev.GetMessageLength(), 0);

    CHECK_RC(DoWriteOrRead(dev, false, false, offset, &message));

    dev.ArrayToInt(message.data(), pData, dev.GetMessageLength());

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LwSwitchI2c::DoReadArray(const Dev& dev, UINT32 offset, UINT32 numReads, vector<UINT32>* pData)
{
    RC rc;
    MASSERT(pData);

    pData->clear();
    for (UINT32 i = 0; i < numReads; i++)
    {
        UINT32 wordData = 0;
        CHECK_RC(DoRead(dev, offset + i*dev.GetOffsetLength(), &wordData));
        pData->push_back(wordData);

        FLOAT64 period = 1.0 / static_cast<FLOAT64>(dev.GetSpeedInKHz());
        Tasker::Sleep(period * 2.0);
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC LwSwitchI2c::DoReadBlk(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>* pData)
{
    MASSERT(pData);
    RC rc;

    pData->clear();

    vector<UINT08> message(numBytes, 0);

    CHECK_RC(DoWriteOrRead(dev, false, true, offset, &message));

    pData->resize(numBytes);
    memcpy(pData->data(), message.data(), numBytes);

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC LwSwitchI2c::DoReadSequence(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>* pData)
{
    MASSERT(pData);
    RC rc;

    pData->clear();

    vector<UINT08> message(numBytes, 0);
    CHECK_RC(DoWriteOrRead(dev, false, false, offset, &message));

    pData->resize(numBytes);
    memcpy(pData->data(), message.data(), numBytes);

    return rc;
}

//-----------------------------------------------------------------------------
RC LwSwitchI2c::DoWrite(const Dev& dev, UINT32 offset, UINT32 data)
{
    vector<UINT08> message(dev.GetMessageLength());
    dev.IntToArray(data, message.data(), dev.GetMessageLength());

    return DoWriteOrRead(dev, true, false, offset, &message);
}

//-----------------------------------------------------------------------------
RC LwSwitchI2c::DoWriteArray(const Dev& dev, UINT32 offset, const vector<UINT32>& data)
{
    RC rc;

    for (UINT32 i = 0; i < data.size(); i++)
    {
        UINT32 wordData = data[i];
        CHECK_RC(DoWrite(dev, offset + i*dev.GetOffsetLength(), wordData));

        FLOAT64 period = 1.0 / static_cast<FLOAT64>(dev.GetSpeedInKHz());
        Tasker::Sleep(period * 2.0);
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC LwSwitchI2c::DoWriteBlk(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>& data)
{
    if (data.size() > numBytes)
    {
        data.resize(numBytes);
    }
    else if (data.size() < numBytes)
    {
        Printf(Tee::PriError, "WriteBlk numBytes greater than data size\n");
        return RC::ILWALID_ARGUMENT;
    }

    return DoWriteOrRead(dev, true, true, offset, &data);
}

//-----------------------------------------------------------------------------
RC LwSwitchI2c::DoWriteSequence(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>& data)
{
    if (data.size() > numBytes)
    {
        data.resize(numBytes);
    }
    else if (data.size() < numBytes)
    {
        Printf(Tee::PriError, "WriteSequence numBytes greater than data size\n");
        return RC::ILWALID_ARGUMENT;
    }
    return DoWriteOrRead(dev, true, false, offset, &data);
}

//-----------------------------------------------------------------------------
//! \brief Common code for Write() and Read() methods
//!
RC LwSwitchI2c::DoWriteOrRead
(
    const Dev& dev,
    bool bIsWrite,
    bool bIsBlk,
    UINT32 offset,
    vector<UINT08>* pData
)
{
    MASSERT(pData);

    RC rc;

    LWSWITCH_CTRL_I2C_INDEXED_PARAMS params = {};

    params.port     = dev.GetPort();
    params.bIsRead  = !bIsWrite;
    params.address  = dev.GetAddr();
    CHECK_RC(GetFlags(dev, bIsWrite, bIsBlk, &params.flags));

    if ((bIsWrite && dev.GetWriteCmd()) || (!bIsWrite && dev.GetReadCmd()))
    {
        params.index[0] = bIsWrite ? dev.GetWriteCmd() : dev.GetReadCmd();
        dev.IntToArray(offset, &params.index[1], dev.GetOffsetLength());
    }
    else
    {
        dev.IntToArray(offset, &params.index[0], dev.GetOffsetLength());
    }

    if (!bIsWrite && dev.GetVerifyReads())
    {
        pData->resize(2 * pData->size());
        fill(pData->begin(), pData->end(), 0);
    }

    params.messageLength = static_cast<UINT32>(pData->size());
    memcpy(&params.message[0], pData->data(), params.messageLength);

    UINT32 retries = 0;
    rc = GetDevice()->GetLibIf()->Control(m_LibHandle,
                                          LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_I2C_INDEXED,
                                          &params,
                                          sizeof(params));
    while ((rc == RC::LWRM_IN_USE) && (retries++ < I2C_RETRIES))
    {
        Tasker::Sleep(I2C_DELAY_MS);
        rc.Clear();
        rc = GetDevice()->GetLibIf()->Control(m_LibHandle,
                                              LwLinkDevIf::LibInterface::CONTROL_LWSWITCH_I2C_INDEXED,
                                              &params,
                                              sizeof(params));
    }
    CHECK_RC(rc);

    if (bIsWrite)
    {
        UINT32 pollCount = 0;
        do
        {
            Tasker::Sleep(I2C_DELAY_MS);
            rc.Clear();
            rc = DoPing(dev);
        } while ((rc != OK) && (pollCount++ < I2C_POLL_RETRIES));

        if (rc != OK)
        {
            Printf(Tee::PriError, "I2C Write Timed Out!\n");
            return RC::TIMEOUT_ERROR;
        }
    }
    else
    {
        memcpy(pData->data(), &params.message[0], params.messageLength);
    }

    if (!bIsWrite && dev.GetVerifyReads())
    {
        if (memcmp(&(*pData)[0], &(*pData)[pData->size()/2], pData->size()/2))
        {
            Printf(Tee::PriError, "I2C Read Verify failed!\n");
            return RC::GENERIC_I2C_ERROR;
        }
        else
        {
            pData->resize(pData->size() / 2);
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LwSwitchI2c::GetFlags(const Dev& dev, bool bIsWrite, bool bIsBlk, UINT32* pFlags) const
{
    MASSERT(pFlags);
    *pFlags = 0;
    UINT32 flags = 0;

    flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _START, _SEND, flags);
    flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _STOP, _SEND, flags);
    if (!bIsWrite)
    {
        flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _RESTART, _SEND, flags);
    }

    if (bIsBlk)
    {
        flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _BLOCK_PROTOCOL, _ENABLED, flags);
    }

    switch (dev.GetFlavor())
    {
        case FLAVOR_HW:
            flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _FLAVOR, _HW, flags);
            break;
        case FLAVOR_SW:
            flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _FLAVOR, _SW, flags);
            break;
        default:
            Printf(Tee::PriError, "Invalid I2C Flavor = %d\n", dev.GetFlavor());
            return RC::BAD_PARAMETER;
    }

    switch (dev.GetSpeedInKHz())
    {
        case SPEED_100KHz:
            flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _SPEED_MODE, _100KHZ, flags);
            break;
        case SPEED_200KHz:
            flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _SPEED_MODE, _200KHZ, flags);
            break;
        case SPEED_300KHz:
            flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _SPEED_MODE, _300KHZ, flags);
            break;
        case SPEED_400KHz:
            flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _SPEED_MODE, _400KHZ, flags);
            break;
        case SPEED_1000KHz:
            flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _SPEED_MODE, _1000KHZ, flags);
            break;
        default:
            Printf(Tee::PriError, "Invalid I2C Speed = %d\n", dev.GetSpeedInKHz());
            return RC::BAD_PARAMETER;
    }

    switch (dev.GetAddrMode())
    {
        case ADDRMODE_7BIT:
            flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _ADDRESS_MODE, _7BIT, flags);
            break;
        case ADDRMODE_10BIT:
            flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _ADDRESS_MODE, _10BIT, flags);
            break;
        default:
            Printf(Tee::PriError, "Invalid I2C address mode = %d\n", dev.GetAddrMode());
            return RC::BAD_PARAMETER;
    }

    UINT32 indexLength = dev.GetOffsetLength();
    if ((bIsWrite && dev.GetWriteCmd()) || (!bIsWrite && dev.GetReadCmd()))
        indexLength++;
    switch (indexLength)
    {
        case 0:
            flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _INDEX_LENGTH, _ZERO, flags);
            break;
        case 1:
            flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _INDEX_LENGTH, _ONE, flags);
            break;
        case 2:
            flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _INDEX_LENGTH, _TWO, flags);
            break;
        case 3:
            flags = FLD_SET_DRF(SWITCH_CTRL, _I2C_FLAGS, _INDEX_LENGTH, _THREE, flags);
            break;
        default:
            Printf(Tee::PriError, "Invalid I2C offset length = %d\n", dev.GetOffsetLength());
            return RC::BAD_PARAMETER;
    }

    *pFlags = flags;

    return OK;
}
