/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_AMAPV1FB_H
#define INCLUDED_AMAPV1FB_H

#include "gpufb.h"
#include "amap_v1.h"
#include "hal_v1.h"

//! \brief Base class for all FrameBuffer objects that use the AMAP V1 library

class AmapV1FrameBuffer : public GpuFrameBuffer
{
public:
    AmapV1FrameBuffer(const char *name, GpuSubdevice *pGpuSubdevice,
                      LwRm *pLwRm, AmapLitter litter);
    ~AmapV1FrameBuffer();
    UINT32 GetHalfFbpaMask()   const override { return m_HalfFbpaMask;   }
    UINT32 GetHalfFbpaStride() const override { return m_HalfFbpaStride; }
    RC     FbioSubpToHbmSiteChannel(UINT32 hwFbio, UINT32 subpartition,
                                     UINT32 *pHbmSite,
                                     UINT32 *pHbmChannel) const override;
    // This is for HBM3, it needs pseudoChannel as an additional input parameter
    RC     FbioSubpToHbmSiteChannel(UINT32 hwFbio, UINT32 subpartition,
                                    UINT32 pseudoChannel,
                                     UINT32 *pHbmSite,
                                     UINT32 *pHbmChannel) const override;
    RC     HbmSiteChannelToFbioSubp(UINT32 hbmSite, UINT32 hbmChannel,
                                    UINT32 *pHwFbio,
                                    UINT32 *pSubpartition) const override;
    UINT32 VirtualLtcToHwLtc(UINT32 virtualLtc) const override;
    UINT32 HwLtcToVirtualLtc(UINT32 hwLtc)      const override;

    RC     EncodeAddress(const FbDecode &decode, UINT32 pteKind,
                         UINT32 pageSizeKB, UINT64 *pEccOnAddr,
                         UINT64 *pEccOffAddr) const override;
    RC     DecodeAddress(FbDecode *pDecode, UINT64 fbOffset,
                         UINT32 pteKind, UINT32 pageSizeKB) const override;
    void   GetRBCAddress(EccAddrParams *pRbcAddr, UINT64 fbOffset,
                         UINT32 pteKind, UINT32 pageSizeKB,
                         UINT32 errCount, UINT32 errPos) override;
    RC EccAddrToPhysAddr
    (
        const EccAddrParams & eccAddr,
        UINT32 pteKind,
        UINT32 pageSizeKB,
        PHYSADDR *pPhysAddr
    ) override;
    void   GetEccInjectRegVal(EccErrInjectParams *pInjectParams,
                              UINT64 fbOffset, UINT32 pteKind,
                              UINT32 pageSizeKB, UINT32 errCount,
                              UINT32 errPos) override;
    void   EnableDecodeDetails(bool enable) override;
    RC     DisableFbEccCheckbitsAtAddress(UINT64 physAddr,
                                          UINT32 pteKind,
                                          UINT32 pageSizeKB) override;
    RC     ApplyFbEccWriteKillPtr(UINT64 physAddr, UINT32 pteKind,
                                  UINT32 pageSizeKB,
                                  INT32 kptr0, INT32 kptr1) override;
    GpuLitter GetAmapLitter() const override;

    // Static functions
    static bool GetSkipParseRowRemapTable();
    static RC SetSkipParseRowRemapTable(bool b);

    // Static variables
    static bool s_SkipParseRowRemapTable;

protected:
    RC         InitCommon() override;
    RC         InitAmap();
    RC         SetupAmapRowRemapping(void* pAmapConf);
    RC         ConfigureLtcMapping();
    RC         InitDramHal();
    void       GetMemParams(MemParamsV1 *pMemParams,
                            UINT32 pteKind, UINT32 pageSize) const;
    bool       SwizzleHbmRank() const;
    UINT32     DecodeAmapSubpartition(uint32_t amapSubpartition,
                                      UINT32 hwLtc) const;
    uint32_t   EncodeAmapSubpartition(UINT32 subpartition,
                                      UINT32 virtualLtc) const;
    UINT32     CallwlateChannel(UINT32 subpartition,
                                UINT32 amapChannel) const;
    UINT32     GetAmapExtBanks() const;
    void       HbmFlipSubpPc(UINT32 hwFbio, UINT32 *pSubp, UINT32 *pPc) const;

    UINT32 m_HalfFbpaMask        = 0;
    UINT32 m_HalfFbpaStride      = 0;
    bool   m_BalancedLtcMode     = false;
    DramHalChipConfV1 m_ChipConf = {}; //!< Litter (eg GMLIT4) & dram (eg HBM2)
    void *m_pAmapConfEccOff = nullptr; //!< Amap config if ECC is off
    void *m_pAmapConfEccOn  = nullptr; //!< Amap config if ECC is on
    void *m_pAmapConf       = nullptr; //!< Amap config to use for decoding
    void *m_pDramConf       = nullptr; //!< DRAM HAL config
    Tee::Priority m_VerbosePrintPri = Tee::PriNone;
};

#endif // INCLUDED_AMAPV1FB_H
