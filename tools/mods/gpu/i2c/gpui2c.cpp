/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpui2c.h"
#include "Lwcm.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "class/cl402c.h" // LW40_I2C
#include "ctrl/ctrl402c.h"

namespace
{
    // The PMU also generates I2C transactions that can prevent MODS from
    // acquiring the I2C bus.  Since MODS tests rely on I2C failures indicating
    // a real HW failure rather than a "busy" condition, it is necessary
    // to retry when acquiring the I2C bus
    const UINT32 I2C_RETRY_DELAY_MS = 5;
    const UINT32 I2C_RETRIES        = 10;

    //----------------------------------------------------------------
    //! \brief RAII class for allocating a temporary LW40_I2C handle
    //!
    //! This class allocates a temporary LW40_I2C handle if needed,
    //! and frees it in the destructor.
    //!
    //! Ideally, we'd just allocate a permanent handle, but only one
    //! handle can be allocated per GpuSubdevice at a time, and we
    //! have to cooperate with rmtests and such that may allocate
    //! their own handles.
    //!
    class TmpI2cHandle
    {
    public:
        TmpI2cHandle() = default;
        ~TmpI2cHandle() { if (m_HandleOwner) LwRmPtr()->Free(m_Handle); }
        RC Initialize(GpuSubdevice *pGpuSubdevice, LwRm::Handle I2cHandle = 0);
        operator LwRm::Handle() const { return m_Handle; }
    private:
        LwRm::Handle m_Handle = 0;
        bool         m_HandleOwner = false;
    };

    //----------------------------------------------------------------
    //! \brief Allocate a temporary LW40_I2C handle, if needed.
    //!
    //! This method either allocates a temporary handle, or re-uses an
    //! existing one, depending on the I2cHandle parameter.  If
    //! I2cHandle is zero, then allocate a temporary handle and free
    //! it in the destructor.  If I2cHandle is nonzero, then use that
    //! handle and do not free it.
    //!
    RC TmpI2cHandle::Initialize
    (
        GpuSubdevice *pGpuSubdevice,
        LwRm::Handle I2cHandle
    )
    {
        LwRmPtr pLwRm;
        RC rc;

        MASSERT(m_Handle == 0);
        if (m_Handle != 0)
            return RC::SOFTWARE_ERROR;

        if (I2cHandle == 0)
        {
            CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(pGpuSubdevice),
                                  &m_Handle, LW40_I2C, NULL));
            m_HandleOwner = true;
        }
        else
        {
            m_Handle = I2cHandle;
        }
        return rc;
    }
}

