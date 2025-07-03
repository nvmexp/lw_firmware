/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2010,2012,2015-2017,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file lwsmbpt.cpp
//! \brief Implementation of class for LW Smbus ports
//!
//! Contains class implementation for LW Smbus ports
//!

#include "lwsmbpt.h"
#include "core/include/platform.h"
#include "core/include/tee.h"
#include "smbreg.h"
#include "core/include/tasker.h"
#include "core/include/data.h"
#include "cheetah/include/logmacros.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME  "LwSmbPort"

LwSmbPort::LwSmbPort(SmbDevice *pSmbDev, UINT32 Index, UINT32 CtrlBase)
    : SmbPort(pSmbDev, Index, CtrlBase)
    , m_CtrlBase(CtrlBase)
    , m_Index(Index)
    , m_IsSearched(false)
{
    LOG_ENT();
    LOG_EXT();
}

LwSmbPort::~LwSmbPort() //destructor
{
    Uninitialize();
}

RC LwSmbPort::RegRd(UINT32 Reg, UINT32 *pData, bool IsDebug)
{
    if(!pData)
    {
        Printf(Tee::PriError, "LwSmbPort::RegRd - pData argument is null pointer");
        return RC::ILWALID_INPUT;
    }
    if(Reg<=SMBUS_REG_SNPADDR)
        return Platform::PioRead08(m_CtrlBase+Reg,(UINT08 *)pData);
    else if(Reg<=SMBUS_REG_CTRL)
        return Platform::PioRead16(m_CtrlBase+Reg,(UINT16 *)pData);
    else
    {
        return RC::ILWALID_REGISTER_NUMBER;
    }
}

RC LwSmbPort::RegWr(UINT32 Reg, UINT32 Data, bool IsDebug)
{
    if(Reg<=SMBUS_REG_SNPADDR)
        return Platform::PioWrite08(m_CtrlBase+Reg,(UINT08)Data);
    else if(Reg<=SMBUS_REG_CTRL)
        return Platform::PioWrite16(m_CtrlBase+Reg,(UINT16)Data);
    else
    {
        return RC::ILWALID_REGISTER_NUMBER;
    }
}

RC LwSmbPort::Initialize()
{
    RC rc;

    m_IsSearched = false;
    //Disable Interrupts
    CHECK_RC(RegWr(SMBUS_REG_CTRL, 0));
    CHECK_RC(ResetCtrlReg());
    m_IsInit = true;
    return OK;
}

RC LwSmbPort::ResetCtrlReg()
{
    LOG_ENT();

    RC rc;

    CHECK_RC(Uninitialize());
    //Init reg
    UINT32 reg;
    CHECK_RC(RegRd(SMBUS_REG_HASADDR,&reg));
    if(reg!=(SMBUS_DEFAULT_HOST_ADRS<<ADDR_SHIFT))
    {
        CHECK_RC(RegWr(SMBUS_REG_HASADDR, SMBUS_DEFAULT_HOST_ADRS<<ADDR_SHIFT));
    }
    CHECK_RC(RegRd(SMBUS_REG_SNPADDR,&reg));
    if(reg!=(SMBUS_DEFAULT_HOST_ADRS<<ADDR_SHIFT))
    {
        CHECK_RC(RegWr(SMBUS_REG_SNPADDR, SMBUS_DEFAULT_HOST_ADRS<<ADDR_SHIFT));
    }
    LOG_EXT();
    return rc;
}

RC LwSmbPort::Uninitialize()
{
    LOG_ENT();
    RC rc;

    //clear alarm
    CHECK_RC(RegWr(SMBUS_REG_STS,0));
    CHECK_RC(RegWr(SMBUS_REG_ALARM_DATA_HI,0));
    CHECK_RC(RegWr(SMBUS_REG_ALARM_DATA_LO,0));

    //clear byte count
    CHECK_RC(RegWr(SMBUS_REG_BCNT,0));

    //clear command
    CHECK_RC(RegWr(SMBUS_REG_CMD,0));

    //clear data array
    for(UINT08 i=0;i<SMBUS_MAX_DATA_LOAD;i++)
    {
        CHECK_RC(RegWr(SMBUS_REG_DATA_BASE+i,0));
    }

    m_IsInit = false;
    LOG_EXT();
    return OK;
}

