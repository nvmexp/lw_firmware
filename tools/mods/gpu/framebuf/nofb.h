/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_NOFB_H
#define INCLUDED_NOFB_H

#include "core/include/framebuf.h"
class GpuSubdevice;

class NoFrameBuffer : public FrameBuffer
{
public:
    explicit NoFrameBuffer(GpuSubdevice *pSub, LwRm *pLwRm)
        : FrameBuffer("NoFrameBuffer", pSub, pLwRm),
         m_WarningMessage("function does nothing because mods is running with -fb_broken argument"),
         m_PrintPri(Tee::PriLow)
    {};
    ~NoFrameBuffer(){};

    RC GetFbRanges(FbRanges *pRanges) const override;
    UINT64 GetGraphicsRamAmount() const override;
    bool   IsSOC() const override;
    UINT32 GetSubpartitions() const override;
    UINT32 GetBusWidth() const override;
    UINT32 GetPseudoChannelsPerChannel() const override;
    bool   IsEccOn() const override;
    bool   IsRowRemappingOn() const override;
    UINT32 GetRankCount() const override;
    UINT32 GetBankCount() const override;
    UINT32 GetRowBits() const override;
    UINT32 GetRowSize() const override;
    UINT32 GetBurstSize() const override;
    RC Initialize() override;
    UINT32 GetDataRate() const override;
    RC   DecodeAddress(FbDecode *pDecode, UINT64 fbOffset,
                               UINT32 pteKind, UINT32 pageSizeKB) const override;
    void GetRBCAddress
    (
        EccAddrParams *pRbcAddr,
        UINT64 fbOffset,
        UINT32 pteKind,
        UINT32 pageSize,
        UINT32 errCount,
        UINT32 errPos
    ) override {}
    RC EccAddrToPhysAddr
    (
        const EccAddrParams & eccAddr,
        UINT32 pteKind,
        UINT32 pageSizeKB,
        PHYSADDR *pPhysAddr
    ) override;
    void GetEccInjectRegVal
    (
         EccErrInjectParams *pInjectParams,
         UINT64 fbOffset,
         UINT32 pteKind,
         UINT32 pageSize,
         UINT32 errCount,
         UINT32 errPos
    ) override {}

    bool IsFbBroken() const override { return true; }

private:
    string m_WarningMessage;
    UINT32 m_PrintPri;
};
#endif
