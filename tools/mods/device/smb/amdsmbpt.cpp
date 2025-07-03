/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file intsmbpt.cpp
//! \brief Implementation of class for AMD Smbus ports
//!
//! Contains class implementation for AMD Smbus ports
//!

#include "amdsmbpt.h"
#include "core/include/platform.h"
#include "core/include/tee.h"
#include "amdsmbrg.h"
#include "core/include/tasker.h"
#include "core/include/data.h"
#include "cheetah/include/logmacros.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME  "AmdSmbPort"

AmdSmbPort::AmdSmbPort(SmbDevice *pSmbDev, UINT32 Index, UINT32 CtrlBase)
    : SmbPort(pSmbDev, Index, CtrlBase)
    , m_CtrlBase(CtrlBase)
    , m_Index(Index)
    , m_IsSearched(false)
{
    LOG_ENT();
    LOG_EXT();
}

AmdSmbPort::~AmdSmbPort()
{
    Uninitialize();
}

RC AmdSmbPort::RegRd(UINT32 Reg, UINT32 *pData, bool IsDebug)
{
    if (!pData)
    {
        Printf(Tee::PriError, "AmdSmbPort::RegRd - pData argument is null pointer");
        return RC::ILWALID_INPUT;
    }

    *pData = 0;
    if (Reg <= AMD_SMBUS_REG_SLVDATB)
    {
        UINT08 byteData = 0;
        Platform::PioRead08(m_CtrlBase + Reg, &byteData);
        *pData = byteData;
    }
    else
    {
        return RC::ILWALID_REGISTER_NUMBER;
    }
    return OK;
}

RC AmdSmbPort::RegWr(UINT32 Reg, UINT32 Data, bool IsDebug)
{
    if (Reg <= AMD_SMBUS_REG_SLVDATB)
    {
        Platform::PioWrite08(m_CtrlBase + Reg, (UINT08)Data);
    }
    else
    {
        return RC::ILWALID_REGISTER_NUMBER;
    }
    return OK;
}

RC AmdSmbPort::Initialize()
{
    RC rc;
    CHECK_RC(ResetCtrlReg());
    m_IsSearched = false;
    m_IsInit = true;
    return OK;
}

RC AmdSmbPort::ResetCtrlReg()
{
    RC rc;
    CHECK_RC(RegWr(AMD_SMBUS_REG_HSTSTS, 0));
    CHECK_RC(RegWr(AMD_SMBUS_REG_HSTCTRL, 0));
    CHECK_RC(RegWr(AMD_SMBUS_REG_HSTCMD, 0));
    CHECK_RC(RegWr(AMD_SMBUS_REG_HSTADDR, 0));
    CHECK_RC(RegWr(AMD_SMBUS_REG_SLVCTRL, 0));
    return rc;
}

RC AmdSmbPort::Uninitialize()
{
    RC rc;
    m_IsInit = false;
    return OK;
}

RC AmdSmbPort::PrintInfo(Tee::Priority Pri)
{
    RC rc;
    UINT32 status;
    CHECK_RC(RegRd(AMD_SMBUS_REG_HSTSTS, &status));
    Printf(Pri, "AmdSmbPort_%i IO Base: 0x%08x\n", m_Index, m_CtrlBase);
    Printf(Pri, "Status (AMD_SMBUS_REG_HSTSTS): 0x%08x\n", status);
    Printf(Pri, "\tStatus Bit PEC Error: 0x%02x\n", status & AMD_SMBUS_STATUS_PEC_ERROR);
    Printf(Pri, "\tStatus Bit Bus Collision: 0x%02x\n", status & AMD_SMBUS_STATUS_BUS_COLLISION);
    Printf(Pri, "\tStatus Bit Device Error: 0x%02x\n", status & AMD_SMBUS_STATUS_DEVICE_ERROR);
    Printf(Pri, "\tStatus Bit Interrupt: 0x%02x\n", status & AMD_SMBUS_STATUS_INTERRUPT);
    Printf(Pri, "\tStatus Bit Host Busy: 0x%02x\n", status & AMD_SMBUS_STATUS_HOST_BUSY);
    return OK;
}

