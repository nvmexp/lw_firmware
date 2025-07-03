/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_VMFB_H
#define INCLUDED_VMFB_H

#include "core/include/framebuf.h"
class GpuSubdevice;

//--------------------------------------------------------------------
//! \brief FrameBuffer subclass for SRIOV virtual machines
//!
//! FrameBuffer class used so that we can run DMA tests such as WfMats
//! in a virtual machine, even though the VM only has access to part
//! of the FB and don't have the info needed to do full
//! memory-decoding.
//!
//! This class takes whatever portion of the FB we can access, and
//! models it as a phony RAM with a 32-bit bus, one beat/burst, one
//! FBIO, one subchannel, one rank, and one bank, and roughly
//! sqrt(fbSize) rows and sqrt(fbSize) bytes per row.  (Our decode
//! logic uses 32-bit rows and 32-bit columns, so we split the 64-bit
//! physical address between them.)
//!
class VmFrameBuffer : public FrameBuffer
{
public:
    explicit VmFrameBuffer(GpuSubdevice* pSub, LwRm* pLwRm) :
        FrameBuffer("VmFrameBuffer", pSub, pLwRm) {}
    RC Initialize() override;

    RC GetFbRanges(FbRanges* pRanges) const override;
    UINT64 GetGraphicsRamAmount()  const override { return m_FbSize; }
    bool   IsSOC()                 const override;
    UINT32 GetSubpartitions()      const override { return 1; }
    UINT32 GetBusWidth()           const override { return 32; }
    UINT32 GetPseudoChannelsPerChannel() const override { return 1; }
    bool   IsEccOn()               const override { return m_IsEccOn; }
    bool   IsRowRemappingOn()      const override { return m_IsRowRemappingOn; }
    UINT32 GetRankCount()          const override { return 1; }
    UINT32 GetBankCount()          const override { return 1; }
    UINT32 GetRowBits()            const override { return m_RowBits; }
    UINT32 GetRowSize()            const override { return m_RowSize; }
    UINT32 GetBurstSize()          const override { return 32; }
    UINT32 GetDataRate()           const override { return 2; }
    bool   IsFbBroken()            const override { return m_FbSize == 0; }

    RC EncodeAddress(const FbDecode& decode, UINT32 pteKind,
                     UINT32 pageSizeKB, UINT64* pEccOnAddr,
                     UINT64* pEccOffAddr) const override;
    RC DecodeAddress(FbDecode* pDecode, UINT64 fbOffset,
                     UINT32 pteKind, UINT32 pageSizeKB) const override;
    void GetRBCAddress(EccAddrParams* pRbcAddr, UINT64 fbOffset,
                       UINT32 pteKind, UINT32 pageSize, UINT32 errCount,
                       UINT32 errPos) override;
    void GetEccInjectRegVal(EccErrInjectParams* pInjectParams, UINT64 fbOffset,
                            UINT32 pteKind, UINT32 pageSize, UINT32 errCount,
                            UINT32 errPos) override;

private:
    UINT64 m_FbSize  = 0;
    UINT32 m_RowSize = 0;
    UINT32 m_RowBits = 0;
    bool   m_IsEccOn = false;
    bool   m_IsRowRemappingOn = false;
};

#endif
