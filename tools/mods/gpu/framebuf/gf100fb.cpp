/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2017,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// GF100 frame buffer interface.
#include "gf100fb.h"
#include "gf100adr.h"
#include "fermi/gf100/dev_mmu.h"
#include "fermi/gf100/dev_ltc.h"
#include "fermi/gf100/dev_top.h"
#include "fermi/gf100/hwproject.h"
#include "ctrl/ctrl2080/ctrl2080fb.h"

#ifndef MATS_STANDALONE
#include "gpu/include/floorsweepimpl.h"
#endif

GF100FrameBuffer::GF100FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm) :
    GpuFrameBuffer("GF100 FrameBuffer", pGpuSubdevice, pLwRm),
    m_FbpaMask(0),
    m_FbioShift(0)
{
}

GF100FrameBuffer::GF100FrameBuffer
(
    const char*   derivedClassName,
    GpuSubdevice* pGpuSubdevice,
    LwRm*         pLwRm
) :
    GpuFrameBuffer(derivedClassName, pGpuSubdevice, pLwRm),
    m_FbpaMask(0),
    m_FbioShift(0)
{
}

//--------------------------------------------------------------------
// For most GPUs, FBIO and FBPA map one-to-one in an "iron grid"
// configuration.  But in fermi through maxwell, they can be remapped
// to a limited extent.  If FBIO_SHIFT[n] is set, then FBIO[n] maps to
// FBPA[n-1].  Wraparound is possible.
//
UINT32 GF100FrameBuffer::HwFbioToHwFbpa(UINT32 hwFbio) const
{
    MASSERT(((GetFbioMask() >> hwFbio) & 1) == 1);
    if (((m_FbioShift >> hwFbio) & 1) == 1)
        return (hwFbio == 0) ? (GetMaxFbioCount() - 1) : (hwFbio - 1);
    else
        return hwFbio;
}

//--------------------------------------------------------------------
// See FBIO_SHIFT dislwssion in HwFbioToHwFbpa.
//
UINT32 GF100FrameBuffer::HwFbpaToHwFbio(UINT32 hwFbpa) const
{
    MASSERT(((m_FbpaMask >> hwFbpa) & 1) == 1);
    const UINT32 shiftedFbio = (hwFbpa + 1) % GetMaxFbioCount();
    if (((m_FbioShift >> shiftedFbio) & 1) == 1)
        return shiftedFbio;
    else
        return hwFbpa;
}

//--------------------------------------------------------------------
// See FBIO_SHIFT dislwssion in HwFbioToHwFbpa.
//
// The virtual FBIO and virtual FBPA are usually identical regardless
// of FBIO_SHIFT, with one exception: if the LSB of FBIO_SHIFT and
// FBIO_MASK are both 1, then FBIO[0] maps to FBPA[max].  In this
// "wraparound" case, virtualFbio[n] maps to virtualFbpa[n-1], modulo
// the number of FBIOs.
//
UINT32 GF100FrameBuffer::VirtualFbioToVirtualFbpa(UINT32 virtualFbio) const
{
    MASSERT(virtualFbio < GetFbioCount());
    if ((m_FbioShift & GetFbioMask() & 1) == 1)
        return (virtualFbio == 0) ? (GetFbioCount() - 1) : (virtualFbio - 1);
    else
        return virtualFbio;
}

//--------------------------------------------------------------------
// See FBIO_SHIFT dislwssion in VirtualFbioToVirtualFbpa.
//
UINT32 GF100FrameBuffer::VirtualFbpaToVirtualFbio(UINT32 virtualFbpa) const
{
    MASSERT(virtualFbpa < GetFbioCount());
    if ((m_FbioShift & GetFbioMask() & 1) == 1)
        return (virtualFbpa + 1) % GetFbioCount();
    else
        return virtualFbpa;
}

//--------------------------------------------------------------------
// The base-class implementation assumes that FBIO and FBPA are
// connected in an iron grid (see HwFbioToHwFbpa).  Ajusting for
// FBIO_SHIFT here.
//
/* virtual */ UINT32 GF100FrameBuffer::HwLtcToHwFbio(UINT32 hwLtc) const
{
    const UINT32 hwFbpa = GpuFrameBuffer::HwLtcToHwFbio(hwLtc);
    return HwFbpaToHwFbio(hwFbpa);
}

/* virtual */ UINT32 GF100FrameBuffer::HwFbioToHwLtc
(
    UINT32 hwFbio,
    UINT32 subpartition
) const
{
    const UINT32 hwFbpa = HwFbioToHwFbpa(hwFbio);
    return GpuFrameBuffer::HwFbioToHwLtc(hwFbpa, subpartition);
}

//--------------------------------------------------------------------
// The base-class implementation assumes that FBIO and FBPA are
// connected in an iron grid (see HwFbioToHwFbpa).  Ajusting for
// FBIO_SHIFT here.
//
/* virtual */ RC GF100FrameBuffer::InitCommon()
{
    RC rc;
    CHECK_RC(GpuFrameBuffer::InitCommon());
    m_FbpaMask  = GpuSub()->GetFsImpl()->FbpaMask();
    m_FbioShift = GpuSub()->GetFsImpl()->FbioshiftMask();
    return OK;
}

/* virtual */ RC GF100FrameBuffer::Initialize()
{
    RC rc;

    CHECK_RC(InitCommon());
    m_Decoder.reset(new GF100AddressDecode(GetFbioCount(), GpuSub()));
    m_Encoder.reset(new GF100AddressEncode(GetFbioCount(), GpuSub()));

    return OK;
}

