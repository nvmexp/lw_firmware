
/* LWIDIA_COPYRIGHT_BEGIN                                                 */
/*                                                                        */
/* Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All  */
/* information contained herein is proprietary and confidential to LWPU */
/* Corporation.  Any use, reproduction, or disclosure without the written */
/* permission of LWPU Corporation is prohibited.                        */
/*                                                                        */
/* LWIDIA_COPYRIGHT_END                                                   */

//! \file uphyif.h -- Uphy interface defintion

#pragma once

#include "core/include/types.h"
#include "core/include/rc.h"
#include <vector>

//--------------------------------------------------------------------
//! \brief Pure virtual interface describing LwLink interfaces specific to a GPU
//!
//! CheetAh already has a class called uphy
//!
class UphyIf
{
public:
    enum class Version : UINT08
    {
       V10
      ,V30
      ,V31
      ,V32
      ,V50
      ,V61
      ,UNKNOWN
    };
    enum class RegBlock : UINT08
    {
        CLN,
        DLN,
        ALL
    };
    enum class Interface : UINT08
    {
        Pcie,
        LwLink,
        C2C,
        All
    };

    UINT32 GetActiveLaneMask(RegBlock regBlock) const
        { return DoGetActiveLaneMask(regBlock); }
    UINT32 GetMaxRegBlocks(RegBlock regBlock) const
        { return DoGetMaxRegBlocks(regBlock); }
    UINT32 GetMaxLanes(RegBlock regBlock) const
        { return DoGetMaxLanes(regBlock); }
    bool GetRegLogEnabled() const
        { return DoGetRegLogEnabled(); }
    UINT64 GetRegLogRegBlockMask(RegBlock regBlock)
        { return DoGetRegLogRegBlockMask(regBlock); }
    Version GetVersion() const
        { return DoGetVersion(); }
    RC IsRegBlockActive(RegBlock regBlock, UINT32 blockIdx, bool *pbActive)
        { return DoIsRegBlockActive(regBlock, blockIdx, pbActive); }
    bool IsSupported() const
        { return DoIsUphySupported(); }
    RC ReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData)
        { return DoReadPadLaneReg(linkId, lane, addr, pData); }
    RC WritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data)
        { return DoWritePadLaneReg(linkId, lane, addr, data); }
    RC ReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData)
        { return DoReadPllReg(ioctl, addr, pData); }
    void SetRegLogEnabled(bool bEnabled)
        { DoSetRegLogEnabled(bEnabled); }
    RC SetRegLogLinkMask(UINT64 linkMask)
        { return DoSetRegLogLinkMask(linkMask); }

    static const char * InterfaceToString(Interface interface);
    static const char * VersionToString(Version version);
    static const char * RegBlockToString(RegBlock regblock);
private:

    virtual UINT32 DoGetActiveLaneMask(RegBlock regBlock) const = 0;
    virtual UINT32 DoGetMaxRegBlocks(RegBlock regBlock) const = 0;
    virtual UINT32 DoGetMaxLanes(RegBlock regBlock) const = 0;
    virtual bool DoGetRegLogEnabled() const = 0;
    virtual UINT64 DoGetRegLogRegBlockMask(RegBlock regBlock) = 0;
    virtual Version DoGetVersion() const = 0;
    virtual RC DoIsRegBlockActive(RegBlock regBlock, UINT32 blockIdx, bool *pbActive) = 0;
    virtual bool DoIsUphySupported() const = 0;
    virtual RC DoReadPadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 * pData) = 0;
    virtual RC DoWritePadLaneReg(UINT32 linkId, UINT32 lane, UINT16 addr, UINT16 data) = 0;
    virtual RC DoReadPllReg(UINT32 ioctl, UINT16 addr, UINT16 * pData) = 0;
    virtual void DoSetRegLogEnabled(bool bEnabled) = 0;
    virtual RC DoSetRegLogLinkMask(UINT64 linkMask) = 0;
};