RC AmdSmbPort::PrintReg(Tee::Priority Pri)
{
    RC rc;
    UINT32 reg;
    CHECK_RC(PrintInfo(Pri));
    CHECK_RC(RegRd(AMD_SMBUS_REG_SLVSTS, &reg));
    Printf(Pri,"Slv Status (AMD_SMBUS_REG_SLVSTS): 0x%08x\n", reg);
    CHECK_RC(RegRd(AMD_SMBUS_REG_HSTCTRL, &reg));
    Printf(Pri,"Control (AMD_SMBUS_REG_HSTCTRL): 0x%08x\n", reg);
    CHECK_RC(RegRd(AMD_SMBUS_REG_HSTCMD, &reg));
    Printf(Pri,"Host Command (AMD_SMBUS_REG_HSTCMD): 0x%08x\n", reg);
    CHECK_RC(RegRd(AMD_SMBUS_REG_HSTADDR, &reg));
    Printf(Pri,"Address (AMD_SMBUS_REG_HSTADDR): 0x%08x\n", reg);
    CHECK_RC(RegRd(AMD_SMBUS_REG_HSTDAT0, &reg));
    Printf(Pri,"Data 0 (AMD_SMBUS_REG_HSTDAT0): 0x%08x\n", reg);
    CHECK_RC(RegRd(AMD_SMBUS_REG_HSTDAT1, &reg));
    Printf(Pri,"Data 1 (AMD_SMBUS_REG_HSTDAT1): 0x%08x\n", reg);
    CHECK_RC(RegRd(AMD_SMBUS_REG_BLKDAT, &reg));
    Printf(Pri,"Block Data (AMD_SMBUS_REG_BLKDAT): 0x%08x\n", reg);
    CHECK_RC(RegRd(AMD_SMBUS_REG_SLVCTRL, &reg));
    Printf(Pri,"Slv Control (AMD_SMBUS_REG_SLVCTRL): 0x%08x\n", reg);
    CHECK_RC(RegRd(AMD_SMBUS_REG_SHDWCMD, &reg));
    Printf(Pri,"Shadow Command (AMD_SMBUS_REG_SHDWCMD): 0x%08x\n", reg);
    CHECK_RC(RegRd(AMD_SMBUS_REG_SLVEVTA, &reg));
    Printf(Pri,"Slv Event Low Byte (AMD_SMBUS_REG_SLVEVTA): 0x%08x\n", reg);
    CHECK_RC(RegRd(AMD_SMBUS_REG_SLVEVTB, &reg));
    Printf(Pri,"Slv Event High Byte (AMD_SMBUS_REG_SLVEVTB): 0x%08x\n", reg);
    CHECK_RC(RegRd(AMD_SMBUS_REG_SLVDATA, &reg));
    Printf(Pri,"Slv Data Low Byte (AMD_SMBUS_REG_SLVDATA): 0x%08x\n", reg);
    CHECK_RC(RegRd(AMD_SMBUS_REG_SLVDATB, &reg));
    Printf(Pri,"Slv Data High Byte (AMD_SMBUS_REG_SLVDATB): 0x%08x\n", reg);
    return rc;
}

RC AmdSmbPort::WrQuickCmd(UINT16 SlvAddr)
{
    RC rc;
    CHECK_RC(ResetAndExelwte(SlvAddr, Protocol::WR_QUICK, NULL, NULL, 0));
    return OK;
}

RC AmdSmbPort::RdQuickCmd(UINT16 SlvAddr)
{
    RC rc;
    CHECK_RC(ResetAndExelwte(SlvAddr, Protocol::RD_QUICK, NULL, NULL, 0));
    return OK;
}

RC AmdSmbPort::SendByteCmd(UINT16 SlvAddr, UINT08 Data)
{
    RC rc;
    vector<UINT08> vData(1, Data);
    CHECK_RC(ResetAndExelwte(SlvAddr, Protocol::SND_BYTE, &vData, NULL, 0));
    return OK;
}

RC AmdSmbPort::RecvByteCmd(UINT16 SlvAddr, UINT08 *pData)
{
    RC rc;
    vector<UINT08> vData(1);
    CHECK_RC(ResetAndExelwte(SlvAddr, Protocol::RCV_BYTE, NULL, &vData, 0));
    *pData = vData[1];
    return OK;
}

RC AmdSmbPort::WrByteCmd(UINT16 SlvAddr, UINT08 CmdCode, UINT08 Data)
{
    RC rc;
    vector<UINT08> vData(1, Data);
    CHECK_RC(ResetAndExelwte(SlvAddr, Protocol::WR_BYTE, &vData, NULL, CmdCode));
    return OK;
}

