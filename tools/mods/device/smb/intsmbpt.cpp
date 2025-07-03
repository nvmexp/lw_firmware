/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2010,2016-2017,2020 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file intsmbpt.cpp
//! \brief Implementation of class for Intel Smbus ports
//!
//! Contains class implementation for Intel Smbus ports
//!

#include "intsmbpt.h"
#include "core/include/platform.h"
#include "core/include/tee.h"
#include "intsmbrg.h"
#include "core/include/tasker.h"
#include "core/include/data.h"
#include "cheetah/include/logmacros.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME  "IntelSmbPort"

IntelSmbPort::IntelSmbPort(SmbDevice *pSmbDev, UINT32 Index, UINT32 CtrlBase)
    : SmbPort(pSmbDev, Index, CtrlBase)
    , m_CtrlBase(CtrlBase)
    , m_Index(Index)
    , m_IsSearched(false)
{
    LOG_ENT();
    LOG_EXT();
}

IntelSmbPort::~IntelSmbPort() //destructor
{
    Uninitialize();
}

RC IntelSmbPort::RegRd(UINT32 Reg, UINT32 *pData, bool IsDebug)
{
    if(!pData)
    {
        Printf(Tee::PriError, "IntelSmbPort::RegRd - pData argument is null pointer");
        return RC::ILWALID_INPUT;
    }

    *pData = 0;
    if(Reg<=INTEL_SMBUS_REG_SMLINK_PIN_CTL)
    {
        UINT08 byteData;
        Platform::PioRead08(m_CtrlBase+Reg, &byteData);
        *pData = byteData;
    }
    else
    {
        return RC::ILWALID_REGISTER_NUMBER;
    }
    return OK;
}

RC IntelSmbPort::RegWr(UINT32 Reg, UINT32 Data, bool IsDebug)
{
    if(Reg<=INTEL_SMBUS_REG_SMLINK_PIN_CTL)
        Platform::PioWrite08(m_CtrlBase+Reg,(UINT08)Data);
    else
    {
        return RC::ILWALID_REGISTER_NUMBER;
    }
    return OK;
}

RC IntelSmbPort::Initialize()
{
    RC rc;

    m_IsSearched = false;
    CHECK_RC(ResetCtrlReg());
    m_IsInit = true;
    return OK;
}

RC IntelSmbPort::ResetCtrlReg()
{
    LOG_ENT();

    RC rc;

    CHECK_RC(Uninitialize());
    LOG_EXT();
    return rc;
}

RC IntelSmbPort::Uninitialize()
{
    LOG_ENT();
    RC rc;

    //clear alarm
    CHECK_RC(RegWr(INTEL_SMBUS_REG_STS,INTEL_SMBUS_STS_CLEAR));

    m_IsInit = false;
    LOG_EXT();
    return OK;
}

RC IntelSmbPort::PrintInfo(Tee::Priority Pri)
{
    RC rc;
    UINT32 status = 0;

    CHECK_RC(RegRd(INTEL_SMBUS_REG_STS,&status));
    Printf(Pri, "Port_%i IO_CtrlBase: 0x%04x\n", m_Index, m_CtrlBase);
    Printf(Pri, "Port_%i Status: 0x%02x\n", m_Index, status);

    switch (status&INTEL_STS_STATUS_MASK)
    {
        case INTEL_STS_STATUS_OK:
            Printf(Pri, "\tSTATUS_OK\n");
            break;

        case INTEL_STS_STATUS_NAK:
            Printf(Pri, "\tSTATUS_NAK\n");
            break;

        case INTEL_STS_STATUS_TIMEOUT:
            Printf(Pri, "\tSTATUS_TINEOUT\n");
            break;

        case INTEL_STS_STATUS_UNSUP_PRTCL:
            Printf(Pri, "\tSTATUS_UNSUP_PRTCL\n");
            break;

        case INTEL_STS_STATUS_SMBUS_BUSY:
            Printf(Pri, "\tSTATUS_BUSY\n");
            break;

        case INTEL_STS_STATUS_PEC_ERROR:
            Printf(Pri, "\tSTATUS_ERROR\n");
            break;

        default:
            Printf(Pri, "\tIlwalid Status.\n");
    }

    if (status&INTEL_STS_ALARM_ACTIVE)
        Printf(Pri, "\tALARM_ACTIVE.\n");
    else
        Printf(Pri, "\tALARM_INACTIVE/CLEAR.\n");

    if (status&INTEL_STS_DONE_OK)
        Printf(Pri, "\tDONE_OK.\n");
    else
        Printf(Pri, "\tDONE_NOTOK.\n");
    return OK;
}

