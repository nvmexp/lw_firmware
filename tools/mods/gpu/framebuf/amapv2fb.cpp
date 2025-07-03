/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/framebuf/amapv2fb.h"

//--------------------------------------------------------------------
AmapV2FrameBuffer::AmapV2FrameBuffer
(
    const char*   name,
    GpuSubdevice* pGpuSubdevice,
    LwRm*         pLwRm,
    AmapLitter    litter
) :
    AmapV1FrameBuffer(name, pGpuSubdevice, pLwRm, litter)
{
    m_ChipConf.litter = litter;
}

//--------------------------------------------------------------------
//! Colwert a physical address to the bit-packed RBC format used by
//! the LW_PFB_FBPA_*_ECC_ADDR & LW_PFB_FBPA_*_ECC_ADDR_EXT registers
//! when the GPU reports an ECC fault.  Used by trace3d plugins.
//!
void AmapV2FrameBuffer::GetRBCAddress
(
    EccAddrParams *pRbcAddr,
    UINT64 fbOffset,
    UINT32 pteKind,
    UINT32 pageSizeKB,
    UINT32 errCount,
    UINT32 errPos
)
{
    MASSERT(pRbcAddr);

    MemParamsV1 memParams;
    GetMemParams(&memParams, pteKind, pageSizeKB);

    EccPaParamsV1 eccPaParams = {0};
    eccPaParams.pa = fbOffset;
    eccPaParams.EccErrInjection4BOffset = errPos;

    EccAddrParamsV2 eccAddrParams = {0};
    mapPaToEccAddrV2(m_pAmapConf, &memParams, &eccPaParams, &eccAddrParams);

    memset(pRbcAddr, 0, sizeof(*pRbcAddr));
    pRbcAddr->eccAddr      = eccAddrParams.ECC_ADDR;
    pRbcAddr->eccAddrExt   = eccAddrParams.ECC_ADDR_EXT;
    pRbcAddr->eccAddrExt2  = eccAddrParams.ECC_ADDR_EXT2;
    pRbcAddr->extBank      = eccAddrParams.extBank;
    pRbcAddr->subPartition = eccAddrParams.subPartition;
    pRbcAddr->partition    = eccAddrParams.partition;
    pRbcAddr->channel      = eccAddrParams.channel;
}

//------------------------------------------------------------------------------
RC AmapV2FrameBuffer::EccAddrToPhysAddr
(
    const EccAddrParams & eccAddr,
    UINT32 pteKind,
    UINT32 pageSizeKB,
    PHYSADDR *pPhysAddr
)
{
    MASSERT(pPhysAddr);

    MemParamsV1 memParams;
    GetMemParams(&memParams, pteKind, pageSizeKB);

    EccAddrParamsV2 eccAddrParams = { };
    eccAddrParams.ECC_ADDR      = eccAddr.eccAddr;
    eccAddrParams.ECC_ADDR_EXT  = eccAddr.eccAddrExt;
    eccAddrParams.ECC_ADDR_EXT2 = eccAddr.eccAddrExt2;
    eccAddrParams.extBank       = eccAddr.extBank;
    eccAddrParams.subPartition  = eccAddr.subPartition;
    eccAddrParams.partition     = eccAddr.partition;
    eccAddrParams.channel       = eccAddr.channel;

    EccPaParamsV1 eccPaParams = { };
    mapEccAddrToPaV2(m_pAmapConf, &memParams, &eccAddrParams, &eccPaParams);
    *pPhysAddr = static_cast<PHYSADDR>(eccPaParams.pa);
    return RC::OK;
}

//--------------------------------------------------------------------
//! Colwert a physical address to the fields used by the
//! LW_PFB_FBPA_*_ECC_CONTROL and LW_PFB_FBPA_*_ECC_ERR_INJECTION_ADDR
//! registers to disable ECC checkbits at a specified address.
//!
void AmapV2FrameBuffer::GetEccInjectRegVal
(
    EccErrInjectParams *pInjectParams,
    UINT64 fbOffset,
    UINT32 pteKind,
    UINT32 pageSizeKB,
    UINT32 errCount,
    UINT32 errPos
)
{
    MASSERT(pInjectParams);

    MemParamsV1 memParams;
    GetMemParams(&memParams, pteKind, pageSizeKB);

    EccPaParamsV1 paParams = {0};
    paParams.pa = fbOffset;
    paParams.EccErrInjection4BOffset = errPos;

    EccErrInjectParamsV2 injectParams = {0};
    mapPaToEccErrInjectV2(m_pAmapConf, &memParams, &paParams, &injectParams);

    memset(pInjectParams, 0, sizeof(*pInjectParams));
    pInjectParams->eccCtrlWrtKllPtr0   = injectParams.ECC_CONTROL_WRITE_KILL_PTR0;
    pInjectParams->eccErrInjectAddr    = injectParams.ECC_ERR_INJECTION_ADDR;
    pInjectParams->eccErrInjectAddrExt = injectParams.ECC_ERR_INJECTION_ADDR_EXT;
    pInjectParams->subPartition        = injectParams.subPartition;
    pInjectParams->partition           = injectParams.partition;
    pInjectParams->channel             = injectParams.channel;
}