RC AmdSmbPort::RdByteCmd(UINT16 SlvAddr, UINT08 CmdCode, UINT08 *pData)
{
    RC rc;
    vector<UINT08> vData(1);
    CHECK_RC(ResetAndExelwte(SlvAddr, Protocol::RD_BYTE, NULL, &vData, CmdCode));
    *pData = vData[0];
    return OK;
}

RC AmdSmbPort::WrWordCmd(UINT16 SlvAddr, UINT08 CmdCode, UINT16 Data)
{
    RC rc;
    vector<UINT08> vData(2);
    vData[0] = Data & 0xff;
    vData[1] = Data >> 8;
    CHECK_RC(ResetAndExelwte(SlvAddr, Protocol::WR_WORD, &vData, NULL, CmdCode));
    return OK;
}

RC AmdSmbPort::RdWordCmd(UINT16 SlvAddr, UINT08 CmdCode, UINT16 *pData)
{
    RC rc;
    vector<UINT08> vData(2);
    CHECK_RC(ResetAndExelwte(SlvAddr, Protocol::RD_WORD, NULL, &vData, CmdCode));
    *pData = (vData[1] << 8) | vData[0];
    return OK;
}

RC AmdSmbPort::WrBlkCmd(UINT16 SlvAddr, UINT08 CmdCode, vector<UINT08> *pData)
{
    RC rc;
    CHECK_RC(ResetAndExelwte(SlvAddr, Protocol::WR_BLK, pData, NULL, CmdCode));
    return OK;
}

RC AmdSmbPort::RdBlkCmd(UINT16 SlvAddr, UINT08 CmdCode, vector<UINT08> *pData)
{
    RC rc;
    CHECK_RC(ResetAndExelwte(SlvAddr, Protocol::RD_BLK, NULL, pData, CmdCode));
    return OK;
}

RC AmdSmbPort::ProcCallCmd(UINT16 SlvAddr, UINT08 CmdCode, UINT16 DataWr, UINT16 *pDataRd)
{
    Printf(Tee::PriError, "ProcCallCmd is not implemented in AmdSmbPort");
    return RC::UNSUPPORTED_FUNCTION;
}

RC AmdSmbPort::ProcCallBlkCmd(UINT16 SlvAddr, UINT08 CmdCode,vector<UINT08> *pDataWr, vector<UINT08> *pDataRd)
{
    Printf(Tee::PriError, "ProcCallBlkCmd is not implemented in AmdSmbPort");
    return RC::UNSUPPORTED_FUNCTION;
}

RC AmdSmbPort::HostNotify(UINT08 DvcAdrs, UINT16 Data)
{
    Printf(Tee::PriError, "HostNotify is not implemented in AmdSmbPort");
    return RC::UNSUPPORTED_FUNCTION;
}

RC AmdSmbPort::Search()
{
    LOG_ENT();
    RC rc;
    UINT08 temp;

    m_vSlvAddr.clear();

    for (UINT08 slvAddr = 0; slvAddr < MAX_DEVICES; slvAddr++)
    {
        rc = RdByteCmd(slvAddr, 0, &temp);
        if(OK == rc)
        {
            m_vSlvAddr.push_back(slvAddr);
        }
    }

    Printf(Tee::PriNormal, "SmbusPort_%i found %zu devices\n", m_Index, m_vSlvAddr.size());
    VectorData::Print(&m_vSlvAddr, Tee::PriNormal);

    m_IsSearched = true;
    LOG_EXT();
    return OK;
}

RC AmdSmbPort::IsSlavePresent(UINT16 SlvAddr)
{
    UINT08 temp;
    RC rc = RdByteCmd(SlvAddr, 0, &temp);

    if (OK == rc)
    {
        Printf(Tee::PriNormal, "Smb Device at address 0x%04x is CONNECTED", SlvAddr);
    }
    else
    {
        Printf(Tee::PriError, "Smb Device at address 0x%04x is NOT CONNECTED", SlvAddr);
        return RC::UNEXPECTED_RESULT;
    }
    return OK;
}