RC IntelSmbPort::PrintReg(Tee::Priority Pri)
{
    RC rc;
    UINT32 reg;

    Printf(Pri, "Port_%i IO_CtrlBase: 0x%04x\n", m_Index, m_CtrlBase);

    CHECK_RC(RegRd(INTEL_SMBUS_REG_STS,&reg));
    Printf(Pri,"\n\tINTEL_SMBUS_REG_STS:\t\t0x%08x",reg);
    CHECK_RC(RegRd(INTEL_SMBUS_REG_CNT,&reg));
    Printf(Pri,"\n\tINTEL_SMBUS_REG_CNT:\t\t0x%08x",reg);
    CHECK_RC(RegRd(INTEL_SMBUS_REG_CMD,&reg));
    Printf(Pri,"\n\tINTEL_SMBUS_REG_CMD:\t\t0x%08x",reg);
    CHECK_RC(RegRd(INTEL_SMBUS_REG_ADDR,&reg));
    Printf(Pri,"\n\tINTEL_SMBUS_REG_ADDR:\t\t0x%08x",reg);
    CHECK_RC(RegRd(INTEL_SMBUS_REG_HST_D0,&reg));
    Printf(Pri,"\n\tINTEL_SMBUS_REG_HST_D0:\t0x%08x",reg);
    CHECK_RC(RegRd(INTEL_SMBUS_REG_HST_D1,&reg));
    Printf(Pri,"\n\tINTEL_SMBUS_REG_HST_D1:\t0x%08x",reg);
    CHECK_RC(RegRd(INTEL_SMBUS_REG_BCNT,&reg));
    Printf(Pri,"\n\tINTEL_SMBUS_REG_BCNT:\t\t0x%08x",reg);
    return rc;
}

RC IntelSmbPort::SetData(vector<UINT08> *pData, UINT08 Protocol)
{
    LOG_ENT();
    RC rc;

    if (!pData)
    {
        Printf(Tee::PriError, "IntelSmbPort::SetData - Invalid pData");
        return RC::ILWALID_INPUT;
    }

    UINT08 byteCnt = pData->size();

    if ((Protocol == INTEL_PRTCL_SND_BYTE) ||
        (Protocol == INTEL_PRTCL_WR_BYTE) ||
        (Protocol == INTEL_PRTCL_WR_WORD))
    {
        if (((Protocol != INTEL_PRTCL_WR_WORD) && (byteCnt != 1)) ||
            ((Protocol == INTEL_PRTCL_WR_WORD) && (byteCnt != 2)))
        {
            Printf(Tee::PriError, "Invalid byteCnt = 0x%x for protocol = 0x%x",
                    byteCnt, Protocol);
            return RC::ILWALID_INPUT;
        }
        CHECK_RC( RegWr(INTEL_SMBUS_REG_HST_D0, (*pData)[0]) );
        if (Protocol == INTEL_PRTCL_WR_WORD)
        {
            CHECK_RC( RegWr(INTEL_SMBUS_REG_HST_D1, (*pData)[1]) );
        }
        return rc;
    }

    if (byteCnt>INTEL_SMBUS_MAX_DATA_LOAD)
    {
        Printf(Tee::PriError, "Invalid byteCnt = 0x%x", byteCnt);
        return RC::ILWALID_INPUT;
    }

    // A read of the control register clears the internal index pointer
    // for block data
    UINT32 cntrl;
    CHECK_RC(RegRd(INTEL_SMBUS_REG_CNT, &cntrl));

    // Set the ByteCont Register
    CHECK_RC( RegWr(INTEL_SMBUS_REG_BCNT,byteCnt) );

    // Set the Data Array Register
    for (UINT08 i = 0; i<byteCnt; i++)
    {
        CHECK_RC( RegWr(INTEL_SMBUS_REG_HST_BLOCK_DB,  (*pData)[i]) );
    }

    LOG_EXT();
    return OK;
}