void GF100FrameBuffer::EnableDecodeDetails(bool enable)
{
    m_Decoder->EnableDecodeDetails(enable);
}

RC GF100FrameBuffer::DecodeAddress
(
    FbDecode* pDecode,
    UINT64 physicalFbOffset,
    UINT32 pteKind,
    UINT32 pageSizeKB
) const
{
    MASSERT(m_Decoder.get() != nullptr);
    RC rc;
    if (m_Decoder.get() == nullptr)
        return RC::UNSUPPORTED_FUNCTION;
    CHECK_RC(m_Decoder->DecodeAddress(pDecode, physicalFbOffset,
                                      pteKind, pageSizeKB));
    return OK;
}

//See //hw/kepler1_gk100/fmod/fb_pa/LW_FB_pa_module.cpp#32:1748
//These definitions are taken from RAMIF addressing //hw tree. They match with
// dev_fbpa PFB_CFG register.
#define ECC_ADDR_COL  10:0
#define ECC_ADDR_ROW  27:11
#define ECC_ADDR_BANK 31:28
#define ECC_ADDR_EXT_PSEUDO_CHANNEL 30:30
#define ECC_ADDR_EXT_EXTBANK 31:31

void GF100FrameBuffer::GetRBCAddress
(
   EccAddrParams *pRbcAddr,
   UINT64 fbOffset,
   UINT32 pteKind,
   UINT32 pageSize,
   UINT32 errCount,
   UINT32 errPos
)
{
    FbDecode decode;
    if (m_Decoder.get() != 0)
    {
        m_Decoder->DecodeAddress(&decode, fbOffset, pteKind, pageSize);
    }
    else
    {
        //Decoder is not initialised yet
        memset((void *)pRbcAddr, 0, sizeof(EccAddrParams));
        MASSERT(0);
        return;
    }

    //See //hw/kepler1_gk100/fmod/fb_pa/LW_FB_pa_module.cpp:1745

    //Clear space for Ecc Error injection Position (8 Byte aligned for Ecc memory region)
    const UINT32 rowOffset = GetRowOffset(decode.burst, decode.beat,
                                          decode.beatOffset);
    UINT32 column = rowOffset / GetAmapColumnSize();
    column = (column>>3)<<3;

    //Make space bit address
    column <<= 6;

    //See //hw/kepler1_gk100/fmod/fb_pa/LW_FB_pa_module.cpp#32:1748
    if (errCount == 1)
    {
        // SEC
        if (errPos < 256)
        {
            // SEC in data
            column |= (errPos); //bit position to byte position
        }
        else
        {
            // what would be the byte index if the ECC payload bit (not data) has flipped
            MASSERT(!"Error injection in ECC bits is not supported");
        }
    }

    // Reference: //hw/lwgpu_gmlit4/clib/lwshared fmod/fb_pa/LW_FB_pa_module.cpp

    // Format is in bug 698886:
    UINT32 rbcAddr = 0;
    UINT32 rbcAddrExt = 0;
    UINT32 msb = Utility::Log2i(GetRowSize()) - 1;
    msb += 3;//Colwert to Bit
    if (msb <= 10)
    {
        rbcAddr |= REF_VAL(ECC_ADDR_COL, column);
    }
    else
    {
        rbcAddr    |= REF_VAL(msb:(msb - 10), column);
        rbcAddrExt |= REF_VAL((msb - 11):0,   column);
    }
    rbcAddr    |= REF_VAL(ECC_ADDR_ROW,                decode.row);
    rbcAddr    |= REF_VAL(ECC_ADDR_BANK,               decode.row);
    rbcAddrExt |= REF_VAL(ECC_ADDR_EXT_EXTBANK,        decode.rank);
    rbcAddrExt |= REF_VAL(ECC_ADDR_EXT_PSEUDO_CHANNEL, decode.rank);

    pRbcAddr->eccAddr =  rbcAddr;
    pRbcAddr->eccAddrExt = rbcAddrExt;
    pRbcAddr->extBank = 0; //Don't care
    pRbcAddr->subPartition = decode.subpartition;
    pRbcAddr->partition = decode.virtualFbio;
}

//------------------------------------------------------------------------------
void GF100FrameBuffer::GetEccInjectRegVal
(
   EccErrInjectParams *pInjectParams,
   UINT64 fbOffset,
   UINT32 pteKind,
   UINT32 pageSize,
   UINT32 errCount,
   UINT32 errPos
)
{
    Printf(Tee::PriWarn, "Function not supported on this chip\n");
    memset((void *)pInjectParams, 0, sizeof(EccErrInjectParams));
    return;
}

RC GF100FrameBuffer::EncodeAddress
(
    const FbDecode &decode,
    UINT32 pteKind,
    UINT32 pageSizeKB,
    UINT64 *pEccOnAddr,
    UINT64 *pEccOffAddr
) const
{
    MASSERT(m_Encoder.get() != 0);

    *pEccOnAddr  = m_Encoder.get()->EncodeAddress(decode, pteKind,
                                                  pageSizeKB, true);
    *pEccOffAddr = m_Encoder.get()->EncodeAddress(decode, pteKind,
                                                  pageSizeKB, false);
    return OK;
}

UINT32 GF100FrameBuffer::GetNumReversedBursts() const
{
    return 4;
}

FrameBuffer* CreateGF100FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new GF100FrameBuffer(pGpuSubdevice, pLwRm);
}