RC AmdSmbPort::GetSlaveAddrs(vector<UINT08> *pvSlvAddr)
{
    LOG_ENT();
    if (!m_IsSearched)
    {
        Printf(Tee::PriError, "AmdSmbPort not searched");
        return RC::WAS_NOT_INITIALIZED;
    }
    if(!pvSlvAddr)
    {
        Printf(Tee::PriError, "pvSlvAddr is null");
        return RC::ILWALID_INPUT;
    }
    for (UINT32 i = 0; i < m_vSlvAddr.size(); i++)
    {
        pvSlvAddr->push_back(m_vSlvAddr[i]);
    }
    LOG_EXT();
    return OK;
}

RC AmdSmbPort::SetData(vector<UINT08> *pData, Protocol Protocol)
{
    LOG_ENT();
    RC rc;

    if (!pData)
    {
        Printf(Tee::PriError, "AmdSmbPort::SetData - Invalid pData");
        return RC::ILWALID_INPUT;
    }

    UINT08 byteCnt = pData->size();

    // Non-block writes
    if ((Protocol == Protocol::SND_BYTE) ||
        (Protocol == Protocol::WR_BYTE) ||
        (Protocol == Protocol::WR_WORD))
    {
        if (((Protocol != Protocol::WR_WORD) && (byteCnt != 1)) ||
            ((Protocol == Protocol::WR_WORD) && (byteCnt != 2)))
        {
            Printf(Tee::PriError, "Invalid byteCnt = %d", byteCnt);
            return RC::ILWALID_INPUT;
        }

        CHECK_RC(RegWr(AMD_SMBUS_REG_HSTDAT0, (*pData)[0]));
        if (Protocol == Protocol::WR_WORD)
        {
            CHECK_RC(RegWr(AMD_SMBUS_REG_HSTDAT1, (*pData)[1]));
        }
        return rc;
    }

    if (byteCnt > AMD_SMBUS_BLOCK_MAX)
    {
        Printf(Tee::PriError, "Invalid byteCnt = 0x%x", byteCnt);
        return RC::ILWALID_INPUT;
    }

    // For block-sized writes, HSTDATA0 stores the number of bytes
    CHECK_RC(RegWr(AMD_SMBUS_REG_HSTDAT0, byteCnt));

    // Reading REG_HSTCTRL resets the block data register (REG_BLKDAT)
    UINT32 cntrl;
    CHECK_RC(RegRd(AMD_SMBUS_REG_HSTCTRL, &cntrl));

    for (UINT08 i = 0; i < byteCnt; i++)
    {
        CHECK_RC(RegWr(AMD_SMBUS_REG_BLKDAT, (*pData)[i]));
    }

    LOG_EXT();
    return OK;
}

RC AmdSmbPort::SetProtocol(Protocol Protocol)
{
    RC rc;
    UINT08 prtcl = AMD_SMBUS_HSTCTRL_PROTO_QUICK;

    switch (Protocol) {
    case Protocol::WR_QUICK:
    case Protocol::RD_QUICK:
        prtcl = AMD_SMBUS_HSTCTRL_PROTO_QUICK;
        break;
    case Protocol::SND_BYTE:
    case Protocol::RCV_BYTE:
        prtcl = AMD_SMBUS_HSTCTRL_PROTO_BYTE;
        break;
    case Protocol::WR_BYTE:
    case Protocol::RD_BYTE:
        prtcl = AMD_SMBUS_HSTCTRL_PROTO_BYTE_DATA;
        break;
    case Protocol::WR_WORD:
    case Protocol::RD_WORD:
        prtcl = AMD_SMBUS_HSTCTRL_PROTO_WORD_DATA;
        break;
    case Protocol::WR_BLK:
    case Protocol::RD_BLK:
        prtcl = AMD_SMBUS_HSTCTRL_PROTO_BLOCK;
        break;
    }

    CHECK_RC(RegWr(AMD_SMBUS_REG_HSTCTRL, prtcl & AMD_SMBUS_HSTCTRL_PROTO_MASK));
    return OK;
}

