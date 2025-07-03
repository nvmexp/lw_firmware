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

#ifndef INCLUDED_SMBPORT_H
#define INCLUDED_SMBPORT_H

#include "core/include/rc.h"

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

class McpDev;
//! \brief Abstract Base Class definition for Smbus ports
//-----------------------------------------------------------------------------

class SmbPort
{
public:
    SmbPort(SmbDevice *pSmbDev, UINT32 Index, UINT32 CtrlBase)
        : m_Index(Index),
          m_pHostDev(pSmbDev),
          m_IsInit(false){};
    virtual ~SmbPort(){};
    virtual RC Initialize() = 0;
    virtual RC Uninitialize() = 0;
    virtual RC RegRd(UINT32 Reg, UINT32 *pData, bool IsDebug=false) = 0;
    virtual RC RegWr(UINT32 Reg, UINT32 Data, bool IsDebug=false) = 0;
    virtual RC PrintInfo(Tee::Priority Pri = Tee::PriNormal) = 0;
    virtual RC PrintReg(Tee::Priority Pri = Tee::PriNormal) = 0;
    virtual RC Search() = 0;

    virtual RC ResetCtrlReg() = 0;

    virtual RC GetSlaveAddrs (vector<UINT08> *pvSlvAddr) = 0;

    //Smbus protocol implementation
    virtual RC WrQuickCmd(UINT16 SlvAdrs) = 0;
    virtual RC RdQuickCmd(UINT16 SlvAdrs) = 0;

    virtual RC SendByteCmd(UINT16 SlvAdrs, UINT08 Data) = 0;
    virtual RC RecvByteCmd(UINT16 SlvAdrs, UINT08 * pData) = 0;

    virtual RC WrByteCmd(UINT16 SlvAdrs, UINT08 CmdCode, UINT08 Data) = 0;
    virtual RC RdByteCmd(UINT16 SlvAdrs, UINT08 CmdCode, UINT08 * pData) = 0;

    virtual RC WrWordCmd(UINT16 SlvAdrs, UINT08 CmdCode, UINT16 Data) = 0;
    virtual RC RdWordCmd(UINT16 SlvAdrs, UINT08 CmdCode, UINT16 * pData) = 0;

    virtual RC WrBlkCmd(UINT16 SlvAdrs, UINT08 CmdCode,vector<UINT08> * pData) = 0;
    virtual RC RdBlkCmd(UINT16 SlvAdrs, UINT08 CmdCode,vector<UINT08> * pData) = 0;

    virtual RC ProcCallCmd(UINT16 SlvAdrs, UINT08 CmdCode,UINT16 DataWr, UINT16 * pDataRd) = 0;

    virtual RC ProcCallBlkCmd(UINT16 SlvAdrs, UINT08 CmdCode,vector<UINT08> * pDataWr, vector<UINT08> * pDataRd) = 0;
    virtual RC HostNotify(UINT08 DvcAdrs, UINT16 Data) = 0;

    virtual RC IsSlavePresent (UINT16 SlvAddr) = 0;

    //! \brief Return sub device index.
    //!
    //--------------------------------------------------------------------------
    UINT32 GetIndex() { return m_Index;}

    //! \brief establish external link.
    //!
    //--------------------------------------------------------------------------
    virtual RC LinkExtDev() { return OK; };

    //! \brief Index of itself.
    //--------------------------------------------------------------------------
    UINT32 m_Index;

    //! \brief The device that sub device belongs to.
    //--------------------------------------------------------------------------
    McpDev *m_pHostDev;

    //! \brief True if sub device got Initialized.
    //--------------------------------------------------------------------------
    bool m_IsInit;
};

#endif //INCLUDED_SMBPORT_H
