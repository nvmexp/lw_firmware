/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2007,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file smbport.h
//! \brief Header file with class for Smbus port
//!
//! Contains class definition for Smbus port
//!

#ifndef INCLUDED_LWSMBPORT_H
#define INCLUDED_LWSMBPORT_H

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

#define MAX_SLAVES 128

//! \brief Class definition for Smbus ports
//-----------------------------------------------------------------------------

class LwSmbPort : public SmbPort
{
public:
    LwSmbPort(SmbDevice *pSmbDev, UINT32 Index, UINT32 CtrlBase);//contructor
    ~LwSmbPort();//destructor
    RC Initialize();
    RC Uninitialize();
    RC RegRd(UINT32 Reg, UINT32 *pData, bool IsDebug = false);
    RC RegWr(UINT32 Reg, UINT32 Data, bool IsDebug = false);
    RC PrintInfo(Tee::Priority Pri = Tee::PriNormal);
    RC PrintReg(Tee::Priority Pri = Tee::PriNormal);
    RC Search();

    RC ResetCtrlReg();

    RC GetSlaveAddrs (vector<UINT08> *pvSlvAddr);

    //protocols
    RC WrQuickCmd(UINT16 SlvAdrs);
    RC RdQuickCmd(UINT16 SlvAdrs);

    RC SendByteCmd(UINT16 SlvAdrs, UINT08 Data);
    RC RecvByteCmd(UINT16 SlvAdrs, UINT08 * pData);

    RC WrByteCmd(UINT16 SlvAdrs, UINT08 CmdCode, UINT08 Data);
    RC RdByteCmd(UINT16 SlvAdrs, UINT08 CmdCode, UINT08 * pData);

    RC WrWordCmd(UINT16 SlvAdrs, UINT08 CmdCode, UINT16 Data);
    RC RdWordCmd(UINT16 SlvAdrs, UINT08 CmdCode, UINT16 * pData);

    RC WrBlkCmd(UINT16 SlvAdrs, UINT08 CmdCode,vector<UINT08> * pData);
    RC RdBlkCmd(UINT16 SlvAdrs, UINT08 CmdCode,vector<UINT08> * pData);

    RC ProcCallCmd(UINT16 SlvAdrs, UINT08 CmdCode,UINT16 DataWr, UINT16 * pDataRd);

    RC ProcCallBlkCmd(UINT16 SlvAdrs, UINT08 CmdCode,vector<UINT08> * pDataWr,
                      vector<UINT08> * pDataRd);
    RC HostNotify(UINT08 DvcAdrs, UINT16 Data);

    RC IsSlavePresent (UINT16 SlvAddr);

private:
    UINT32 m_CtrlBase,m_Index;
    vector<UINT08> m_vSlvAddr;
    bool m_IsSearched;

    RC SetData(vector<UINT08> *pData);
    RC GetData(vector<UINT08> *pData);
    RC Protocol(UINT16 SlvAddr,UINT08 Protocol,vector<UINT08> *pDataWr,vector<UINT08> *pDataRd,UINT08 Cmd);
    RC SetAddr(UINT08 Addr);
    RC SetPrtcl(UINT08 Prt, bool IsPec);
    RC WaitCycDone(UINT32 TimeoutMs);
    RC CheckError();
    bool IsBusy();

    RC PrintSta();

};

#endif //INCLUDED_LWSMBPORT_H