RC IntelSmbPort::GetData(vector<UINT08> *pData, UINT08 Protocol)
{
    LOG_ENT();
    RC rc;
    UINT32 regval;

    if (!pData)
    {
        Printf(Tee::PriError, "IntelSmbPort::GetData - Invalid pData");
        return RC::ILWALID_INPUT;
    }

    //for Read Byte and Read Word
    if ((Protocol == INTEL_PRTCL_RCV_BYTE) ||
        (Protocol == INTEL_PRTCL_RD_BYTE) ||
        (Protocol == INTEL_PRTCL_RD_WORD))
    {
        if (((Protocol != INTEL_PRTCL_RD_WORD) && (pData->size() != 1)) ||
            ((Protocol == INTEL_PRTCL_RD_WORD) && (pData->size() != 2)))
        {
            Printf(Tee::PriError, "Invalid byteCnt = 0x%x for protocol = 0x%x",
                    static_cast<UINT32>(pData->size()),
                    Protocol);
            return RC::ILWALID_INPUT;
        }

        CHECK_RC(RegRd(INTEL_SMBUS_REG_HST_D0,&regval));
        (*pData)[0]=regval;

        if (Protocol == INTEL_PRTCL_RD_WORD)
        {
            CHECK_RC(RegRd(INTEL_SMBUS_REG_HST_D1,&regval));
            (*pData)[1]=regval;
        }
        return rc;
    }

    UINT32 byteCnt = 0;
    // For Read Blk and ProCalls
    // Set the ByteCont Register
    CHECK_RC(RegRd(INTEL_SMBUS_REG_BCNT, &byteCnt));

    if ((byteCnt==0)||(byteCnt>INTEL_SMBUS_MAX_DATA_LOAD))
    {
        Printf(Tee::PriError, "Invalid byteCnt = 0x%x", byteCnt);
        return RC::UNEXPECTED_RESULT;
    }

    // A read of the control register clears the internal index pointer
    // for block data
    UINT32 cntrl;
    CHECK_RC(RegRd(INTEL_SMBUS_REG_CNT, &cntrl));

    // Set the Data Array Register
    for (UINT08 i = 0; i<byteCnt; i++)
    {
        CHECK_RC(RegRd(INTEL_SMBUS_REG_HST_BLOCK_DB,&regval));
        pData->push_back(regval);
    }

    LOG_EXT();
    return OK;
}

bool IntelSmbPort::IsBusy()
{
    UINT32 prt;

    if(OK!=RegRd(INTEL_SMBUS_REG_STS,&prt))
        return true;

    if(prt & INTEL_SMBUS_STS_BUSY)
        return true;
    else
        return false;
}

RC IntelSmbPort::SetAddr(UINT08 Addr, UINT08 ReadBit)
{
    RC rc;
    if (Addr>0x7f)
    {
        Printf(Tee::PriError, "Invalid slave Addr = 0x%x", Addr);
        return RC::ILWALID_INPUT;
    }

    CHECK_RC(RegWr(INTEL_SMBUS_REG_ADDR,
                    ((Addr<<INTEL_ADDR_SHIFT) + ReadBit)));

    return OK;
}