//--------------------------------------------------------------------
//! \brief Common code for BlkWrite() and BlkRead() methods
//!
RC GpuI2c::DoBlkWriteOrRead
(
    const Dev& dev,
    I2cReadWrite RdWr,
    UINT32 Offset,
    UINT32 NumBytes,
    vector<UINT08> *pData,
    LwRm::Handle I2cHandle
)
{
    MASSERT(pData);
    RC rc;
    LwRmPtr pLwRm;

    // Load the RmControl param data struct
    LW402C_CTRL_I2C_TRANSACTION_PARAMS param = {};
    param.portId                               = dev.GetPort();
    param.deviceAddress                        = dev.GetAddr();
    param.flags                                = GetFlags(dev);
    param.transType                            = LW402C_CTRL_I2C_TRANSACTION_TYPE_SMBUS_BLOCK_RW;
    param.transData.smbusBlockData.bWrite          = (RdWr == I2cReadWrite::WRITE);
    param.transData.smbusBlockData.registerAddress = Offset;
    param.transData.smbusBlockData.messageLength   = NumBytes;

    MASSERT(pData->size() >= NumBytes);
    if (RdWr == I2cReadWrite::READ)
    {
        fill(pData->begin(), pData->end(), 0);
    }
    param.transData.smbusBlockData.pMessage = LW_PTR_TO_LwP64(&((*pData)[0]));

    // Call the RM.  Alloc a temporary I2cHandle if needed.
    //
    TmpI2cHandle tmpHandle;
    CHECK_RC(tmpHandle.Initialize(GetGpuSubdevice(), I2cHandle));
    UINT32 retries = 0;
    rc = pLwRm->Control(tmpHandle, LW402C_CTRL_CMD_I2C_TRANSACTION,
                        &param, sizeof(param));
    while ((rc == RC::LWRM_IN_USE) && (retries++ < I2C_RETRIES))
    {
        Tasker::Sleep(I2C_RETRY_DELAY_MS);
        rc.Clear();
        rc = pLwRm->Control(tmpHandle, LW402C_CTRL_CMD_I2C_TRANSACTION,
                            &param, sizeof(param));
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Print a table of data read from the I2C device
//!
//! This method calls Read() on all offsets from BeginOffset <= offset
//! < EndOffset, and pretty-prints the result in tabular form.
//!
RC GpuI2c::DoDump(const Dev& dev, UINT32 beginOffset, UINT32 endOffset)
{
    RC rc;

    UINT32 numColumns = 0;
    UINT32 columnWidth = 2 * dev.GetMessageLength();

    TmpI2cHandle tmpHandle;
    CHECK_RC(tmpHandle.Initialize(GetGpuSubdevice()));

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
        const char *Spaces = (ii % 4) ? " " : "  ";
        hdr += Utility::StrPrintf("%s%0*x", Spaces, columnWidth, ii);
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
            string messageString;
            if ((colOffset >= beginOffset) && (colOffset < endOffset))
            {
                vector<UINT08> message(dev.GetMessageLength());
                RC readRc = DoWriteOrRead(dev, false, colOffset, &message, tmpHandle);
                UINT32 messageVal = 0;
                dev.ArrayToInt(message.data(), &messageVal, dev.GetMessageLength());
                if (readRc == OK)
                {
                    messageString =
                        Utility::StrPrintf("%0*x", columnWidth, messageVal);
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
RC GpuI2c::DoGetI2cDcbDevInfo(I2cDcbDevInfo* pInfo)
{
    MASSERT(pInfo);

    RC rc;
    TmpI2cHandle tmpHandle;
    LW402C_CTRL_I2C_TABLE_GET_DEV_INFO_PARAMS params = {};

    CHECK_RC(tmpHandle.Initialize(GetGpuSubdevice()));
    CHECK_RC(LwRmPtr()->Control(tmpHandle,
                                LW402C_CTRL_CMD_I2C_TABLE_GET_DEV_INFO,
                                &params, sizeof(params)));
    MASSERT(NUM_DEV == NUMELEMS(params.i2cDevInfo));

    pInfo->resize(params.i2cDevCount);

    for (UINT32 dev = 0; dev < pInfo->size(); ++dev)
    {
        I2cDcbDevInfoEntry* dEntry = &(*pInfo)[dev];
        LW402C_CTRL_I2C_DEVICE_INFO rmInfo = params.i2cDevInfo[dev];
        dEntry->Type= rmInfo.type;
        dEntry->I2CAddress = rmInfo.i2cAddress;
        dEntry->I2CLogicalPort = rmInfo.i2cLogicalPort;
        dEntry->I2CDeviceIdx = rmInfo.i2cDevIdx;
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
RC GpuI2c::DoGetPortInfo(PortInfo *pPortInfo)
{
    MASSERT(pPortInfo);

    RC rc;
    TmpI2cHandle tmpHandle;
    LW402C_CTRL_I2C_GET_PORT_INFO_PARAMS params = {};

    CHECK_RC(tmpHandle.Initialize(GetGpuSubdevice()));
    CHECK_RC(LwRmPtr()->Control(tmpHandle,
                                LW402C_CTRL_CMD_I2C_GET_PORT_INFO,
                                &params, sizeof(params)));
    MASSERT(NUM_PORTS == NUMELEMS(params.info));

    pPortInfo->resize(NUM_PORTS);
    for (UINT32 port = 0; port < pPortInfo->size(); ++port)
    {
        PortInfoEntry *pEntry = &(*pPortInfo)[port];
        LwU8 rmInfo = params.info[port];
        pEntry->Implemented = FLD_TEST_DRF(402C_CTRL, _I2C_GET_PORT_INFO,
                                           _IMPLEMENTED, _YES, rmInfo);
        pEntry->DcbDeclared = FLD_TEST_DRF(402C_CTRL, _I2C_GET_PORT_INFO,
                                           _DCB_DECLARED, _YES, rmInfo);
        pEntry->DdcChannel = FLD_TEST_DRF(402C_CTRL, _I2C_GET_PORT_INFO,
                                          _DDC_CHANNEL, _PRESENT, rmInfo);
        pEntry->CrtcMapped = FLD_TEST_DRF(402C_CTRL, _I2C_GET_PORT_INFO,
                                          _CRTC_MAPPED, _YES, rmInfo);
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Helps to ping a I2C device and confirm it's availability.
//!
RC GpuI2c::DoPing(const Dev& dev)
{
    RC rc;
    LwRmPtr pLwRm;

    // Load the RmControl Param data struct
    //
    LW402C_CTRL_I2C_INDEXED_PARAMS param = {};
    param.portId   = dev.GetPort();
    param.address  = dev.GetAddr();
    param.flags    = GetFlags(dev)
                   | DRF_DEF(402C_CTRL, _I2C_FLAGS, _TRANSACTION_MODE, _PING);

    TmpI2cHandle tmpHandle;
    CHECK_RC(tmpHandle.Initialize(GetGpuSubdevice()));
    UINT32 retries = 0;
    rc = pLwRm->Control(tmpHandle, LW402C_CTRL_CMD_I2C_INDEXED,
                        &param, sizeof(param));
    while ((rc == RC::LWRM_IN_USE) && (retries++ < I2C_RETRIES))
    {
        Tasker::Sleep(I2C_RETRY_DELAY_MS);
        rc.Clear();
        rc = pLwRm->Control(tmpHandle, LW402C_CTRL_CMD_I2C_INDEXED,
                            &param, sizeof(param));
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC GpuI2c::DoRead(const Dev& dev, UINT32 offset, UINT32* pData)
{
    RC rc;

    vector<UINT08> message(dev.GetMessageLength(), 0);
    CHECK_RC(DoWriteOrRead(dev, false, offset, &message, 0));

    dev.ArrayToInt(message.data(), pData, dev.GetMessageLength());

    return rc;
}

//-----------------------------------------------------------------------------
RC GpuI2c::DoReadArray(const Dev& dev, UINT32 offset, UINT32 numReads, vector<UINT32>* pData)
{
    RC rc;
    MASSERT(pData);
    TmpI2cHandle tmpHandle;

    CHECK_RC(tmpHandle.Initialize(GetGpuSubdevice()));
    pData->clear();
    for (UINT32 i = 0; i < numReads; i++)
    {
        vector<UINT08> message(dev.GetMessageLength(), 0);
        CHECK_RC(DoWriteOrRead(dev, false, offset + i*dev.GetOffsetLength(), &message, tmpHandle));

        UINT32 wordData = 0;
        dev.ArrayToInt(message.data(), &wordData, dev.GetMessageLength());
        pData->push_back(wordData);
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC GpuI2c::DoReadSequence(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>* pData)
{
    MASSERT(pData);
    RC rc;

    pData->clear();

    vector<UINT08> message(numBytes, 0);
    CHECK_RC(DoWriteOrRead(dev, false, offset, &message, 0));

    pData->resize(numBytes);
    memcpy(pData->data(), message.data(), numBytes);

    return rc;
}

//-----------------------------------------------------------------------------
RC GpuI2c::DoReadBlk(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>* pData)
{
    MASSERT(pData);
    return DoBlkWriteOrRead(dev,
                            I2cReadWrite::READ,
                            offset,
                            numBytes,
                            pData,
                            0);
}

//--------------------------------------------------------------------
//! \brief Run the pullup-resistor test on this port
//!
//! Call the RM test to check for a missing pullup resistors on the
//! port used by this GpuI2c.  This method ignores the addr passed to
//! the constructor.
//!
RC GpuI2c::DoTestPort(const Dev& dev)
{
    MASSERT(dev.GetPort() < NUM_PORTS);

    LW2080_CTRL_I2C_ACCESS_PARAMS params = {};
    RC rc;

    params.cmd  = LW2080_CTRL_I2C_ACCESS_CMD_TEST_PORT;
    params.port = dev.GetPort() + 1; // This RM API takes a one-based port
    params.data = NULL;

    rc = LwRmPtr()->I2CAccess(&params, GetGpuSubdevice());

    if (params.status == LW2080_CTRL_I2C_ACCESS_STATUS_DP2TMDS_DONGLE_MISSING)
    {
        Printf(Tee::PriLow, "RM detects DP2TMDS dongle missing\n");
        rc.Clear();
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC GpuI2c::DoWrite(const Dev& dev, UINT32 offset, UINT32 data)
{
    vector<UINT08> message(dev.GetMessageLength());
    dev.IntToArray(data, message.data(), dev.GetMessageLength());
    return DoWriteOrRead(dev, true, offset, &message, 0);
}

//-----------------------------------------------------------------------------
RC GpuI2c::DoWriteArray(const Dev& dev, UINT32 offset, const vector<UINT32>& data)
{
    TmpI2cHandle tmpHandle;
    RC rc;

    CHECK_RC(tmpHandle.Initialize(GetGpuSubdevice()));
    for (UINT32 i = 0; i < data.size(); i++)
    {
        UINT32 wordData = data[i];
        vector<UINT08> message(dev.GetMessageLength());
        dev.IntToArray(wordData, message.data(), dev.GetMessageLength());
        CHECK_RC(DoWriteOrRead(dev, true, offset + i*dev.GetOffsetLength(), &message, tmpHandle));
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC GpuI2c::DoWriteSequence(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>& data)
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
    return DoWriteOrRead(dev, true, offset, &data, 0);
}

//-----------------------------------------------------------------------------
RC GpuI2c::DoWriteBlk(const Dev& dev, UINT32 offset, UINT32 numBytes, vector<UINT08>& data)
{
    return DoBlkWriteOrRead(dev,
                            I2cReadWrite::WRITE,
                            offset,
                            numBytes,
                            &data,
                            0);
}

//--------------------------------------------------------------------
//! \brief Common code for Write() and Read() methods
//!
RC GpuI2c::DoWriteOrRead
(
    const Dev& dev,
    bool isWrite,
    UINT32 offset,
    vector<UINT08>* pData,
    LwRm::Handle i2cHandle
)
{
    MASSERT(pData);
    RC rc;
    LwRmPtr pLwRm;

    // Load the RmControl Param data struct
    //
    LW402C_CTRL_I2C_INDEXED_PARAMS param = {};
    param.portId   = dev.GetPort();
    param.bIsWrite = isWrite;
    param.address  = dev.GetAddr();
    param.flags    = GetFlags(dev);

    if ((isWrite && dev.GetWriteCmd()) || (!isWrite && dev.GetReadCmd()))
    {
        param.index[0] = isWrite ? dev.GetWriteCmd() : dev.GetReadCmd();
        dev.IntToArray(offset, &param.index[1], dev.GetOffsetLength());
        param.indexLength = dev.GetOffsetLength() + 1;
    }
    else
    {
        dev.IntToArray(offset, &param.index[0], dev.GetOffsetLength());
        param.indexLength = dev.GetOffsetLength();
    }

    if (!isWrite && dev.GetVerifyReads())
    {
        pData->resize(2 * pData->size());
        fill(pData->begin(), pData->end(), 0);
    }
    param.pMessage = LW_PTR_TO_LwP64(pData->data());
    param.messageLength = static_cast<LwU32>(pData->size());

    // Call the RM.  Alloc a temporary I2cHandle if needed.
    //
    TmpI2cHandle tmpHandle;
    CHECK_RC(tmpHandle.Initialize(GetGpuSubdevice(), i2cHandle));
    UINT32 retries = 0;
    rc = pLwRm->Control(tmpHandle, LW402C_CTRL_CMD_I2C_INDEXED,
                        &param, sizeof(param));
    while ((rc == RC::LWRM_IN_USE) && (retries++ < I2C_RETRIES))
    {
        Tasker::Sleep(I2C_RETRY_DELAY_MS);
        rc.Clear();
        rc = pLwRm->Control(tmpHandle, LW402C_CTRL_CMD_I2C_INDEXED,
                            &param, sizeof(param));
    }
    CHECK_RC(rc);

    // Unload the RmControl Param data struct
    //
    if (!isWrite && dev.GetVerifyReads())
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

//--------------------------------------------------------------------
//! Return the flags that should be passed to
//! LW402C_CTRL_CMD_I2C_INDEXED.  Called by DoWriteOrRead().
//!
UINT32 GpuI2c::GetFlags(const Dev& dev) const
{
    UINT32 Flags = 0;

    // set Speed
    UINT32 RmSpeed = LW402C_CTRL_I2C_FLAGS_SPEED_MODE_DEFAULT;
    switch (dev.GetSpeedInKHz())
    {
        case SPEED_100KHz:
            RmSpeed = LW402C_CTRL_I2C_FLAGS_SPEED_MODE_100KHZ;
            break;
        case SPEED_200KHz:
            RmSpeed = LW402C_CTRL_I2C_FLAGS_SPEED_MODE_200KHZ;
            break;
        case SPEED_400KHz:
            RmSpeed = LW402C_CTRL_I2C_FLAGS_SPEED_MODE_400KHZ;
            break;
        case SPEED_33KHz:
            RmSpeed = LW402C_CTRL_I2C_FLAGS_SPEED_MODE_33KHZ;
            break;
        case SPEED_10KHz:
            RmSpeed = LW402C_CTRL_I2C_FLAGS_SPEED_MODE_10KHZ;
            break;
        case SPEED_3KHz:
            RmSpeed = LW402C_CTRL_I2C_FLAGS_SPEED_MODE_3KHZ;
            break;
        case SPEED_300KHz:
            RmSpeed = LW402C_CTRL_I2C_FLAGS_SPEED_MODE_300KHZ;
            break;
        default:
            Printf(Tee::PriNormal, "invalid I2C Speed\n");
            return RC::BAD_PARAMETER;
    }
    Flags |= DRF_NUM(402C_CTRL, _I2C_FLAGS, _SPEED_MODE, RmSpeed);

    UINT32 RmAddrMode = LW402C_CTRL_I2C_FLAGS_ADDRESS_MODE_DEFAULT;
    switch (dev.GetAddrMode())
    {
        case ADDRMODE_7BIT:
            RmAddrMode = LW402C_CTRL_I2C_FLAGS_ADDRESS_MODE_7BIT;
            break;
        case ADDRMODE_10BIT:
            RmAddrMode = LW402C_CTRL_I2C_FLAGS_ADDRESS_MODE_10BIT;
            break;
        default:
            Printf(Tee::PriNormal, "invalid I2C address length\n");
            return RC::BAD_PARAMETER;
    }
    Flags |= DRF_NUM(402C_CTRL, _I2C_FLAGS, _ADDRESS_MODE, RmAddrMode);
    Flags |= DRF_DEF(402C_CTRL, _I2C_FLAGS, _PRIVILEGE, _NDA);

    return Flags;
}

//-----------------------------------------------------------------------------
GpuSubdevice* GpuI2c::GetGpuSubdevice()
{
    return GetDevice()->GetInterface<GpuSubdevice>();
}