//--------------------------------------------------------------------
//! \brief Disable the FB ECC checkbits at just one address
//!
RC AmapV2FrameBuffer::DisableFbEccCheckbitsAtAddress
(
    UINT64 physAddr,
    UINT32 pteKind,
    UINT32 pageSizeKB
)
{
    const RegHal &regs = GpuSub()->Regs();
    RC rc;

    // Use amaplib to find the value to write to
    // ECC_ERR_INJECTION_ADDR, as well as the FBIO & channel
    //
    MemParamsV1 memParams;
    GetMemParams(&memParams, pteKind, pageSizeKB);

    EccPaParamsV1        paParams     = {0};
    EccErrInjectParamsV2 injectParams = {0};

    paParams.pa = physAddr;
    mapPaToEccErrInjectV2(m_pAmapConf, &memParams, &paParams, &injectParams);

    const UINT32 hwLtc   = VirtualLtcToHwLtc(injectParams.partition);
    const UINT32 hwFbio  = HwLtcToHwFbio(hwLtc);
    const UINT32 subpart = DecodeAmapSubpartition(injectParams.subPartition,
                                                  hwLtc);
    const UINT32 channel = CallwlateChannel(subpart, injectParams.channel);

    // Write the registers
    //
    CHECK_RC(WriteEccRegisters(
                    regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_ECC_WRITE_KILL_ADR),
                    regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_ECC_WRITE_KILL),
                    injectParams.ECC_ERR_INJECTION_ADDR,
                    injectParams.ECC_ERR_INJECTION_ADDR_EXT,
                    hwFbio, channel));
    return RC::OK;
}

//--------------------------------------------------------------------
//! \brief Apply FB write-kill at an address
//!
//! See GpuFrameBuffer::ApplyFbEccWriteKillPtr() for a full description
//! \see GpuFrameBuffer::ApplyFbEccWriteKillPtr()
//!
RC AmapV2FrameBuffer::ApplyFbEccWriteKillPtr
(
    UINT64 physAddr,
    UINT32 pteKind,
    UINT32 pageSizeKB,
    INT32 kptr0,
    INT32 kptr1
)
{
    const RegHal &regs = GpuSub()->Regs();
    RC rc;

    if (!regs.IsSupported(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR_MODE_ADR))
        return RC::UNSUPPORTED_FUNCTION;

    // Use amaplib to find the values to write to ECC_CONTROL and
    // ECC_ERR_INJECTION_ADDR at the injection address, as well as the
    // corresponding FBIO & subpartition.
    //
    MemParamsV1 memParams;
    GetMemParams(&memParams, pteKind, pageSizeKB);

    EccPaParamsV1        paParams     = {0};
    EccErrInjectParamsV2 injectParams = {0};

    UINT32 controlValue =
        regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR_MODE_ADR);
    UINT32 controlMask =
        regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR_MODE);
    if (kptr0 >= 0)
    {
        paParams.pa = physAddr;
        paParams.EccErrInjection4BOffset = (physAddr & 0x3) * 8 + kptr0;
        mapPaToEccErrInjectV2(m_pAmapConf, &memParams,
                              &paParams, &injectParams);
        controlValue |=
            regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR0,
                          injectParams.ECC_CONTROL_WRITE_KILL_PTR0) |
            regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR0_EN_TRUE);
        controlMask |=
            regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR0) |
            regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR0_EN);
    }
    if (kptr1 >= 0)
    {
        paParams.pa = physAddr;
        paParams.EccErrInjection4BOffset = (physAddr & 0x3) * 8 + kptr1;
        mapPaToEccErrInjectV2(m_pAmapConf, &memParams,
                              &paParams, &injectParams);
        controlValue |=
            regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR1,
                          (physAddr % GetFbEccSectorSize()) * 8 + kptr1) |
            regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR1_EN_TRUE);
        controlMask |=
            regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR1) |
            regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR1_EN);
    }
    if (kptr0 < 0 && kptr1 < 0)
    {
        // Make sure we have a valid FBIO & subpart, even if the
        // caller sets both kptr0 & kptr1 to NO_KPTR for some reason.
        paParams.pa = physAddr;
        mapPaToEccErrInjectV2(m_pAmapConf, &memParams,
                              &paParams, &injectParams);
    }

    const UINT32 hwLtc   = VirtualLtcToHwLtc(injectParams.partition);
    const UINT32 hwFbio  = HwLtcToHwFbio(hwLtc);
    const UINT32 subpart = DecodeAmapSubpartition(injectParams.subPartition,
                                                  hwLtc);
    const UINT32 channel = CallwlateChannel(subpart, injectParams.channel);

    // Write the registers
    //
    CHECK_RC(WriteEccRegisters(controlValue, controlMask,
                               injectParams.ECC_ERR_INJECTION_ADDR,
                               injectParams.ECC_ERR_INJECTION_ADDR_EXT,
                               hwFbio, channel));
    return RC::OK;
}

//------------------------------------------------------------------------------
FrameBuffer* CreateGHLIT1FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new AmapV2FrameBuffer("GHLIT1 FrameBuffer", pGpuSubdevice, pLwRm,
                                 LITTER_GHLIT1);
}