RC LwSmbPort::PrintInfo(Tee::Priority Pri)
{
    RC rc;
    UINT32 status = 0;

    CHECK_RC(RegRd(SMBUS_REG_STS,&status));
    Printf(Pri, "Port_%i IO_CtrlBase: 0x%04x\n", m_Index, m_CtrlBase);
    Printf(Pri, "Port_%i Status: 0x%02x\n", m_Index, status);

    switch (status&STS_STATUS_MASK)
    {
        case STS_STATUS_OK:
            Printf(Pri, "\tSTATUS_OK\n");
            break;

        case STS_STATUS_NAK:
            Printf(Pri, "\tSTATUS_NAK\n");
            break;

        case STS_STATUS_TIMEOUT:
            Printf(Pri, "\tSTATUS_TIMEOUT\n");
            break;

        case STS_STATUS_UNSUP_PRTCL:
            Printf(Pri, "\tSTATUS_UNSUP_PRTCL\n");
            break;

        case STS_STATUS_SMBUS_BUSY:
            Printf(Pri, "\tSTATUS_BUSY\n");
            break;

        case STS_STATUS_PEC_ERROR:
            Printf(Pri, "\tSTATUS_ERROR\n");
            break;

        default:
            Printf(Pri, "\tIlwalid Status.\n");
    }

    if (status&STS_ALARM_ACTIVE)
        Printf(Pri, "\tALARM_ACTIVE.\n");
    else
        Printf(Pri, "\tALARM_INACTIVE/CLEAR.\n");

    if (status&STS_DONE_OK)
        Printf(Pri, "\tDONE_OK.\n");
    else
        Printf(Pri, "\tDONE_NOTOK.\n");
    return OK;
}

RC LwSmbPort::PrintReg(Tee::Priority Pri)
{
    RC rc;
    UINT32 reg;

    Printf(Pri, "Port_%i IO_CtrlBase: 0x%04x\n", m_Index, m_CtrlBase);

    CHECK_RC(RegRd(SMBUS_REG_PRTCL,&reg));
    Printf(Pri,"\n\tSMBUS_REG_PRTCL:\t0x%08x",reg);
    CHECK_RC(RegRd(SMBUS_REG_STS,&reg));
    Printf(Pri,"\n\tSMBUS_REG_STS:\t\t0x%08x",reg);
    CHECK_RC(RegRd(SMBUS_REG_ADDR,&reg));
    Printf(Pri,"\n\tSMBUS_REG_ADDR:\t\t0x%08x",reg);
    CHECK_RC(RegRd(SMBUS_REG_CMD,&reg));
    Printf(Pri,"\n\tSMBUS_REG_CMD:\t\t0x%08x",reg);
    CHECK_RC(RegRd(SMBUS_REG_DATA_BASE,&reg));
    Printf(Pri,"\n\tSMBUS_REG_DATA_BASE:\t0x%08x",reg);
    CHECK_RC(RegRd(SMBUS_REG_BCNT,&reg));
    Printf(Pri,"\n\tSMBUS_REG_BCNT:\t\t0x%08x",reg);
    CHECK_RC(RegRd(SMBUS_REG_ALARM_ADDR,&reg));
    Printf(Pri,"\n\tSMBUS_REG_ALARM_ADDR:\t0x%08x",reg);
    CHECK_RC(RegRd(SMBUS_REG_ALARM_DATA_LO,&reg));
    Printf(Pri,"\n\tSMBUS_REG_ALARM_DATA_LO:0x%08x",reg);
    CHECK_RC(RegRd(SMBUS_REG_ALARM_DATA_HI,&reg));
    Printf(Pri,"\n\tSMBUS_REG_ALARM_DATA_HI:0x%08x",reg);
    return rc;
}

RC LwSmbPort::SetData(vector<UINT08> *pData)
{
    LOG_ENT();
    RC rc;

    if (!pData)
    {
        Printf(Tee::PriError, "LwSmbPort::SetData - Invalid pData");
        return RC::ILWALID_INPUT;
    }

    UINT08 byteCnt = pData->size();

    if (byteCnt>SMBUS_MAX_DATA_LOAD)
    {
        Printf(Tee::PriError, "Invalid byteCnt = 0x%x", byteCnt);
        return RC::ILWALID_INPUT;
    }

    // Set the ByteCont Register
    CHECK_RC( RegWr(SMBUS_REG_BCNT,byteCnt) );

    // Set the Data Array Register
    for (UINT08 i = 0; i<byteCnt; i++)
    {
        CHECK_RC( RegWr(SMBUS_REG_DATA_BASE+i,  (*pData)[i]) );
    }

    LOG_EXT();
    return OK;
}