RC AmdSmbPort::GetData(vector<UINT08> *pData, Protocol Protocol)
{
    LOG_ENT();
    RC rc;
    UINT32 regval;

    if (!pData)
    {
        Printf(Tee::PriError, "AmdSmbPort::GetData - Invalid pData");
        return RC::ILWALID_INPUT;
    }

    // Non-block reads
    if ((Protocol == Protocol::RCV_BYTE) ||
        (Protocol == Protocol::RD_BYTE) ||
        (Protocol == Protocol::RD_WORD))
    {
        if (((Protocol != Protocol::RD_WORD) && (pData->size() != 1)) ||
            ((Protocol == Protocol::RD_WORD) && (pData->size() != 2)))
        {
            Printf(Tee::PriError, "Invalid byteCnt = 0x%x", (UINT32)pData->size());
            return RC::ILWALID_INPUT;
        }

        CHECK_RC(RegRd(AMD_SMBUS_REG_HSTDAT0, &regval));
        (*pData)[0] = regval;

        if (Protocol == Protocol::RD_WORD)
        {
            CHECK_RC(RegRd(AMD_SMBUS_REG_HSTDAT1, &regval));
            (*pData)[1] = regval;
        }
        return OK;
    }

    UINT32 byteCnt;

    // For block-sized reads, HSTDATA0 stores the number of bytes
    CHECK_RC(RegRd(AMD_SMBUS_REG_HSTDAT0, &byteCnt));

    if ((byteCnt == 0) || (byteCnt > AMD_SMBUS_BLOCK_MAX))
    {
        Printf(Tee::PriError, "Invalid byteCnt = 0x%x", byteCnt);
        return RC::UNEXPECTED_RESULT;
    }

    // Reading REG_HSTCTRL resets the block data register (REG_BLKDAT)
    CHECK_RC(RegRd(AMD_SMBUS_REG_HSTCTRL, &regval));
    pData->clear();

    for (UINT08 i = 0; i < byteCnt; i++)
    {
        CHECK_RC(RegRd(AMD_SMBUS_REG_BLKDAT, &regval));
        pData->push_back(regval);
    }

    LOG_EXT();
    return OK;
}

RC AmdSmbPort::Transaction()
{
    RC rc;
    UINT32 regval;
    UINT32 retries = MAX_RETRIES;

    // Make sure the SMBus host is ready to start transmitting
    CHECK_RC(RegRd(AMD_SMBUS_REG_HSTSTS, &regval));
    if (regval != 0x00)
    {
        // Reset
        CHECK_RC(RegWr(AMD_SMBUS_REG_HSTSTS, regval));

        CHECK_RC(RegRd(AMD_SMBUS_REG_HSTSTS, &regval));
        if (regval != 0x00)
        {
            Printf(Tee::PriWarn, "AmdSmbPort::Transaction - Device Busy\n");
            return RC::CMD_STATUS_ERROR;
        }
    }

    // Start the transaction by setting bit 6 of REG_HSTCTRL
    CHECK_RC(RegRd(AMD_SMBUS_REG_HSTCTRL, &regval));
    CHECK_RC(RegWr(AMD_SMBUS_REG_HSTCTRL, regval | AMD_SMBUS_HSTCTRL_START));
    Tasker::Sleep(5);

    // Wait for transaction to finish
    while (retries--)
    {
        CHECK_RC(RegRd(AMD_SMBUS_REG_HSTSTS, &regval));
        if ((regval & AMD_SMBUS_STATUS_HOST_BUSY) == 0x00)
            break;
        Tasker::Sleep(1);
    }
    if (!retries)
    {
        Printf(Tee::PriWarn, "AmdSmbPort::Transaction - SMBus Timeout\n");
        return RC::CMD_STATUS_ERROR;
    }

    // Error checking
    if (((regval & AMD_SMBUS_STATUS_PEC_ERROR) != 0) ||
        ((regval & AMD_SMBUS_STATUS_BUS_COLLISION) != 0) ||
        ((regval & AMD_SMBUS_STATUS_DEVICE_ERROR) != 0))
    {
        Printf(Tee::PriWarn, "AmdSmbPort::Transaction - Failed, Status = 0x%x\n",
            regval);
        return RC::CMD_STATUS_ERROR;
    }

    // Reset after transaction
    CHECK_RC(RegRd(AMD_SMBUS_REG_HSTSTS, &regval));
    if (regval != 0x00)
    {
        CHECK_RC(RegWr(AMD_SMBUS_REG_HSTSTS, regval));
    }
    CHECK_RC(RegRd(AMD_SMBUS_REG_HSTSTS, &regval));
    if (regval != 0x00)
    {
        Printf(Tee::PriWarn, "AmdSmbPort::Transaction - Failed reset after transaction\n");
        return RC::CMD_STATUS_ERROR;
    }

    return rc;
}

