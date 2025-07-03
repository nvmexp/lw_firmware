/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file amdsmbpt.h
//! \brief Header file with class for AMD Smbus port
//!
//! Contains class definition for AMD Smbus port. Compatible with the KERNCZ
//! SMBus controller used by AMD Family 17h (Zen/Zen+/Zen2). AMD Documentation:
//! BIOS and Kernel Developer’s Guide (BKDG) for AMD Family 15h Models 70h-7Fh Processors
//!

#ifndef INCLUDED_AMD_SMBPORT_H
#define INCLUDED_AMD_SMBPORT_H

#include "core/include/rc.h"
#include "smbport.h"

#ifndef INCLUDED_TEE_H
    #include "core/include/tee.h"
#endif
#ifndef INCLUDED_STL_VECTOR
    #include <vector>
    #define INCLUDED_STL_VECTOR
#endif
#ifndef INCLUDED_SMBDEV_H
    #include "smbdev.h"
#endif

//! \brief Class definition for AMD Smbus ports
//-----------------------------------------------------------------------------

#define MAX_DEVICES 128
#define MAX_RETRIES 500

class AmdSmbPort : public SmbPort
{
public:
    AmdSmbPort(SmbDevice *pSmbDev, UINT32 PortIndex, UINT32 CtrlBase);
    ~AmdSmbPort();
    RC Initialize();
    RC Uninitialize();
    RC RegRd(UINT32 Reg, UINT32 *pData, bool IsDebug = false);
    RC RegWr(UINT32 Reg, UINT32 Data, bool IsDebug = false);
    RC PrintInfo(Tee::Priority Pri = Tee::PriNormal);
    RC PrintReg(Tee::Priority Pri = Tee::PriNormal);
    RC WrQuickCmd(UINT16 SlvAdrs);
    RC RdQuickCmd(UINT16 SlvAdrs);
    RC SendByteCmd(UINT16 SlvAdrs, UINT08 Data);
    RC RecvByteCmd(UINT16 SlvAdrs, UINT08 *pData);
    RC WrByteCmd(UINT16 SlvAdrs, UINT08 CmdCode, UINT08 Data);
    RC RdByteCmd(UINT16 SlvAdrs, UINT08 CmdCode, UINT08 *pData);
    RC WrWordCmd(UINT16 SlvAdrs, UINT08 CmdCode, UINT16 Data);
    RC RdWordCmd(UINT16 SlvAdrs, UINT08 CmdCode, UINT16 *pData);
    RC WrBlkCmd(UINT16 SlvAdrs, UINT08 CmdCode,vector<UINT08> *pData);
    RC RdBlkCmd(UINT16 SlvAdrs, UINT08 CmdCode,vector<UINT08> *pData);
    RC ProcCallCmd(UINT16 SlvAdrs, UINT08 CmdCode, UINT16 DataWr, UINT16 *pDataRd);
    RC ProcCallBlkCmd(UINT16 SlvAdrs, UINT08 CmdCode, vector<UINT08> *pDataWr,
                      vector<UINT08> *pDataRd);
    RC HostNotify(UINT08 DvcAdrs, UINT16 Data);
    RC IsSlavePresent (UINT16 SlvAddr);
    RC Search();
    RC ResetCtrlReg();
    RC GetSlaveAddrs(vector<UINT08> *pvSlvAddr);

private:
    enum class Protocol
    {
        WR_QUICK,
        RD_QUICK,
        SND_BYTE,
        RCV_BYTE,
        WR_BYTE,
        RD_BYTE,
        WR_WORD,
        RD_WORD,
        WR_BLK,
        RD_BLK
    };

    UINT32 m_CtrlBase, m_Index;
    vector<UINT08> m_vSlvAddr;
    bool m_IsSearched;

    RC SetData(vector<UINT08> *pData, Protocol Protocol);
    RC SetProtocol(Protocol Protocol);
    RC GetData(vector<UINT08> *pData, Protocol Protocol);
    RC ResetAndExelwte(UINT16 SlvAddr, Protocol Protocol, vector<UINT08> *pDataWr,
                vector<UINT08> *pDataRd, UINT08 Cmd);
    RC ExelwteSmbCmd(UINT16 SlvAddr, Protocol Protocol, vector<UINT08> *pDataWr,
                vector<UINT08> *pDataRd, UINT08 Cmd);
    RC Transaction();
    RC AcquireSemaphore();
    RC ReleaseSemaphore();
};

#endif //INCLUDED_AMD_SMBPORT_H