RC LwSmbPort::GetData(vector<UINT08> *pData)
{
    LOG_ENT();
    RC rc;
    UINT32 regval;

    if (!pData)
    {
        Printf(Tee::PriError, "LwSmbPort::GetData - Invalid pData");
        return RC::ILWALID_INPUT;
    }

    UINT32 byteCnt = 0;

    //for Read Byte and Read Word
    if(pData->size()!=0)
    {
        //Set the Data Array Register
        for(UINT08 i=0; i<pData->size(); i++)
        {
            CHECK_RC(RegRd(SMBUS_REG_DATA_BASE+i,&regval));
            (*pData)[i]=regval;
        }

        return OK;
    }

    // For Read Blk and ProCalls
    // Set the ByteCont Register
    CHECK_RC(RegRd(SMBUS_REG_BCNT,&byteCnt));

    if ((byteCnt==0)||(byteCnt>SMBUS_MAX_DATA_LOAD))
    {
        Printf(Tee::PriError, "Invalid byteCnt = 0x%x", byteCnt);
        return RC::UNEXPECTED_RESULT;
    }

    // Set the Data Array Register
    for (UINT08 i = 0; i<byteCnt; i++)
    {
        CHECK_RC(RegRd(SMBUS_REG_DATA_BASE+i,&regval));
        pData->push_back(regval);
    }
    LOG_EXT();
    return OK;
}

bool LwSmbPort::IsBusy()
{
    UINT32 prt = 0;

    if(OK!=RegRd(SMBUS_REG_PRTCL,&prt))
        return true;

    if(prt)
        return true;
    else
        return false;
}

RC LwSmbPort::SetAddr(UINT08 Addr)
{
    RC rc;
    if (Addr>0x7f)
    {
        Printf(Tee::PriError, "Invalid slave Addr = 0x%x", Addr);
        return RC::ILWALID_INPUT;
    }

    CHECK_RC(RegWr(SMBUS_REG_ADDR, Addr<<ADDR_SHIFT));

    return OK;
}