RC IntelSmbPort::SetPrtcl(UINT08 Prt, bool IsPec)
{
    if (Prt>INTEL_PRTCL_UNSUPPORTED)
    {
        Printf(Tee::PriError, "Invalid Protocol = 0x%x", Prt);
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc;
    UINT08 cntrl = INTEL_PRTCL_START;

    if (IsPec)
        cntrl |= INTEL_PRTCL_PEC_EN;

    switch(Prt)
    {
        case INTEL_PRTCL_SND_BYTE:
        case INTEL_PRTCL_RCV_BYTE:
            cntrl |= INTEL_PRTCL_BYTE;
            break;

        case INTEL_PRTCL_WR_BYTE:
        case INTEL_PRTCL_RD_BYTE:
            cntrl |= INTEL_PRTCL_BYTE_DATA;
            break;

        case INTEL_PRTCL_WR_WORD:
        case INTEL_PRTCL_RD_WORD:
            cntrl |= INTEL_PRTCL_WORD_DATA;
            break;

        case INTEL_PRTCL_PROCESS_CALL:
            cntrl |= INTEL_PRTCL_PROC;
            break;

        case INTEL_PRTCL_WR_BLK:
        case INTEL_PRTCL_RD_BLK:
            cntrl |= INTEL_PRTCL_BLOCK;
            break;

        default:
            //Quick command, don't need to modify cntrl
            break;
    }

    CHECK_RC(RegWr(INTEL_SMBUS_REG_CNT, cntrl));

    return OK;
}

RC IntelSmbPort::WaitCycDone(UINT32 TimeoutMs)
{
    McpDev::PollIoArgs args;
    args.ioAddr = m_CtrlBase+INTEL_SMBUS_REG_STS;
    args.exp=INTEL_STS_DONE_OK;
    args.unexp=0;
    return Tasker::Poll(McpDev::PollIo, (void *)(&args), TimeoutMs);
}

RC IntelSmbPort::CheckError()
{
    UINT32 sta;
    RegRd(INTEL_SMBUS_REG_STS,&sta);

    if ((sta&INTEL_STS_DONE_OK)!=INTEL_STS_DONE_OK)
    {
        Printf(Tee::PriHigh,"*** ERROR: IntelSmbPort::CheckError - Status = 0x%04x ***\n", sta );
        return RC::CMD_STATUS_ERROR;
    }
    return OK;
}

RC IntelSmbPort::Protocol(UINT16 SlvAddr,UINT08 Protocol,vector<UINT08> *pDataWr,
                          vector<UINT08> *pDataRd,UINT08 Cmd)
{
    LOG_ENT();
    RC rc;
    UINT08 ReadBit = INTEL_ADDR_READ;

    if ((Protocol<INTEL_PRTCL_WR_QUICK)||(Protocol>INTEL_PRTCL_PROCESS_CALL))
    {
        Printf(Tee::PriError, "Invalid Protocol 0x%x", Protocol);
        return RC::BAD_PARAMETER;
    }

    Uninitialize();

    // Check is busy
    if (IsBusy())
    {
        Printf(Tee::PriHigh, "IntelSmbPort::Protocol() - Device Busy\n");
        return RC::CMD_STATUS_ERROR;
    }

    if ((Protocol == INTEL_PRTCL_WR_BLK) ||
        (Protocol == INTEL_PRTCL_RD_BLK))
    {
        CHECK_RC(RegWr(INTEL_SMBUS_REG_AUX_CTL, INTEL_AUXCTL_32B_BUF_EN));
    }
    else
    {
        CHECK_RC(RegWr(INTEL_SMBUS_REG_AUX_CTL, 0));
    }

    switch (Protocol)
    {
        case INTEL_PRTCL_SND_BYTE:
        case INTEL_PRTCL_WR_BYTE:
        case INTEL_PRTCL_WR_WORD:
        case INTEL_PRTCL_WR_BLK:
            // Need to load up data.
            CHECK_RC(SetData(pDataWr, Protocol));
            // FALLTHROUGH

        case INTEL_PRTCL_WR_QUICK:
            ReadBit = 0;
            // FALLTHROUGH

        case INTEL_PRTCL_RD_QUICK:
        case INTEL_PRTCL_RCV_BYTE:
        case INTEL_PRTCL_RD_BYTE:
        case INTEL_PRTCL_RD_WORD:
        case INTEL_PRTCL_RD_BLK:
            if (Protocol == INTEL_PRTCL_RD_BLK)
            {
                CHECK_RC( RegWr(INTEL_SMBUS_REG_BCNT,
                                static_cast<UINT32>(pDataRd->size())) );
            }
            CHECK_RC(SetAddr(SlvAddr, ReadBit));
            CHECK_RC(RegWr(INTEL_SMBUS_REG_CMD,Cmd));
            break;

        default:
            Printf(Tee::PriError, "Invalid Protocol 0x%x", Protocol);
            return RC::BAD_PARAMETER;
    }

    // Set protocol (start the cycle)
    CHECK_RC(SetPrtcl(Protocol, false));

    // Wait till done
    CHECK_RC(WaitCycDone(25));

    // Check the status
    CHECK_RC(CheckError());

    // Get data back
    switch (Protocol)
    {
        case INTEL_PRTCL_RCV_BYTE:
        case INTEL_PRTCL_RD_BYTE:
            pDataRd->resize(1);
            CHECK_RC(GetData(pDataRd, Protocol));
            break;

        case INTEL_PRTCL_RD_WORD:
            pDataRd->resize(2);
            CHECK_RC(GetData(pDataRd, Protocol));
            break;

        case INTEL_PRTCL_RD_BLK:
            pDataRd->clear();
            CHECK_RC(GetData(pDataRd, Protocol));
            if (pDataRd->size()>0x20)
            {
                Printf(Tee::PriError, "Invalid RdBlk Size 0x%x", (UINT32)pDataRd->size());
                rc = RC::UNEXPECTED_RESULT;
            }
            break;
    }

    // Print the debug info
    if (pDataWr)
    {
        Printf(Tee::PriDebug, "DataWr:\n");
        VectorData::Print (pDataWr, Tee::PriDebug);
    }

    if (pDataRd)
    {
        Printf(Tee::PriDebug, "DataRd:\n");
        VectorData::Print (pDataRd, Tee::PriDebug);
    }

    LOG_EXT();
    return rc;
}

//protocols
RC IntelSmbPort::WrQuickCmd(UINT16 SlvAddr)
{
    RC rc;
    CHECK_RC(Protocol(SlvAddr,INTEL_PRTCL_WR_QUICK,NULL,NULL,0));
    return OK;
}

RC IntelSmbPort::RdQuickCmd(UINT16 SlvAddr)
{
    RC rc;
    CHECK_RC(Protocol(SlvAddr,INTEL_PRTCL_RD_QUICK,NULL,NULL,0));
    return OK;
}

RC IntelSmbPort::SendByteCmd(UINT16 SlvAddr, UINT08 Data)
{
    RC rc;
    vector<UINT08> vData(1, Data);
    CHECK_RC(Protocol(SlvAddr,INTEL_PRTCL_SND_BYTE,&vData,NULL,0));
    return OK;
}

RC IntelSmbPort::RecvByteCmd(UINT16 SlvAddr, UINT08 * pData)
{
    RC rc;
    vector<UINT08> vData(1);
    CHECK_RC(Protocol(SlvAddr,INTEL_PRTCL_RCV_BYTE,NULL,&vData,0));
    (*pData) = vData[0];
    return OK;
}

RC IntelSmbPort::WrByteCmd(UINT16 SlvAddr, UINT08 CmdCode, UINT08 Data)
{
    RC rc;
    vector<UINT08> vData(1, Data);
    CHECK_RC(Protocol(SlvAddr,INTEL_PRTCL_WR_BYTE,&vData,NULL,CmdCode));
    return OK;
}

RC IntelSmbPort::RdByteCmd(UINT16 SlvAddr, UINT08 CmdCode, UINT08 * pData)
{
    RC rc;
    vector<UINT08> vData(1);

    CHECK_RC(Protocol(SlvAddr,INTEL_PRTCL_RD_BYTE,NULL,&vData,CmdCode));
    *pData = vData[0];
    return OK;
}

RC IntelSmbPort::WrWordCmd(UINT16 SlvAddr, UINT08 CmdCode, UINT16 Data)
{
    RC rc;
    vector<UINT08> vData(2);
    vData[0] = Data&0xff;
    vData[1] = Data>>8;

    CHECK_RC(Protocol(SlvAddr,INTEL_PRTCL_WR_WORD,&vData,NULL,CmdCode));
    return OK;
}

RC IntelSmbPort::RdWordCmd(UINT16 SlvAddr, UINT08 CmdCode, UINT16 * pData)
{
    RC rc;
    vector<UINT08> vData(2);

    CHECK_RC(Protocol(SlvAddr,INTEL_PRTCL_RD_WORD,NULL,&vData,CmdCode));
    *pData = (vData[1]<<8) | vData[0];
    return OK;
}

RC IntelSmbPort::WrBlkCmd(UINT16 SlvAddr, UINT08 CmdCode,vector<UINT08> * pData)
{
    RC rc;
    CHECK_RC(Protocol(SlvAddr,INTEL_PRTCL_WR_BLK,pData,NULL,CmdCode));
    return OK;
}

RC IntelSmbPort::RdBlkCmd(UINT16 SlvAddr, UINT08 CmdCode,vector<UINT08> * pData)
{
    RC rc;
    CHECK_RC( Protocol(SlvAddr,INTEL_PRTCL_RD_BLK,NULL,pData,CmdCode));
    return OK;
}

RC IntelSmbPort::ProcCallCmd(UINT16 SlvAddr, UINT08 CmdCode,UINT16 DataWr, UINT16 * pDataRd)
{
    RC rc;

    vector<UINT08> vDataWr(2);
    vector<UINT08> vDataRd(2);
    vDataWr[0] = DataWr&0xff;
    vDataWr[1] = DataWr>>8;

    CHECK_RC(Protocol(SlvAddr,INTEL_PRTCL_PROCESS_CALL,&vDataWr,&vDataRd,CmdCode));

    *pDataRd = (vDataRd[1]<<8) | vDataRd[0];

    return OK;
}

RC IntelSmbPort::ProcCallBlkCmd(UINT16 SlvAddr, UINT08 CmdCode,vector<UINT08> * pDataWr, vector<UINT08> * pDataRd)
{
    Printf(Tee::PriError, "ProcessCallBlk is not supported in Smbus2.0");
    return OK;
}

RC IntelSmbPort::HostNotify(UINT08 DvcAdrs, UINT16 Data)
{
    Printf(Tee::PriError, "HostNotify is not supported in IntelSmbPort");
    return RC::SOFTWARE_ERROR;
}

RC IntelSmbPort::Search()
{
    LOG_ENT();
    RC rc;
    UINT16 slvAddr;
    UINT08 junk;

    m_vSlvAddr.clear();

    for(slvAddr = 0; slvAddr < MAX_SLAVES ; slvAddr++)
    {
        if(OK == (rc = RdByteCmd(slvAddr, 0, &junk)))
        {
            m_vSlvAddr.push_back(slvAddr);
        }
    }

    if(m_vSlvAddr.size())
    {
        Printf(Tee::PriNormal, "\tSmbusPort_%i is Connected with Slave Addresses:\n\t\t",
               m_Index);
        for(UINT32 i = 0; i < m_vSlvAddr.size(); i++)
        {
            Printf(Tee::PriNormal, "0x%02x ", (m_vSlvAddr)[i]);
        }
        Printf(Tee::PriNormal, "\n");
    }
    else
        Printf(Tee::PriNormal, "\tSmbusPort_%i is NOT Connected with any Slave Devices\n",
               m_Index);

    m_IsSearched = true;
    LOG_EXT();
    return OK;
}

RC IntelSmbPort::IsSlavePresent (UINT16 SlvAddr)
{
    LOG_ENT();
    RC rc;
    UINT08 junk;

    if(OK == (rc = RdByteCmd(SlvAddr, 0, &junk)))
    {
        Printf(Tee::PriNormal, "Slave Address 0x%04x is CONNECTED", SlvAddr);
    }
    else
    {
        Printf(Tee::PriError, "Slave Address 0x%04x is NOT CONNECTED", SlvAddr);
        return RC::UNEXPECTED_RESULT;
    }
    return OK;
}

RC IntelSmbPort::GetSlaveAddrs(vector<UINT08> *pvSlvAddr)
{
    LOG_ENT();
    if (!m_IsSearched)
    {
        Printf(Tee::PriError, "Smb port not searched");
        return RC::WAS_NOT_INITIALIZED;
    }
    if(!pvSlvAddr)
    {
        Printf(Tee::PriError, "Null pointer");
        return RC::ILWALID_INPUT;
    }
    for (UINT32 i = 0; i < m_vSlvAddr.size(); i++)
    {
        pvSlvAddr -> push_back((m_vSlvAddr)[i]);
    }
    LOG_EXT();
    return OK;
}