RC AmdSmbPort::AcquireSemaphore()
{
    RC rc;
    UINT32 retries = MAX_RETRIES;
    UINT32 regval;
    CHECK_RC(RegRd(AMD_SMBUS_REG_SLVCTRL, &regval));
    while (retries--)
    {
        CHECK_RC(RegWr(AMD_SMBUS_REG_SLVCTRL, regval | AMD_SMBUS_SLVCTRL_HOST_SEMA));
        CHECK_RC(RegRd(AMD_SMBUS_REG_SLVCTRL, &regval));
        if (regval & AMD_SMBUS_SLVCTRL_HOST_SEMA)
            break;
        Tasker::Sleep(1);
    }

    // Failed to acquire semaphore
    if (!retries)
    {
        Printf(Tee::PriWarn, "AmdSmbPort::AcquireSemaphore - Device Busy\n");
        return RC::CMD_STATUS_ERROR;
    }

    return OK;
}

RC AmdSmbPort::ReleaseSemaphore()
{
    RC rc;
    UINT32 regval;
    CHECK_RC(RegRd(AMD_SMBUS_REG_SLVCTRL, &regval));
    CHECK_RC(RegWr(AMD_SMBUS_REG_SLVCTRL, regval | AMD_SMBUS_SLVCTRL_CLEAR_HOST_SEMA));
    return OK;
}

RC AmdSmbPort::ResetAndExelwte(UINT16 SlvAddr, Protocol Protocol, vector<UINT08> *pDataWr,
                          vector<UINT08> *pDataRd, UINT08 Cmd)
{
    LOG_ENT();
    RC rc;
    UINT08 regval = 0;

    // Reset the relevant registers
    CHECK_RC(ResetCtrlReg());

    // Port selection
    Platform::PioWrite08(AMD_SMBUS_PORT_PM_IDX, AMD_SMBUS_PM_00_PORTSEL_OFFSET);
    Platform::PioRead08(AMD_SMBUS_PORT_PM_DATA, &regval);
    if ((regval & AMD_SMBUS_PM_00_PORTSEL_MASK) != AMD_SMBUS_PM_00_PORTSEL_2)
    {
        Platform::PioWrite08(AMD_SMBUS_PORT_PM_DATA,
            (regval & ~AMD_SMBUS_PM_00_PORTSEL_MASK) | AMD_SMBUS_PM_00_PORTSEL_2);
    }

    AcquireSemaphore();

    rc = ExelwteSmbCmd(SlvAddr, Protocol, pDataWr, pDataRd, Cmd);

    ReleaseSemaphore();

    // Restore port selection. This must be done even if ExelwteSmbCmd fails,
    // otherwise some systems fail to warm reboot. 
    Platform::PioWrite08(AMD_SMBUS_PORT_PM_DATA, regval);

    LOG_EXT();
    return rc;
}

RC AmdSmbPort::ExelwteSmbCmd(UINT16 SlvAddr, Protocol Protocol, vector<UINT08> *pDataWr,
                          vector<UINT08> *pDataRd, UINT08 Cmd)
{
    RC rc;

    // Should be a 7-bit address
    if (SlvAddr > 0x7f)
    {
        Printf(Tee::PriError, "Invalid Addr = 0x%x", SlvAddr);
        return RC::ILWALID_INPUT;
    }

    // Set address register
    UINT08 ReadBit = (Protocol == Protocol::RCV_BYTE ||
        Protocol == Protocol::RD_BYTE ||
        Protocol == Protocol::RD_WORD ||
        Protocol == Protocol::RD_BLK ||
        Protocol == Protocol::RD_QUICK) ?
        AMD_SMBUS_ADDR_READ : AMD_SMBUS_ADDR_WRITE;
    CHECK_RC(RegWr(AMD_SMBUS_REG_HSTADDR, (SlvAddr << 1) | ReadBit));

    // Set command register
    CHECK_RC(RegWr(AMD_SMBUS_REG_HSTCMD, Cmd));

    // Set data to be written, WR_QUICK doesn't write any data
    if (ReadBit == AMD_SMBUS_ADDR_WRITE && Protocol != Protocol::WR_QUICK)
    {
        CHECK_RC(SetData(pDataWr, Protocol));
    }

    CHECK_RC(SetProtocol(Protocol));

    // Execute command
    CHECK_RC(Transaction());

    // Get data for read cmds, RD_QUICK doesn't read any data
    if (ReadBit == AMD_SMBUS_ADDR_READ && Protocol != Protocol::RD_QUICK)
    {
        CHECK_RC(GetData(pDataRd, Protocol));
    }

    return rc;
}