RC LwSmbPort::SetPrtcl(UINT08 Prt, bool IsPec)
{
    if (Prt>PRTCL_UNSUPPORTED)
    {
        Printf(Tee::PriError, "Invalid Protocol = 0x%x", Prt);
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc;
    UINT08 prtcl = (Prt&PRTCL_PROTOCAL_MASK);
    // Set PEC_ENable bit
    if (IsPec)
        prtcl |= PRTCL_PEC_EN;
    else
        prtcl&=(~PRTCL_PEC_EN);

    CHECK_RC(RegWr(SMBUS_REG_PRTCL,prtcl));

    return OK;
}

RC LwSmbPort::WaitCycDone(UINT32 TimeoutMs)
{
    McpDev::PollIoArgs args;
    args.ioAddr = m_CtrlBase+SMBUS_REG_PRTCL;
    args.exp=0;
    args.unexp=PRTCL_PROTOCAL_MASK;
    return POLLWRAP(McpDev::PollIo, (void *)(&args), TimeoutMs);
}

RC LwSmbPort::CheckError()
{
    UINT32 sta;
    RegRd(SMBUS_REG_STS,&sta);

    if (((sta & STS_STATUS_MASK) != STS_STATUS_OK) ||
        ((sta & STS_DONE_OK) != STS_DONE_OK))
    {
        Printf(Tee::PriHigh,"*** ERROR: LwSmbPort::CheckError - Status = 0x%04x ***\n", sta );
        return RC::CMD_STATUS_ERROR;
    }
    return OK;
}

RC LwSmbPort::Protocol(UINT16 SlvAddr,UINT08 Protocol,vector<UINT08> *pDataWr,vector<UINT08> *pDataRd,UINT08 Cmd)
{
    LOG_ENT();
    RC rc;

    if ((Protocol<PRTCL_WR_QUICK)||(Protocol>PRTCL_PROCESS_CALL))
    {
        Printf(Tee::PriError, "Invalid Protocol 0x%x", Protocol);
        return RC::BAD_PARAMETER;
    }

    Uninitialize();

    // Check is busy
    if (IsBusy())
    {
        Printf(Tee::PriError, "LwSmbPort::Protocol() - Device Busy\n");
        return RC::CMD_STATUS_ERROR;
    }

    switch (Protocol)
    {
        case PRTCL_SND_BYTE:
        case PRTCL_WR_BYTE:
        case PRTCL_WR_WORD:
        case PRTCL_WR_BLK:
            // Need to load up data.
            CHECK_RC(SetData(pDataWr));
            // FALLTHROUGH
        case PRTCL_RD_QUICK:
        case PRTCL_WR_QUICK:
        case PRTCL_RCV_BYTE:
        case PRTCL_RD_BYTE:
        case PRTCL_RD_WORD:
        case PRTCL_RD_BLK:
            CHECK_RC(SetAddr(SlvAddr));
            CHECK_RC(RegWr(SMBUS_REG_CMD,Cmd));
            break;

        default:
            Printf(Tee::PriError, "Invalid Protocol 0x%x", Protocol);
            return RC::BAD_PARAMETER;
    }

    // Set protocol (start the cycle)
    CHECK_RC(SetPrtcl(Protocol, false));

    // Wait till done
    CHECK_RC(WaitCycDone(1000));

    // Check the status
    CHECK_RC(CheckError());

    // Get data back
    switch (Protocol)
    {
        case PRTCL_RCV_BYTE:
        case PRTCL_RD_BYTE:
            pDataRd->resize(1);
            CHECK_RC(GetData(pDataRd));
            break;

        case PRTCL_RD_WORD:
            pDataRd->resize(2);
            CHECK_RC(GetData(pDataRd));
            break;

        case PRTCL_RD_BLK:
            pDataRd->clear();
            CHECK_RC(GetData(pDataRd));
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
RC LwSmbPort::WrQuickCmd(UINT16 SlvAddr)
{
    RC rc;
    CHECK_RC(Protocol(SlvAddr,PRTCL_WR_QUICK,NULL,NULL,0));
    return OK;
}

RC LwSmbPort::RdQuickCmd(UINT16 SlvAddr)
{
    RC rc;
    CHECK_RC(Protocol(SlvAddr,PRTCL_RD_QUICK,NULL,NULL,0));
    return OK;
}

RC LwSmbPort::SendByteCmd(UINT16 SlvAddr, UINT08 Data)
{
    RC rc;
    vector<UINT08> vData(1, Data);
    CHECK_RC(Protocol(SlvAddr,PRTCL_SND_BYTE,&vData,NULL,0));
    return OK;
}

RC LwSmbPort::RecvByteCmd(UINT16 SlvAddr, UINT08 * pData)
{
    RC rc;
    vector<UINT08> vData(1);
    CHECK_RC(Protocol(SlvAddr,PRTCL_RCV_BYTE,NULL,&vData,0));
    (*pData) = vData[0];
    return OK;
}

RC LwSmbPort::WrByteCmd(UINT16 SlvAddr, UINT08 CmdCode, UINT08 Data)
{
    RC rc;
    vector<UINT08> vData(1, Data);
    CHECK_RC(Protocol(SlvAddr,PRTCL_WR_BYTE,&vData,NULL,CmdCode));
    return OK;
}

RC LwSmbPort::RdByteCmd(UINT16 SlvAddr, UINT08 CmdCode, UINT08 * pData)
{
    RC rc;
    vector<UINT08> vData(1);

    CHECK_RC(Protocol(SlvAddr,PRTCL_RD_BYTE,NULL,&vData,CmdCode));
    *pData = vData[0];
    return OK;
}

RC LwSmbPort::WrWordCmd(UINT16 SlvAddr, UINT08 CmdCode, UINT16 Data)
{
    RC rc;
    vector<UINT08> vData(2);
    vData[0] = Data&0xff;
    vData[1] = Data>>8;

    CHECK_RC(Protocol(SlvAddr,PRTCL_WR_WORD,&vData,NULL,CmdCode));
    return OK;
}

RC LwSmbPort::RdWordCmd(UINT16 SlvAddr, UINT08 CmdCode, UINT16 * pData)
{
    RC rc;
    vector<UINT08> vData(2);

    CHECK_RC(Protocol(SlvAddr,PRTCL_RD_WORD,NULL,&vData,CmdCode));
    *pData = (vData[1]<<8) | vData[0];
    return OK;
}

RC LwSmbPort::WrBlkCmd(UINT16 SlvAddr, UINT08 CmdCode,vector<UINT08> * pData)
{
    RC rc;
    CHECK_RC(Protocol(SlvAddr,PRTCL_WR_BLK,pData,NULL,CmdCode));
    return OK;
}

RC LwSmbPort::RdBlkCmd(UINT16 SlvAddr, UINT08 CmdCode,vector<UINT08> * pData)
{
    RC rc;
    CHECK_RC( Protocol(SlvAddr,PRTCL_RD_BLK,NULL,pData,CmdCode));
    return OK;
}

RC LwSmbPort::ProcCallCmd(UINT16 SlvAddr, UINT08 CmdCode,UINT16 DataWr, UINT16 * pDataRd)
{
    RC rc;

    vector<UINT08> vDataWr(2);
    vector<UINT08> vDataRd(2);
    vDataWr[0] = DataWr&0xff;
    vDataWr[1] = DataWr>>8;

    CHECK_RC(Protocol(SlvAddr,PRTCL_PROCESS_CALL,&vDataWr,&vDataRd,CmdCode));

    *pDataRd = (vDataRd[1]<<8) | vDataRd[0];

    return OK;
}

RC LwSmbPort::ProcCallBlkCmd(UINT16 SlvAddr, UINT08 CmdCode,vector<UINT08> * pDataWr, vector<UINT08> * pDataRd)
{
    Printf(Tee::PriError, "ProcessCallBlk is not supported in Smbus2.0");
    return OK;
}

RC LwSmbPort::HostNotify(UINT08 DvcAdrs, UINT16 Data)
{
    LOG_ENT();
    RC rc;
    UINT32 dataRd = 0, alarmAddr = 0;

    CHECK_RC(WrWordCmd(SMBUS_DEFAULT_HOST_ADRS, DvcAdrs, Data));

    // Check Alarm Status
    UINT32 sta;
    CHECK_RC(RegRd(SMBUS_REG_STS,&sta));

    if ((sta&STS_ALARM_ACTIVE)!=STS_ALARM_ACTIVE )
    {
        Printf(Tee::PriError, "Alarm not active, Status = 0x%2x", sta);
        PrintInfo();
        return RC::UNEXPECTED_RESULT;
    }

    //Check Result
    CHECK_RC(RegRd(SMBUS_REG_ALARM_ADDR,&alarmAddr));
    UINT32 tempData = 0;
    CHECK_RC(RegRd(SMBUS_REG_ALARM_DATA_HI,&tempData));
    CHECK_RC(RegRd(SMBUS_REG_ALARM_DATA_LO,&dataRd));
    dataRd|=tempData<<8;

    Printf(Tee::PriDebug, "AlarmAddr 0x%x, DataRd 0x%x.\n", alarmAddr, dataRd);

    if (alarmAddr != DvcAdrs)
    {
        Printf(Tee::PriError, "Address Mismatch");
        rc = RC::DATA_MISMATCH;
    }

    if (dataRd != Data)
    {
        Printf(Tee::PriError, "Data Mismatch");
        rc = RC::DATA_MISMATCH;
    }

    Printf(Tee::PriNormal, "Port_%i HostNotify(SlvAdrs=0x%x, Data=0x%x) \n",
           m_Index, DvcAdrs, Data);
    if(OK==rc)
    {
        Printf(Tee::PriNormal, "$$$ PASSED $$$");
    }
    else
    {
        Printf(Tee::PriNormal, "*** FAILED ***");
    }
    Printf(Tee::PriNormal, " %s::%s()\n", CLASS_NAME, FUNCNAME);
    LOG_EXT();
    return rc;
}

RC LwSmbPort::Search()
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

RC LwSmbPort::IsSlavePresent (UINT16 SlvAddr)
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

RC LwSmbPort::GetSlaveAddrs(vector<UINT08> *pvSlvAddr)
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

