/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_TEGRAFB_H
#define INCLUDED_TEGRAFB_H

#include "core/include/framebuf.h"
class GpuSubdevice;

/**
 * @class( TegraFrameBuffer )
 *
 * CheetAh frame buffer device.
 */

class TegraFrameBuffer : public FrameBuffer
{
public:
    ~TegraFrameBuffer() override;

    explicit TegraFrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm);

    RC     GetFbRanges(FbRanges *pRanges) const override;
    UINT64 GetGraphicsRamAmount() const override;
    bool   IsSOC() const override;
    UINT32 GetSubpartitions() const override;
    UINT32 GetBusWidth() const override;
    UINT32 VirtualFbpToHwFbp(UINT32 virtualFbp) const override;
    UINT32 HwFbpToVirtualFbp(UINT32 hwFbp) const override;
    UINT32 VirtualFbioToHwFbio(UINT32 virtualFbio) const override;
    UINT32 HwFbioToVirtualFbio(UINT32 hwFbio) const override;
    UINT32 VirtualLtcToHwLtc(UINT32 virtualLtc) const override;
    UINT32 HwLtcToVirtualLtc(UINT32 hwLtc) const override;
    UINT32 HwLtcToHwFbio(UINT32 hwLtc) const override;
    UINT32 HwFbioToHwLtc(UINT32 hwFbio, UINT32 subpartition) const override;
    UINT32 VirtualL2SliceToHwL2Slice(UINT32 virtualLtc, UINT32 virtualL2Slice) const override;
    UINT32 GetPseudoChannelsPerChannel() const override;
    bool   IsEccOn() const override;
    bool   IsRowRemappingOn() const override;
    UINT32 GetRankCount() const override;
    UINT32 GetBankCount() const override;
    UINT32 GetRowBits() const override;
    UINT32 GetRowSize() const override;
    UINT32 GetBurstSize() const override;
    UINT32 GetBeatSize() const override;
    UINT32 GetDataRate() const override;

    RamProtocol GetRamProtocol() const override { return m_RamProtocol; }

    RC     Initialize() override;
    RC     DecodeAddress(FbDecode* pDecode, UINT64 fbOffset,
                         UINT32 pteKind, UINT32 pageSizeKB) const override;
    void GetRBCAddress
    (
        EccAddrParams *pRbcAddr,
        UINT64 fbOffset,
        UINT32 pteKind,
        UINT32 pageSize,
        UINT32 errCount,
        UINT32 errPos
    ) override;
    void GetEccInjectRegVal
    (
         EccErrInjectParams *pInjectParams,
         UINT64 fbOffset,
         UINT32 pteKind,
         UINT32 pageSize,
         UINT32 errCount,
         UINT32 errPos
    ) override;

protected:
    RC InitFbpInfo() override;

private:
    RamProtocol m_RamProtocol;
};

#endif // ! INCLUDED_TEGRAFB_H
