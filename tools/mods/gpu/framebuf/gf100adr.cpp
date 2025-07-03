/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2012,2014,2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// The contents of this file have been copied from:
// //hw/fermi1_gf100/clib/lwshared/Memory/FermiAddressMapping.cpp#45
//
// The GF108-specific code comes from:
// //hw/fermi1_gf108/clib/lwshared/Memory/FermiAddressMapping.cpp#2
//
// The GF119-specific code comes from:
// //hw/fermi1_gf119/clib/lwshared/Memory/FermiAddressMapping.cpp#1
//
// The address mapping is dolwmented here:
// //hw/doc/gpu/fermi/fermi/design/Functional_Descriptions/Fermi_address_mapping.doc

#include "gf100adr.h"
#include "bitop.h"
#include "fermi/gf100/dev_mmu.h"
#include "fermi/gf100/dev_ltc.h"
#include "fermi/gf110/dev_fbpa.h"

using namespace BitOp;

static const bool PartSwizzle = false;
static const bool SliceSwizzle = false;
static const bool SliceLowerBit = false;
#define PA_MSB 39

GF100AddressDecode::GF100AddressDecode
(
    UINT32 NumPartitions,
    GpuSubdevice* pSubdev
)
: m_PhysAddr(0),
  m_PteKind(0),
  m_PageSize(0),
  m_PrintDecodeDetails(false),
  m_pGpuSubdevice(pSubdev),
  m_NumPartitions(NumPartitions),
  m_FbioChannelWidth(0),
  m_NumXbarL2Slices(0),
  m_NumDRAMBanks(0),
  m_NumExtDRAMBanks(0),
  m_DRAMPageSize(0),
  m_DRAMBankSwizzle(false),
  m_ECCOn(false),
  m_BurstSize(0),
  m_BeatSize(0),
  m_PartitionInterleave(ComputePartitionInterleave(NumPartitions, pSubdev)),

  // GF100-specific:
  m_NumL2Sets(32),
  m_NumL2SetsBits(5),
  m_FBDataBusWidth(32),
  m_NumL2Sectors(4),
  m_NumL2Banks(4),
  m_PAKS(0),
  m_Quotient(0),
  m_Offset(0),
  m_Remainder(0),
  m_Partition(0),
  m_Slice(0),
  m_L2Set(0),
  m_L2Bank(0),
  m_L2Tag(0),
  m_L2Sector(0),
  m_L2Offset(0),
  m_SubPart(0),
  m_Bank(0),
  m_ExtBank(0),
  m_Row(0),
  m_Col(0)
{
    const FrameBuffer *pFb = m_pGpuSubdevice->GetFB();

    // The GF108 chip has one partition but two FBPAs, FbMask() will report 2
    m_NumXbarL2Slices =
        DRF_VAL(_PLTCG, _LTCS_LTSS_CBC_PARAM, _SLICES_PER_FBP,
            pSubdev->RegRd32(LW_PLTCG_LTCS_LTSS_CBC_PARAM));
    m_AmapColumnSize  = pFb->GetAmapColumnSize();
    m_NumDRAMBanks    = pFb->GetBankCount();
    m_NumExtDRAMBanks = Utility::Log2i(pFb->GetRankCount());
    m_DRAMPageSize    = pFb->GetRowSize();
    m_DRAMBankSwizzle = 0 !=
        DRF_VAL(_PFB, _FBPA_BANK_SWIZZLE, _ENABLE,
            pSubdev->RegRd32(LW_PFB_FBPA_BANK_SWIZZLE));
    m_ECCOn = 0 !=
        DRF_VAL(_PFB, _FBPA_ECC_CONTROL, _MASTER_EN,
            pSubdev->RegRd32(LW_PFB_FBPA_ECC_CONTROL));
    m_BurstSize = pFb->GetBurstSize();
    m_BeatSize = pFb->GetBeatSize();
}

void GF100AddressDecode::PrintFbConfig()
{
    constexpr auto Pri = Tee::PriDebug;
    Printf(Pri, "FB config for decode:\n");
    Printf(Pri, "  NumPartitions   %u\n", m_NumPartitions);
    Printf(Pri, "  NumXbarL2Slices %u\n", m_NumXbarL2Slices);
    Printf(Pri, "  NumDRAMBanks    %u\n", m_NumDRAMBanks);
    Printf(Pri, "  NumExtDRAMBanks %u\n", m_NumExtDRAMBanks);
    Printf(Pri, "  DRAMPageSize    %u\n", m_DRAMPageSize);
    Printf(Pri, "  DRAMBankSwizzle %s\n", m_DRAMBankSwizzle?"true":"false");
    Printf(Pri, "  ECCOn           %s\n", m_ECCOn?"true":"false");
    Printf(Pri, "  BurstSize       0x%x\n", m_BurstSize);
    Printf(Pri, "  BeatSize        0x%x\n", m_BeatSize);
    Printf(Pri, "  NumL2Sets       %u\n", m_NumL2Sets);
    Printf(Pri, "  NumL2SetsBits   %u\n", m_NumL2SetsBits);
    Printf(Pri, "  FBDataBusWidth  %u\n", m_FBDataBusWidth);
    Printf(Pri, "  NumL2Sectors    %u\n", m_NumL2Sectors);
    Printf(Pri, "  NumL2Banks      %u\n", m_NumL2Banks);
}

GF100AddressDecode::~GF100AddressDecode()
{
}

UINT64 GF100AddressDecode::PhysicalToRaw(UINT64 PhysicalFbOffset, UINT32 PteKind)
{
    switch (PteKind)
    {
        case LW_MMU_PTE_KIND_GENERIC_16BX2:
            {
                // Based on //hw/doc/gpu/fermi/fermi/design/Functional_Descriptions/physical-to-raw.vsd
                static const UINT32 PhysicalToRawMap[32] =
                {
                    0x020, 0x040, 0x120, 0x140,
                    0x030, 0x050, 0x130, 0x150,
                    0x000, 0x060, 0x100, 0x160,
                    0x010, 0x070, 0x110, 0x170,
                    0x0E0, 0x080, 0x1E0, 0x180,
                    0x0F0, 0x090, 0x1F0, 0x190,
                    0x0C0, 0x0A0, 0x1C0, 0x1A0,
                    0x0D0, 0x0B0, 0x1D0, 0x1B0
                };
                const UINT32 PhysicalGobOffset = static_cast<UINT32>(PhysicalFbOffset & 0x1F0);
                const UINT32 RawGobOffset = PhysicalToRawMap[PhysicalGobOffset>>4];
                return (PhysicalFbOffset & ~0x1F0) | RawGobOffset;
            }

        case LW_MMU_PTE_KIND_PITCH_NO_SWIZZLE:
        case LW_MMU_PTE_KIND_PITCH:
            // No translation required
            return PhysicalFbOffset;

        default:
            MASSERT(!"Unsupported PTE kind!");
            return 0;
    }
}

RC GF100AddressDecode::DecodeAddress
(
    FbDecode* pDecode,
    UINT64 physicalFbOffset,
    UINT32 pteKind,
    UINT32 pageSize
)
{
    const FrameBuffer *pFb = m_pGpuSubdevice->GetFB();
    m_PteKind = pteKind;
    switch (m_PteKind)
    {
        // Hack for bug 805235:
        // treating LW_MMU_PTE_KIND_C32_MS2_2C as blocklinear.
        case LW_MMU_PTE_KIND_C32_MS2_2C:
            m_PteKind = LW_MMU_PTE_KIND_GENERIC_16BX2;
            break;
        case LW_MMU_PTE_KIND_PITCH_NO_SWIZZLE:
        case LW_MMU_PTE_KIND_PITCH:
        case LW_MMU_PTE_KIND_GENERIC_16BX2:
            break;
        default:
            Printf(Tee::PriError, "DecodeAddress: "
                    "Unsupported PTE kind - 0x%02x!\n",
                    m_PteKind);
            MASSERT(!"Unsupported PTE kind!");
            return RC::BAD_PARAMETER;
    }
    m_PageSize = pageSize;
    switch (m_PageSize)
    {
        case 4:
        case 64:
        case 128:
            break;
        default:
            Printf(Tee::PriError, "DecodeAddress: "
                    "Unsupported page size - %uKB!\n",
                    m_PageSize);
            MASSERT(!"Unsupported page size!");
            return RC::BAD_PARAMETER;
    }

    m_PhysAddr = PhysicalToRaw(physicalFbOffset, m_PteKind);
    ComputePAKS();
    ComputeQRO();
    ComputePartition();
    ComputeSlice();
    ComputeL2Address();
    ComputeRBSC();

    const UINT32 rowOffset = ((m_Col * m_AmapColumnSize) +
                              (physicalFbOffset % m_AmapColumnSize));

    memset(pDecode, 0, sizeof(*pDecode));
    pDecode->virtualFbio  = pFb->VirtualFbpaToVirtualFbio(m_Partition);
    pDecode->subpartition = m_SubPart;
    pDecode->rank         = m_ExtBank;
    pDecode->bank         = m_Bank;
    pDecode->row          = m_Row;
    pDecode->burst        = rowOffset / m_BurstSize;
    pDecode->beat         = (rowOffset % m_BurstSize) / m_BeatSize;
    pDecode->beatOffset   = rowOffset % m_BeatSize;
    pDecode->hbmSite      = 0;
    pDecode->hbmChannel   = 0;
    pDecode->channel      = pDecode->subpartition;
    pDecode->extColumn    = pDecode->burst;
    pDecode->slice        = m_Slice;
    pDecode->xbarRawAddr  = XbarRawPaddr();

    if (m_PrintDecodeDetails)
    {
        PrintDecodeDetails(pteKind, pageSize);
    }
    return OK;
}

void GF100AddressDecode::ComputePAKS()
{
    m_PAKS = 0;
    switch (m_PteKind)
    {
        case LW_MMU_PTE_KIND_PITCH_NO_SWIZZLE:
            if (m_PageSize == 4)
            {
                if (m_NumPartitions == 3)
                {
                    CalcPAKSCommonPS4KParts3(m_PhysAddr);
                    break;
                }
                if (m_NumPartitions == 4)
                {
                    CalcPAKSCommonPS4KParts4(m_PhysAddr);
                    break;
                }
                if (m_NumPartitions == 5)
                {
                    CalcPAKSCommonPS4KParts5(m_PhysAddr);
                    break;
                }
                if (m_NumPartitions == 8)
                {
                    CalcPAKSCommonPS4KParts8(m_PhysAddr);
                    break;
                }
            }
            m_PAKS = m_PhysAddr;
            break;

        case LW_MMU_PTE_KIND_PITCH:
            if (m_PageSize == 4)
            {
                switch (m_NumPartitions)
                {
                    case 1:
                        CalcPAKSPitchLinearPS4KParts1();
                        break;
                    case 2:
                        CalcPAKSPitchLinearPS4KParts2();
                        break;
                    case 3:
                        CalcPAKSPitchLinearPS4KParts3();
                        break;
                    case 4:
                        CalcPAKSPitchLinearPS4KParts4();
                        break;
                    case 5:
                        CalcPAKSPitchLinearPS4KParts5();
                        break;
                    case 6:
                        CalcPAKSPitchLinearPS4KParts6();
                        break;
                    case 7:
                        CalcPAKSPitchLinearPS4KParts7();
                        break;
                    case 8:
                        CalcPAKSPitchLinearPS4KParts8();
                        break;
                    default:
                        MASSERT(!"Invalid number of partitions.");
                        break;
                }
            }
            else
            {
                switch (m_NumPartitions)
                {
                    case 1:
                        CalcPAKSPitchLinearPS64KParts1();
                        break;
                    case 2:
                        CalcPAKSPitchLinearPS64KParts2();
                        break;
                    case 4:
                        CalcPAKSPitchLinearPS64KParts4();
                        break;
                    case 3:
                        CalcPAKSPitchLinearPS64KParts3();
                        break;
                    case 5:
                        CalcPAKSPitchLinearPS64KParts5();
                        break;
                    case 6:
                        CalcPAKSPitchLinearPS64KParts6();
                        break;
                    case 7:
                        CalcPAKSPitchLinearPS64KParts7();
                        break;
                    case 8:
                        CalcPAKSPitchLinearPS64KParts8();
                        break;
                    default:
                        MASSERT(!"Invalid number of partitions.");
                        break;
                }
            }
            break;

        // All block linear kinds
        case LW_MMU_PTE_KIND_GENERIC_16BX2:
            if (m_PageSize == 4)
            {
                switch (m_NumPartitions)
                {
                    case 1:
                        CalcPAKSBlockLinearPS4KParts1();
                        break;
                    case 2:
                        CalcPAKSBlockLinearPS4KParts2();
                        break;
                    case 3:
                        CalcPAKSBlockLinearPS4KParts3();
                        break;
                    case 4:
                        CalcPAKSBlockLinearPS4KParts4();
                        break;
                    case 5:
                        CalcPAKSBlockLinearPS4KParts5();
                        break;
                    case 6:
                        CalcPAKSBlockLinearPS4KParts6();
                        break;
                    case 7:
                        CalcPAKSBlockLinearPS4KParts7();
                        break;
                    case 8:
                        CalcPAKSBlockLinearPS4KParts8();
                        break;
                    default:
                        MASSERT(!"Invalid number of partitions.");
                        break;
                }
            }
            else
            {
                switch (m_NumPartitions)
                {
                    case 1:
                        CalcPAKSBlockLinearPS64KParts1();
                        break;
                    case 2:
                        CalcPAKSBlockLinearPS64KParts2();
                        break;
                    case 3:
                        CalcPAKSBlockLinearPS64KParts3();
                        break;
                    case 4:
                        CalcPAKSBlockLinearPS64KParts4();
                        break;
                    case 5:
                        CalcPAKSBlockLinearPS64KParts5();
                        break;
                    case 6:
                        CalcPAKSBlockLinearPS64KParts6();
                        break;
                    case 7:
                        CalcPAKSBlockLinearPS64KParts7();
                        break;
                    case 8:
                        CalcPAKSBlockLinearPS64KParts8();
                        break;
                    default:
                        MASSERT(!"Invalid number of partitions.");
                        break;
                }
            }
            break;
        default:
            MASSERT(!"Invalid PTE kind.");
            break;
    }
}

UINT32 GF100AddressDecode::ComputePartitionInterleave
(
    UINT32 NumPartitions,
    GpuSubdevice* pSubdev
)
{
    UINT32 PartitionInterleave = 0;
    switch (NumPartitions)
    {
        case 1:
        case 2:
        case 4:
        case 6:
        case 7:
        case 8:
            PartitionInterleave = 1024;
            break;

        case 3:
        case 5:
            PartitionInterleave = 2048;
            break;

        default:
            MASSERT(!"Unsupported number of partitions!");
            break;
    }
    return PartitionInterleave;
}

void GF100AddressDecode::ComputeQRO()
{
    m_Offset = (UINT32)(m_PAKS % m_PartitionInterleave);
    const UINT64 tmp = m_PAKS / m_PartitionInterleave;
    m_Quotient = tmp / m_NumPartitions;
    m_Remainder = (UINT32)(tmp % m_NumPartitions);
}

void GF100AddressDecode::ComputePartition()
{
    switch (m_NumPartitions)
    {
        case 1:
            m_Partition = 0;
            break;
        case 2:
        case 3:
        case 4:
        case 5:
        case 8:
            m_Partition = m_Remainder;
            break;
        case 6:
            m_Partition = static_cast<UINT32>((bit<2>(m_Quotient) ^ bit<0>(m_Remainder)) |
                (bits<2, 1>(m_Remainder) << 1));
            break;
        case 7:
            m_Partition = (m_Remainder + bits<2, 0>(m_Quotient) ) % 7;
            break;
        default:
            Printf(Tee::PriError, "DecodeAddress: "
                    "Unsupported number of partitions - %u!\n",
                    m_NumPartitions);
            MASSERT(!"Unsupported number of partitions!");
            break;
    }
}

void GF100AddressDecode::ComputeSlice()
{
    switch (m_NumXbarL2Slices)
    {
        case 2:
            switch (m_NumPartitions)
            {
                case 1:
                case 2:
                case 4:
                case 8:
                    if (SliceLowerBit) // 512B
                    {
                        m_Slice = bit<9>(m_Offset);
                    }
                    else // 1KB
                    {
                        m_Slice = static_cast<UINT32>(bit<0>(m_Quotient));
                    }
                    break;
                case 3:
                case 5:
                    m_Slice = bit<10>(m_Offset);
                    break;
                case 6:
                    if (SliceLowerBit)
                    {
                        m_Slice = static_cast<UINT32>(bit<9>(m_Offset) ^ bit<3>(m_Quotient));
                    }
                    else
                    {
                        m_Slice = static_cast<UINT32>(bit<0>(m_Quotient) ^ bit<0>(m_Remainder) ^ bit<3>(m_Quotient)    // post divide R swizzle
                            ^ bit<4>(m_Quotient)  ^ bit<5>(m_Quotient)  ^ bit<6>(m_Quotient)  ^ bit<7>(m_Quotient)  ^ bit<8>(m_Quotient)  ^ bit<9>(m_Quotient)  ^ bit<10>(m_Quotient)
                            ^ bit<11>(m_Quotient) ^ bit<12>(m_Quotient) ^ bit<13>(m_Quotient) ^ bit<14>(m_Quotient) ^ bit<15>(m_Quotient) ^ bit<16>(m_Quotient) ^ bit<17>(m_Quotient)
                            ^ bit<18>(m_Quotient) ^ bit<19>(m_Quotient) ^ bit<20>(m_Quotient) ^ bit<21>(m_Quotient) ^ bit<22>(m_Quotient) ^ bit<23>(m_Quotient) ^ bit<24>(m_Quotient)
                            ^ bit<25>(m_Quotient) ^ bit<26>(m_Quotient) ^ bit<27>(m_Quotient) ^ bit<28>(m_Quotient) ^ bit<29>(m_Quotient) ^ bit<30>(m_Quotient) ^ bit<31>(m_Quotient));
                    }
                    break;
                case 7:
                    m_Slice = static_cast<UINT32>(bit<3>(m_Quotient) ^ bit<2>(m_Quotient) ^ bit<0>(m_Quotient) ^ bit<0>(m_Partition));
                    break;
                default:
                    MASSERT(!"Invalid number of partitions.");
                    break;
            }
            break;

        case 4:
            switch (m_NumPartitions)
            {
                case 1:
                case 2:
                case 4:
                case 8:
                    m_Slice = static_cast<UINT32>(bit<0>(m_Quotient) << 1) | bit<9>(m_Offset);
                    break;
                case 3:
                case 5:
                    m_Slice = bits<10, 9>(m_Offset);
                    break;
                case 6:
                    m_Slice = static_cast<UINT32>(((bit<0>(m_Quotient)  ^ bit<0>(m_Remainder) ^ bit<3>(m_Quotient)    // post divide R swizzle
                               ^ bit<4>(m_Quotient) ^ bit<5>(m_Quotient) ^ bit<6>(m_Quotient) ^ bit<7>(m_Quotient) ^ bit<8>(m_Quotient) ^ bit<9>(m_Quotient) ^ bit<10>(m_Quotient)
                               ^ bit<11>(m_Quotient) ^ bit<12>(m_Quotient) ^ bit<13>(m_Quotient) ^ bit<14>(m_Quotient) ^ bit<15>(m_Quotient) ^ bit<16>(m_Quotient) ^ bit<17>(m_Quotient)
                               ^ bit<18>(m_Quotient) ^ bit<19>(m_Quotient) ^ bit<20>(m_Quotient) ^ bit<21>(m_Quotient) ^ bit<22>(m_Quotient) ^ bit<23>(m_Quotient) ^ bit<24>(m_Quotient)
                               ^ bit<25>(m_Quotient) ^ bit<26>(m_Quotient) ^ bit<27>(m_Quotient) ^ bit<28>(m_Quotient) ^ bit<29>(m_Quotient) ^ bit<30>(m_Quotient) ^ bit<31>(m_Quotient)
                               ) << 1  ) | bit<9>(m_Offset));
                    break;
                case 7:
                    m_Slice = static_cast<UINT32>(((bit<3>(m_Quotient) ^ bit<2>(m_Quotient) ^ bit<0>(m_Quotient) ^ bit<0>(m_Partition)) << 1) | bit<9>(m_Offset));
                    break;
                default:
                    MASSERT(!"Invalid number of partitions.");
                    break;
            }
            break;

        default:
            MASSERT(!"Invalid number of XBAR L2 slices.");
            break;
    }
}

void GF100AddressDecode::ComputeL2Address()
{
    const UINT32 switchKey =
        m_NumXbarL2Slices * 1000
        + m_NumL2Sets     * 10
        + m_NumPartitions;

    switch (switchKey)
    {
        // 2 slices, 16 L2 sets
        case 2168: case 2167: case 2166: case 2164: case 2162: case 2161:
            CalcL2SetSlices2L2Sets16Parts876421();
            break;
        case 2165: case 2163:
            CalcL2SetSlices2L2Sets16Parts53();
            break;

        // 2 slices, 32 L2 sets
        case 2328: case 2327:
        case 2326: case 2324: case 2322: case 2321:
            CalcL2SetSlices2L2Sets32Parts876421();
            break;
        case 2325: case 2323:
            CalcL2SetSlices2L2Sets32Parts53();
            break;

        // 4 slices, 16 L2 sets
        case 4168: case 4167: case 4166: case 4164: case 4162: case 4161:
            CalcL2SetSlices4L2Sets16Parts876421();
            break;
        case 4165: case 4163:
            CalcL2SetSlices4L2Sets16Parts53();
            break;

        // 4 slices, 32 L2 sets
        case 4328: case 4327: case 4326: case 4324: case 4322: case 4321:
            CalcL2SetSlices4L2Sets32Parts876421();
            break;
        case 4325: case 4323:
            CalcL2SetSlices4L2Sets32Parts53();
            break;

        default:
            Printf(Tee::PriError, "DecodeAddress: "
                    "Unsupported memory configuration - "
                    "%u slices, %u L2 sets, %u partitions\n",
                    m_NumXbarL2Slices, m_NumL2Sets, m_NumPartitions);
            MASSERT(!"Unsupported memory configuration!");
            break;
    }
}

void GF100AddressDecode::ComputeRBSC()
{
    const UINT32 saveL2Set = m_L2Set;
    const UINT32 saveL2Tag = m_L2Tag;

    if (m_DRAMBankSwizzle)
    {
        UINT32 padr = XbarRawPaddr();
        padr = (padr & ~0x38) | ((bits<2, 0>( bits<5, 3>(padr) -3*bits<8, 6>(padr))) << 3);

        // Update for ECC RBSC callwlation
        m_L2Set = padr & ((1<<m_NumL2SetsBits) -1);
        m_L2Tag = padr >> m_NumL2SetsBits;
    }
    if (m_ECCOn)
    {
        const UINT32 padr = (XbarRawPaddr() / 7) * 8 | (XbarRawPaddr() % 7);

        // Update for ECC RBSC callwlation
        m_L2Set = padr & ((1<<m_NumL2SetsBits) -1);
        m_L2Tag = padr >> m_NumL2SetsBits;
    }

    const UINT32 switchKey =
        m_NumXbarL2Slices * 10000
        + m_NumL2Sets     * 100
        + m_NumDRAMBanks  * 1;

    switch (switchKey)
    {
        // 4 slices, 16 L2 sets, 4 DRAM banks
        case 41604:
            CalcRBCSlices4L2Sets16DRAMBanks4();
            break;

        // 4 slices, 16 L2 sets, 8 DRAM banks
        case 41608:
            CalcRBCSlices4L2Sets16DRAMBanks8();
            break;

        // 4 slices, 16 L2 sets, 16 DRAM banks
        case 41616:
            CalcRBCSlices4L2Sets16DRAMBanks16();
            break;

        // 4 slices, 32 L2 sets, 4 DRAM banks
        case 43204:
            CalcRBCSlices4L2Sets32DRAMBanks4();
            break;

        // 4 slices, 32 L2 sets, 8 DRAM banks
        case 43208:
            CalcRBCSlices4L2Sets32DRAMBanks8();
            break;

        // 4 slices, 32 L2 sets, 16 DRAM banks
        case 43216:
            CalcRBCSlices4L2Sets32DRAMBanks16();
            break;

        // 2 slices, 16 L2 sets, 4 DRAM banks
        case 21604:
            CalcRBCSlices2L2Sets16DRAMBanks4();
            break;

        // 2 slices, 16 L2 sets, 8 DRAM banks
        case 21608:
            CalcRBCSlices2L2Sets16DRAMBanks8();
            break;

        // 2 slices, 16 L2 sets, 16 DRAM banks
        case 21616:
            CalcRBCSlices2L2Sets16DRAMBanks16();
            break;

        // 2 slices, 32 L2 sets, 4 DRAM banks
        case 23204:
            CalcRBCSlices2L2Sets32DRAMBanks4();
            break;

        // 2 slices, 32 L2 sets, 8 DRAM banks
        case 23208:
            CalcRBCSlices2L2Sets32DRAMBanks8();
            break;

        // 2 slices, 32 L2 sets, 16 DRAM banks
        case 23216:
            CalcRBCSlices2L2Sets32DRAMBanks16();
            break;

        default:
            Printf(Tee::PriError, "DecodeAddress: "
                    "Unsupported memory configuration - "
                    "%u slices, %u L2 sets, %u DRAM banks, %u ext DRAM banks\n",
                    m_NumXbarL2Slices, m_NumL2Sets, m_NumDRAMBanks, m_NumExtDRAMBanks);
            MASSERT(!"Unsupported memory configuration!");
            break;
    }

    // Restore
    m_L2Set = saveL2Set;
    m_L2Tag = saveL2Tag;
}

void GF100AddressDecode::PrintDecodeDetails(UINT32 PteKind, UINT32 PageSize)
{
    Printf(Tee::PriNormal, "GF10x Address Decode 0x%llx, kind 0x%02x, page %uKB: "
        "numpart %u, "
        "fbpa %u, "
        "subpart %u, "
        "extbank %u, "
        "bank %u, "
        "row 0x%x, "
        "column 0x%x\n",
        m_PhysAddr, PteKind, PageSize, m_NumPartitions, m_Partition, m_SubPart,
        m_ExtBank, m_Bank, m_Row, m_Col);
    Printf(Tee::PriNormal, "Detailed decode for 0x%llx: "
        "paks 0x%llx, pi %u, q 0x%llx, r 0x%x, o 0x%x, s %u, "
        "l2set %u, l2b %u, l2t %u, l2sec %u, l2o %u\n",
        m_PhysAddr, m_PAKS, m_PartitionInterleave, m_Quotient, m_Remainder, m_Offset, m_Slice,
        m_L2Set, m_L2Bank, m_L2Tag, m_L2Sector, m_L2Offset);
}

void GF100AddressDecode::CalcPAKSPitchLinearPS4KParts8()
{
    // Mapping with 4KB VM pages for vidmem is a galactic screw job.
    // part[2] and slice[1] are above the VM page.
    // Make it decent by m_PAKS[13] = m_PhysAddr[13] ^ m_PhysAddr[10] in both block linear and pitch

    // m_PAKS[6:0] = m_PhysAddr[6:0]                                                512B gob
    // m_PAKS[8:7] = m_PhysAddr[11:10]                                              512B gob
    // m_PAKS[9] = m_PhysAddr[7] ^ m_PhysAddr[14]                                     slice[0]
    // m_PAKS[11:10] = m_PhysAddr[9:8] + upper bit swizzle                          part[1:0]

    // Common mapping for both block linear and pitch
    // m_PAKS[12] = m_PhysAddr[12] ^ m_PhysAddr[13] ^ m_PhysAddr[16] ^ ...              part[2]
    // m_PAKS[13] = m_PhysAddr[13] ^ PhysAddrLev1[10] ^ m_PhysAddr[15] ^ ...              slice[1]

    const UINT64 PhysAddrLev1 = maskoff_lower_bits<12>(m_PhysAddr) |
        // part[1:0]
        // cannot swizzle m_PhysAddr[8] with m_PhysAddr[13] derivatives without aliasing
        (((bits<9, 8>(m_PhysAddr)
           + bits<11, 10>(m_PhysAddr)
           + bits<15, 14>(m_PhysAddr) + bits<18, 17>(m_PhysAddr) + bits<21, 20>(m_PhysAddr)
           + bits<24, 23>(m_PhysAddr) + bits<27, 26>(m_PhysAddr) + bits<30, 29>(m_PhysAddr)
          )%4) << 10) |

        // slice[0]
        // Won't help much to swizzle this bit.  Slice[1] has the bits all used.
        ((bit<7>(m_PhysAddr) ^ bit<14>(m_PhysAddr)) << 9) |

        // gob
        (bits<11, 10>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);

    CalcPAKSCommonPS4KParts8(PhysAddrLev1);
}

void GF100AddressDecode::CalcPAKSPitchLinearPS4KParts7()
{
    const UINT32 swizzle1 = static_cast<UINT32>(bit<11>(m_PhysAddr) ^  bit<12>(m_PhysAddr) ^  bit<13>(m_PhysAddr) ^ bit<14>(m_PhysAddr) ^ bit<15>(m_PhysAddr) ^
        bit<16>(m_PhysAddr) ^ bit<17>(m_PhysAddr) ^ bit<18>(m_PhysAddr) ^ bit<19>(m_PhysAddr) ^ bit<20>(m_PhysAddr) ^
        bit<21>(m_PhysAddr) ^ bit<22>(m_PhysAddr) ^ bit<23>(m_PhysAddr) ^ bit<24>(m_PhysAddr) ^ bit<25>(m_PhysAddr) ^
        bit<26>(m_PhysAddr) ^ bit<27>(m_PhysAddr) ^ bit<28>(m_PhysAddr) ^ bit<29>(m_PhysAddr) ^ bit<30>(m_PhysAddr) ^  bit<31>(m_PhysAddr));

    const UINT32 swizzle = static_cast<UINT32>(bits<15, 14>(m_PhysAddr) + bits<17, 16>(m_PhysAddr) + bits<19, 18>(m_PhysAddr) + bits<21, 20>(m_PhysAddr) +
        bits<23, 22>(m_PhysAddr) +  bits<25, 24>(m_PhysAddr) +  bits<27, 26>(m_PhysAddr) +
        bits<29, 28>(m_PhysAddr) +  bits<31, 30>(m_PhysAddr));

    m_PAKS = maskoff_lower_bits<12>(m_PhysAddr) |

        (((bits<9, 8>(m_PhysAddr) + bits<1, 0>(swizzle) ) & 3) << 10) |

        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle1)) << 9) |
        (bits<11, 10>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSPitchLinearPS4KParts6()
{
    m_PAKS = maskoff_lower_bits<12>(m_PhysAddr) |
        (bit<9>(m_PhysAddr) << 11) |

        // part[0]
        ((bit<8>(m_PhysAddr) ^ bit<9>(m_PhysAddr) ^ bit<10>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ bit<15>(m_PhysAddr) ^ bit<17>(m_PhysAddr)
          ^ bit<19>(m_PhysAddr) ^ bit<21>(m_PhysAddr) ^ bit<23>(m_PhysAddr) ^ bit<25>(m_PhysAddr) ^ bit<27>(m_PhysAddr)
          ^ bit<29>(m_PhysAddr) ^ bit<31>(m_PhysAddr)) << 10) |

        // slice[0]
        ((bit<7>(m_PhysAddr) ^ bit<12>(m_PhysAddr) ^ bit<14>(m_PhysAddr) ^ bit<16>(m_PhysAddr)
          ^ bit<18>(m_PhysAddr) ^ bit<20>(m_PhysAddr) ^ bit<22>(m_PhysAddr)
          ^ bit<24>(m_PhysAddr) ^ bit<26>(m_PhysAddr) ^ bit<28>(m_PhysAddr)
          ^ bit<30>(m_PhysAddr)) << 9) |

        (bits<11, 10>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSPitchLinearPS4KParts5()
{
    const UINT32 swizzle = static_cast<UINT32>(bits<13, 12>(m_PhysAddr) + bits<15, 14>(m_PhysAddr) +  bits<17, 16>(m_PhysAddr) +
        bits<19, 18>(m_PhysAddr) + bits<21, 20>(m_PhysAddr) +  bits<23, 22>(m_PhysAddr) +
        bits<25, 24>(m_PhysAddr) + bits<27, 26>(m_PhysAddr) +  bits<29, 28>(m_PhysAddr) +
        bits<31, 30>(m_PhysAddr));

    const UINT64 PhysAddrLev1 = maskoff_lower_bits<12>(m_PhysAddr) |

        // 4KB portion
        // partition[0]
        (bit<9>(m_PhysAddr) << 11) |

        // slice[1:0]
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        (bit<11>(m_PhysAddr) << 8) |
        (bit<10>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);

    CalcPAKSCommonPS4KParts5(PhysAddrLev1);
}

void GF100AddressDecode::CalcPAKSPitchLinearPS4KParts4()
{
    const UINT64 PhysAddrLev1 = maskoff_lower_bits<12>(m_PhysAddr) |
        // part[1:0]
        // cannot swizzle m_PhysAddr[8] with m_PhysAddr[12] derivatives without aliasing
        (((bits<9, 8>(m_PhysAddr)
           + bits<11, 10>(m_PhysAddr)
           + (bit<13>(m_PhysAddr) <<1)
           + bits<15, 14>(m_PhysAddr) + bits<17, 16>(m_PhysAddr) + bits<19, 18>(m_PhysAddr)
           + bits<21, 20>(m_PhysAddr) + bits<23, 22>(m_PhysAddr) + bits<25, 24>(m_PhysAddr)
           + bits<27, 26>(m_PhysAddr) + bits<29, 28>(m_PhysAddr) + bits<31, 30>(m_PhysAddr)

          )%4) << 10) |

        // slice[0]
        ((bit<7>(m_PhysAddr) ^ bit<13>(m_PhysAddr)) << 9) |

        // gob
        (bits<11, 10>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);

    CalcPAKSCommonPS4KParts4(PhysAddrLev1);
}

void GF100AddressDecode::CalcPAKSPitchLinearPS4KParts3()
{
    // both pitch and blocklinear 4KB mappings must swap bits 15, 11
    const UINT32 swizzle = static_cast<UINT32>(bits<12, 11>(m_PhysAddr) ^ bits<14, 13>(m_PhysAddr) ^  bits<16, 15>(m_PhysAddr) ^
        bits<18, 17>(m_PhysAddr) ^ bits<20, 19>(m_PhysAddr) ^  bits<22, 21>(m_PhysAddr) ^
        bits<24, 23>(m_PhysAddr) ^ bits<26, 25>(m_PhysAddr) ^  bits<28, 27>(m_PhysAddr) ^
        bits<30, 29>(m_PhysAddr));

    const UINT64 PhysAddrLev1 = maskoff_lower_bits<12>(m_PhysAddr) |

        // 4KB
        // part[0]
        (bit<9>(m_PhysAddr) << 11) |

        // slice[1:0]
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle) ^ bit<0>(swizzle)) << 10) |
        ((bit<7>(m_PhysAddr) ) << 9) |

        (bit<11>(m_PhysAddr) << 8) |
        (bit<10>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);

    CalcPAKSCommonPS4KParts3(PhysAddrLev1);
}

void GF100AddressDecode::CalcPAKSPitchLinearPS4KParts2()
{
    // looks a lot like 64KB, 128KB 2 partitions pitch
    const UINT32 swizzle = static_cast<UINT32>(bits<11, 10>(m_PhysAddr) + bits<13, 12>(m_PhysAddr) + bits<15, 14>(m_PhysAddr) +
        bits<17, 16>(m_PhysAddr) + bits<19, 18>(m_PhysAddr) + bits<21, 20>(m_PhysAddr) + bits<23, 22>(m_PhysAddr) +
        bits<25, 24>(m_PhysAddr) + bits<27, 26>(m_PhysAddr) + bits<29, 28>(m_PhysAddr) + bits<31, 30>(m_PhysAddr)
        );

    m_PAKS = maskoff_lower_bits<12>(m_PhysAddr) |
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle)) << 11) |

        (((bit<9>(m_PhysAddr) + bit<10>(m_PhysAddr) + bit<11>(m_PhysAddr) + bit<12>(m_PhysAddr) + bit<13>(m_PhysAddr)
           + bit<14>(m_PhysAddr) + bit<15>(m_PhysAddr) + bit<16>(m_PhysAddr) + bit<17>(m_PhysAddr) + bit<18>(m_PhysAddr)
           + bit<19>(m_PhysAddr) + bit<20>(m_PhysAddr) + bit<21>(m_PhysAddr) + bit<22>(m_PhysAddr) + bit<23>(m_PhysAddr)
           + bit<24>(m_PhysAddr) + bit<25>(m_PhysAddr) + bit<26>(m_PhysAddr) + bit<27>(m_PhysAddr) + bit<28>(m_PhysAddr)
           + bit<29>(m_PhysAddr) + bit<30>(m_PhysAddr) + bit<31>(m_PhysAddr)) %2) << 10) |

        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        (bits<11, 10>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSPitchLinearPS4KParts1()
{
    const UINT32 swizzle = static_cast<UINT32>(bits<10, 9>(m_PhysAddr) +
          bits<12, 11>(m_PhysAddr) + bits<14, 13>(m_PhysAddr) + bits<16, 15>(m_PhysAddr) +
          bits<18, 17>(m_PhysAddr) + bits<20, 19>(m_PhysAddr) + bits<22, 21>(m_PhysAddr) + 
          bits<24, 23>(m_PhysAddr) + bits<26, 25>(m_PhysAddr) + bits<28, 27>(m_PhysAddr) +
          bits<30, 29>(m_PhysAddr) + bit<31>(m_PhysAddr)
        );

    m_PAKS = maskoff_lower_bits<12>(m_PhysAddr) |

        (bit<9>(m_PhysAddr) << 11) |

        // slice[1:0]
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        (bits<11, 10>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSBlockLinearPS4KParts8()
{
    // Mapping with 4KB VM pages for vidmem is a galactic screw job.
    // part[2] and slice[1] are above the VM page.
    // Make it decent by m_PAKS[13] = m_PhysAddr[13] ^ m_PhysAddr[10] in both block linear and pitch

    // m_PAKS[8:0] = m_PhysAddr[8:0]                                                512B gob
    // m_PAKS[9] = m_PhysAddr[9] ^ m_PhysAddr[14]                                     slice[0]
    // m_PAKS[11:10] = m_PhysAddr[11:10] + m_PhysAddr[15:14] + m_PhysAddr[18:17] + ...  part[1:0]

    // Common mapping for both block linear and pitch
    // m_PAKS[12] = m_PhysAddr[12] ^ m_PhysAddr[13] ^ m_PhysAddr[16] ^ ...              part[2]
    // m_PAKS[13] = m_PhysAddr[13] ^ PhysAddrLev1[10] ^ m_PhysAddr[15] ^ ...              slice[1]

    const UINT64 PhysAddrLev1 = maskoff_lower_bits<12>(m_PhysAddr) |
        // part[1:0]
        // cannot swizzle m_PhysAddr[10] with m_PhysAddr[13] derivatives without aliasing
        (((bits<11, 10>(m_PhysAddr)
           + bits<15, 14>(m_PhysAddr) + bits<18, 17>(m_PhysAddr) + bits<21, 20>(m_PhysAddr)
           + bits<24, 23>(m_PhysAddr) + bits<27, 26>(m_PhysAddr) + bits<30, 29>(m_PhysAddr)
          )%4) << 10) |

        // slice[0]
        ((bit<9>(m_PhysAddr) ^ bit<14>(m_PhysAddr)) << 9) |

        // gob
        bits<8, 0>(m_PhysAddr);

    CalcPAKSCommonPS4KParts8(PhysAddrLev1);
}

void GF100AddressDecode::CalcPAKSBlockLinearPS4KParts7()
{
    m_PAKS = maskoff_lower_bits<10>(m_PhysAddr) |
        // swizzle slice[0]
        ((bit<9>(m_PhysAddr)  ^ bit<14>(m_PhysAddr)  ^ bit<13>(m_PhysAddr) ) << 9) |
        bits<8, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSBlockLinearPS4KParts6()
{
    m_PAKS = maskoff_lower_bits<14>(m_PhysAddr) |
        // slice[1]
        ((bit<13>(m_PhysAddr)) << 13) |

        // part[2:1] divide by 3 bits
        (((bit<12>(m_PhysAddr))&1) << 12) |
        (((bit<11>(m_PhysAddr))&1) << 11) |

        // part[0], power of 2 bit
        ((bit<10>(m_PhysAddr) ) << 10) | // post divide R swizzle

        // ((bit<10>(m_PhysAddr) ^ bit<14>(m_PhysAddr)) << 10) | // part = R

        // slice[0]
        ((bit<9>(m_PhysAddr)  ^ bit<13>(m_PhysAddr) ) << 9) |
        bits<8, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSBlockLinearPS4KParts5()
{
    // The constraint is to only swizzle 4KB of addressing.
    // As long as I don't swizzle outside 64KB, and I use the same PhysAddrLev1->m_PAKS
    // for both pitch and blocklinear, 4KB pages work.

    const UINT64 PhysAddrLev1 = maskoff_lower_bits<12>(m_PhysAddr) |

        // 4KB
        (bit<11>(m_PhysAddr) << 11) |
        // swizzle slices
        ((bit<10>(m_PhysAddr) ^ bit<13>(m_PhysAddr)) << 10) |
        ((bit<9>(m_PhysAddr) ^ bit<14>(m_PhysAddr)) << 9) |
        bits<8, 0>(m_PhysAddr);

    CalcPAKSCommonPS4KParts5(PhysAddrLev1);
}

void GF100AddressDecode::CalcPAKSBlockLinearPS4KParts4()
{
    const UINT32 CampFix = static_cast<UINT32>(bits<11, 10>(m_PhysAddr));
    const UINT64 PhysAddrLev1 = maskoff_lower_bits<12>(m_PhysAddr) |
        // part[1:0]
        // cannot swizzle m_PhysAddr[10] with m_PhysAddr[12] derivatives without aliasing
        (((CampFix
           + (bit<13>(m_PhysAddr) <<1)
           + bits<15, 14>(m_PhysAddr)

          )%4) << 10) |

        // slice[0]
        ((bit<9>(m_PhysAddr) ^ bit<13>(m_PhysAddr)) << 9) |

        // gob
        bits<8, 0>(m_PhysAddr);

    CalcPAKSCommonPS4KParts4(PhysAddrLev1);
}

void GF100AddressDecode::CalcPAKSBlockLinearPS4KParts3()
{
    const UINT32 CampFix = 0;
    const UINT64 PhysAddrLev1 = maskoff_lower_bits<12>(m_PhysAddr) |

        // 4KB
        // swizzle p[0]
        (bit<11>(m_PhysAddr) << 11) |
        // swizzle slices
        ((bit<10>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ CampFix) << 10) |
        ((bit<9>(m_PhysAddr) ^ bit<14>(m_PhysAddr)) << 9) |
        bits<8, 0>(m_PhysAddr);

    CalcPAKSCommonPS4KParts3(PhysAddrLev1);
}

void GF100AddressDecode::CalcPAKSBlockLinearPS4KParts2()
{
    const UINT32 CampFix = 0;
    const UINT32 BitX = static_cast<UINT32>(bit<15>(m_PhysAddr));
    const UINT32 swizzle = static_cast<UINT32>(bit<10>(m_PhysAddr) ^ bit<13>(m_PhysAddr));

    m_PAKS = maskoff_lower_bits<12>(m_PhysAddr) |

        ((bit<11>(m_PhysAddr) ^ bit<0>(swizzle) ^ bit<14>(m_PhysAddr) ^ BitX
         ) << 11) |

        ((bit<0>(swizzle) ^ bit<0>(CampFix)) << 10) |

        ((bit<9>(m_PhysAddr) ^ bit<13>(m_PhysAddr)) << 9) |
        bits<8, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSBlockLinearPS4KParts1()
{
    const UINT32 swizzle = static_cast<UINT32>(bits<14, 13>(m_PhysAddr) +
                                               bits<16, 15>(m_PhysAddr));

    m_PAKS = maskoff_lower_bits<11>(m_PhysAddr) |
        // don't want diagonals
        // using ^ here so that carry doesn't propagate into bit10 from changing bit9, allows
        // bit flipping in ROP
        ((bit<10>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<9>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        bits<8, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSCommonPS4KParts8(UINT64 Addr)
{
    // This section must be identical for pitch and block linear 8 partitions

    m_PAKS = maskoff_lower_bits<14>(Addr) |

        // slice[1]
        // want to interleave slices vertically up to the top because same swizzling is used by pitch
        ((bit<13>(Addr) ^ bit<10>(Addr)

          ^ bit<15>(Addr) ^ bit<16>(Addr) ^ bit<17>(Addr)
          ^ bit<18>(Addr) ^ bit<19>(Addr) ^ bit<20>(Addr) ^ bit<21>(Addr)
          ^ bit<22>(Addr) ^ bit<23>(Addr) ^ bit<24>(Addr) ^ bit<25>(Addr) ^ bit<26>(Addr)
          ^ bit<27>(Addr) ^ bit<28>(Addr) ^ bit<29>(Addr) ^ bit<30>(Addr) ^ bit<31>(Addr)

         ) << 13) |

        // part[2]
        ((bit<12>(Addr) ^
          bit<13>(Addr) ^ bit<16>(Addr) ^ bit<19>(Addr) ^ bit<22>(Addr) ^ bit<25>(Addr) ^
          bit<28>(Addr) ^ bit<31>(Addr)

         ) << 12) |

        bits<11, 0>(Addr);  // inside 4KB page
}

void GF100AddressDecode::CalcPAKSCommonPS4KParts5(UINT64 Addr)
{
    // This section must be identical for pitch and block linear 5 partitions

    m_PAKS = maskoff_lower_bits<14>(Addr) |
        (bit<13>(Addr)  << 13) |
        ((bit<12>(Addr) ^ bit<10>(Addr)) << 12) |
        bits<11, 0>(Addr);  // inside 4KB page
}

void GF100AddressDecode::CalcPAKSCommonPS4KParts4(UINT64 Addr)
{
    // This section must be identical for pitch and block linear 4 partitions

    const UINT32 CampFix = 0;
    m_PAKS = maskoff_lower_bits<13>(Addr) |

        // slice[1]
        // want to interleave slices vertically up to the top because same swizzling is used by pitch
        ((bit<12>(Addr) ^ CampFix ^ bit<10>(Addr)

          ^ bit<15>(Addr)

         ) << 12) |

        bits<11, 0>(Addr);  // inside 4KB page
}

void GF100AddressDecode::CalcPAKSCommonPS4KParts3(UINT64 Addr)
{
    // This section must be identical for pitch and block linear 3 partitions

    const UINT32 NotCampFix = static_cast<UINT32>(bit<10>(Addr) ^ bit<11>(Addr) ^ bit<13>(Addr));
    m_PAKS = maskoff_lower_bits<14>(Addr) |
        (bit<13>(Addr) << 13) |
        ((bit<12>(Addr) ^ NotCampFix) << 12) |
        bits<11, 0>(Addr);  // inside 4KB page
}

void GF100AddressDecode::CalcPAKSPitchLinearPS64KParts8()
{
    const UINT32 swizzle = static_cast<UINT32>(bits<10, 9>(m_PhysAddr)  + bits<12, 11>(m_PhysAddr) + bits<14, 13>(m_PhysAddr) +
        bits<16, 15>(m_PhysAddr) + bits<18, 17>(m_PhysAddr) + bits<20, 19>(m_PhysAddr) + bits<22, 21>(m_PhysAddr) +
        bits<24, 23>(m_PhysAddr) + bits<26, 25>(m_PhysAddr) + bits<28, 27>(m_PhysAddr) + bits<30, 29>(m_PhysAddr)+
        bit<31>(m_PhysAddr)
        );

    m_PAKS = maskoff_lower_bits<16>(m_PhysAddr) |
        (bits<13, 12>(m_PhysAddr) << 14) |
        ((
          bit<8>(m_PhysAddr) ^ bit<1>(swizzle)
         ) << 13) |
        (((bits<11, 9>(m_PhysAddr) + (bits<14, 12>(m_PhysAddr)) + bits<17, 15>(m_PhysAddr) + bits<20, 18>(m_PhysAddr) + bits<23, 21>(m_PhysAddr)
           + bits<26, 24>(m_PhysAddr) + bits<29, 27>(m_PhysAddr) + bits<31, 30>(m_PhysAddr)) %8) << 10) |

        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |
        (bits<15, 14>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSPitchLinearPS64KParts7()
{
    const UINT32 swizzle1 = static_cast<UINT32>(bit<11>(m_PhysAddr) ^  bit<12>(m_PhysAddr) ^  bit<13>(m_PhysAddr) ^ bit<14>(m_PhysAddr) ^ bit<15>(m_PhysAddr) ^
        bit<16>(m_PhysAddr) ^ bit<17>(m_PhysAddr) ^ bit<18>(m_PhysAddr) ^ bit<19>(m_PhysAddr) ^ bit<20>(m_PhysAddr) ^
        bit<21>(m_PhysAddr) ^ bit<22>(m_PhysAddr) ^ bit<23>(m_PhysAddr) ^ bit<24>(m_PhysAddr) ^ bit<25>(m_PhysAddr) ^
        bit<26>(m_PhysAddr) ^ bit<27>(m_PhysAddr) ^ bit<28>(m_PhysAddr) ^ bit<29>(m_PhysAddr) ^ bit<30>(m_PhysAddr) ^  bit<31>(m_PhysAddr));

    const UINT32 swizzle = static_cast<UINT32>(bits<16, 14>(m_PhysAddr) + bits<19, 17>(m_PhysAddr) + bits<22, 20>(m_PhysAddr) +
        bits<25, 23>(m_PhysAddr) + bits<28, 26>(m_PhysAddr) + bits<31, 29>(m_PhysAddr));

    m_PAKS = maskoff_lower_bits<16>(m_PhysAddr) |
        (bits<13, 12>(m_PhysAddr) << 14) |
        ((bit<8>(m_PhysAddr)) << 13) |

        (((bits<11, 9>(m_PhysAddr) + bits<2, 0>(swizzle)) & 7) << 10) |

        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle1)) << 9) |
        (bits<15, 14>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSPitchLinearPS64KParts6()
{
    m_PAKS = maskoff_lower_bits<16>(m_PhysAddr) |
        (bits<13, 9>(m_PhysAddr) << 11) |

        // part[0]
        ((bit<8>(m_PhysAddr) ^ bit<9>(m_PhysAddr) ^ bit<10>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ bit<15>(m_PhysAddr) ^ bit<17>(m_PhysAddr)
          ^ bit<19>(m_PhysAddr) ^ bit<21>(m_PhysAddr) ^ bit<23>(m_PhysAddr) ^ bit<25>(m_PhysAddr) ^ bit<27>(m_PhysAddr)
          ^ bit<29>(m_PhysAddr) ^ bit<31>(m_PhysAddr)) << 10) |

        // slice[0]
        ((bit<7>(m_PhysAddr) ^ bit<12>(m_PhysAddr) ^ bit<14>(m_PhysAddr) ^ bit<16>(m_PhysAddr)
          ^ bit<18>(m_PhysAddr) ^ bit<20>(m_PhysAddr) ^ bit<22>(m_PhysAddr)
          ^ bit<24>(m_PhysAddr) ^ bit<26>(m_PhysAddr) ^ bit<28>(m_PhysAddr)
          ^ bit<30>(m_PhysAddr)) << 9) |

        (bits<15, 14>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSPitchLinearPS64KParts5()
{
    const UINT32 swizzle = static_cast<UINT32>(bits<13, 12>(m_PhysAddr) + bits<15, 14>(m_PhysAddr) +  bits<17, 16>(m_PhysAddr) +
        bits<19, 18>(m_PhysAddr) + bits<21, 20>(m_PhysAddr) +  bits<23, 22>(m_PhysAddr) +
        bits<25, 24>(m_PhysAddr) + bits<27, 26>(m_PhysAddr) +  bits<29, 28>(m_PhysAddr) +
        bits<31, 30>(m_PhysAddr));

    m_PAKS = maskoff_lower_bits<16>(m_PhysAddr) |
        (bits<13, 12>(m_PhysAddr) << 14) |
        // partitions
        (bits<11, 9>(m_PhysAddr) << 11) |

        // slice[1:0]
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        (bits<15, 14>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSPitchLinearPS64KParts4()
{
    const UINT32 swizzle = static_cast<UINT32>(bits<12, 11>(m_PhysAddr) + bits<14, 13>(m_PhysAddr) + bits<16, 15>(m_PhysAddr) + bits<18, 17>(m_PhysAddr) + bits<20, 19>(m_PhysAddr) +
        bits<22, 21>(m_PhysAddr) + bits<24, 23>(m_PhysAddr) + bits<26, 25>(m_PhysAddr) + bits<28, 27>(m_PhysAddr) + bits<30, 29>(m_PhysAddr) +
        bit<31>(m_PhysAddr));

    m_PAKS = maskoff_lower_bits<16>(m_PhysAddr) |
        (bits<13, 11>(m_PhysAddr) << 13) |
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle)) << 12) |

        (((bits<10, 9>(m_PhysAddr)
           + bits<1, 0>(swizzle)  ) %4) << 10) |

        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |
        (bits<15, 14>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSPitchLinearPS64KParts3()
{
    const UINT32 swizzle = static_cast<UINT32>(bits<12, 11>(m_PhysAddr) + bits<14, 13>(m_PhysAddr) +  bits<16, 15>(m_PhysAddr) +
        bits<18, 17>(m_PhysAddr) + bits<20, 19>(m_PhysAddr) +  bits<22, 21>(m_PhysAddr) +
        bits<24, 23>(m_PhysAddr) + bits<26, 25>(m_PhysAddr) +  bits<28, 27>(m_PhysAddr) +
        bits<30, 29>(m_PhysAddr));

    m_PAKS = maskoff_lower_bits<16>(m_PhysAddr) |
        (bits<13, 11>(m_PhysAddr) << 13) |
        // partitions
        (bits<10, 9>(m_PhysAddr) << 11) |

        // slice[1:0]
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        (bits<15, 14>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSPitchLinearPS64KParts2()
{
    const UINT32 swizzle = static_cast<UINT32>(bits<11, 10>(m_PhysAddr) + bits<13, 12>(m_PhysAddr) + bits<15, 14>(m_PhysAddr) +
        bits<17, 16>(m_PhysAddr) + bits<19, 18>(m_PhysAddr) + bits<21, 20>(m_PhysAddr) + bits<23, 22>(m_PhysAddr) +
        bits<25, 24>(m_PhysAddr) + bits<27, 26>(m_PhysAddr) + bits<29, 28>(m_PhysAddr) + bits<31, 30>(m_PhysAddr)
        );

    m_PAKS = maskoff_lower_bits<16>(m_PhysAddr) |
        (bits<13, 10>(m_PhysAddr) << 12) |
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle)) << 11) |

        (((bit<9>(m_PhysAddr) + bit<10>(m_PhysAddr) + bit<11>(m_PhysAddr) + bit<12>(m_PhysAddr) + bit<13>(m_PhysAddr)
           + bit<14>(m_PhysAddr) + bit<15>(m_PhysAddr) + bit<16>(m_PhysAddr) + bit<17>(m_PhysAddr) + bit<18>(m_PhysAddr)
           + bit<19>(m_PhysAddr) + bit<20>(m_PhysAddr) + bit<21>(m_PhysAddr) + bit<22>(m_PhysAddr) + bit<23>(m_PhysAddr)
           + bit<24>(m_PhysAddr) + bit<25>(m_PhysAddr) + bit<26>(m_PhysAddr) + bit<27>(m_PhysAddr) + bit<28>(m_PhysAddr)
           + bit<29>(m_PhysAddr) + bit<30>(m_PhysAddr) + bit<31>(m_PhysAddr)) %2) << 10) |

        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        (bits<15, 14>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSPitchLinearPS64KParts1()
{
    const UINT32 swizzle = static_cast<UINT32>(bits<10, 9>(m_PhysAddr) +
        bits<12, 11>(m_PhysAddr) + bits<14, 13>(m_PhysAddr) + bits<16, 15>(m_PhysAddr) + 
        bits<18, 17>(m_PhysAddr) + bits<20, 19>(m_PhysAddr) + bits<22, 21>(m_PhysAddr) +
        bits<24, 23>(m_PhysAddr) + bits<26, 25>(m_PhysAddr) + bits<28, 27>(m_PhysAddr) +
        bits<30, 29>(m_PhysAddr) + bit<31>(m_PhysAddr)
        );

    m_PAKS = maskoff_lower_bits<16>(m_PhysAddr) |
        (bits<13, 9>(m_PhysAddr) << 11) |

        // slice[1:0]
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        (bits<15, 14>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSBlockLinearPS64KParts8()
{
    UINT32 swizzle=0;
    if (PartSwizzle && SliceSwizzle)
    {
        UINT32 swizzle=0;
        // Important trick:
        // swizzle[2:0] are the partition bits.
        // Slice bit is above partitions, and I need to swizzle it with partition bits so it alternates
        // as I step through partitions.  The trick is to ^ with ALL partition bits so I get a long pattern.

        swizzle = static_cast<UINT32>(bits<12, 10>(m_PhysAddr) + bits<15, 13>(m_PhysAddr)*5 + bits<18, 16>(m_PhysAddr)
            + bits<21, 19>(m_PhysAddr)
            + bits<24, 22>(m_PhysAddr)
            + bits<27, 25>(m_PhysAddr)
            + bits<30, 28>(m_PhysAddr));

        m_PAKS = maskoff_lower_bits<14>(m_PhysAddr) |
            // want to interleave slices vertically across all pitches up to 256MB
            // using trick here
            ((bit<13>(m_PhysAddr) ^ bit<0>(swizzle) ^ bit<1>(swizzle) ^ bit<2>(swizzle)

              ^ bit<18>(m_PhysAddr)  ^ bit<20>(m_PhysAddr) ^ bit<21>(m_PhysAddr)
              ^ bit<23>(m_PhysAddr) ^ bit<24>(m_PhysAddr) ^ bit<26>(m_PhysAddr)
              ^ bit<27>(m_PhysAddr)

             ) << 13) |

            (bits<2, 0>(swizzle) << 10) |

            ((bit<9>(m_PhysAddr) ^ bit<14>(m_PhysAddr)) << 9) |
            bits<8, 0>(m_PhysAddr);

        /*
        //Swizzle partitions to top of memory for texture starting address variation.
        swizzle = bits<12, 10>(m_PhysAddr)
        + (bit<15>(m_PhysAddr) << 0)
        + (bit<14>(m_PhysAddr) << 1)
        + (bit<13>(m_PhysAddr) << 2)
        + bits<18, 16>(m_PhysAddr)
        + bits<21, 19>(m_PhysAddr)
        + bits<24, 22>(m_PhysAddr)
        + bits<27, 25>(m_PhysAddr)
        + bits<30, 28>(m_PhysAddr);

        // 2 slice, slice = 1KB partition
        //This looks better for the slices.

        m_PAKS = maskoff_lower_bits<14>(m_PhysAddr) |
        // want to interleave slices vertically across all pitches up to 256MB
        ((bit<13>(m_PhysAddr) ^ bit<0>(swizzle)
        ^ bit<17>(m_PhysAddr) ^ bit<18>(m_PhysAddr) ^ bit<20>(m_PhysAddr) ^ bit<21>(m_PhysAddr) ^ bit<23>(m_PhysAddr) ^ bit<24>(m_PhysAddr)
        ^ bit<26>(m_PhysAddr) ^ bit<27>(m_PhysAddr) ^ bit<29>(m_PhysAddr) ^ bit<30>(m_PhysAddr)
        ) << 13) |

        (bits<2, 0>(swizzle) << 10) |

        ((bit<9>(m_PhysAddr) ^ bit<14>(m_PhysAddr)) << 9) |
        bits<8, 0>(m_PhysAddr);
        */
    }
    else if (!PartSwizzle && SliceSwizzle)
    {
        swizzle = static_cast<UINT32>(bits<12, 10>(m_PhysAddr) + bits<15, 13>(m_PhysAddr)*5 + bits<17, 16>(m_PhysAddr));

        m_PAKS = maskoff_lower_bits<14>(m_PhysAddr) |
            // want to interleave slices vertically across all pitches up to 256MB
            ((bit<13>(m_PhysAddr) ^ bit<0>(swizzle) ^ bit<1>(swizzle) ^ bit<2>(swizzle)

              ^ bit<18>(m_PhysAddr)  ^ bit<19>(m_PhysAddr) ^ bit<20>(m_PhysAddr) ^ bit<21>(m_PhysAddr)
              ^ bit<22>(m_PhysAddr) ^ bit<23>(m_PhysAddr) ^ bit<24>(m_PhysAddr) ^ bit<25>(m_PhysAddr) ^ bit<26>(m_PhysAddr)
              ^ bit<27>(m_PhysAddr)

             ) << 13) |

            (bits<2, 0>(swizzle) << 10) |

            ((bit<9>(m_PhysAddr) ^ bit<14>(m_PhysAddr)) << 9) |
            bits<8, 0>(m_PhysAddr);

        /* crap
           swizzle = bits<12, 10>(m_PhysAddr)
           + (bit<15>(m_PhysAddr) << 0)
           + (bit<14>(m_PhysAddr) << 1)
           + (bit<13>(m_PhysAddr) << 2)
           + bits<17, 16>(m_PhysAddr);

           m_PAKS = maskoff_lower_bits<14>(m_PhysAddr) |
        // want to interleave slices vertically across all pitches up to 256MB
        ((bit<13>(m_PhysAddr) ^ bit<0>(swizzle)
        ^ bit<17>(m_PhysAddr) ^ bit<18>(m_PhysAddr) ^ bit<19>(m_PhysAddr) ^ bit<20>(m_PhysAddr) ^ bit<21>(m_PhysAddr) ^ bit<22>(m_PhysAddr) ^ bit<23>(m_PhysAddr) ^ bit<24>(m_PhysAddr)
        ^ bit<25>(m_PhysAddr) ^ bit<26>(m_PhysAddr) ^ bit<27>(m_PhysAddr) ^ bit<28>(m_PhysAddr) ^ bit<29>(m_PhysAddr) ^ bit<30>(m_PhysAddr)
        ) << 13) |

        (bits<2, 0>(swizzle) << 10) |

        ((bit<9>(m_PhysAddr) ^ bit<14>(m_PhysAddr)) << 9) |
        bits<8, 0>(m_PhysAddr);
        */
    }
    else if (PartSwizzle && !SliceSwizzle)
    {
        swizzle = static_cast<UINT32>(bits<12, 10>(m_PhysAddr) + bits<15, 13>(m_PhysAddr)*5 + bits<18, 16>(m_PhysAddr)
            + bits<21, 19>(m_PhysAddr)
            + bits<24, 22>(m_PhysAddr)
            + bits<27, 25>(m_PhysAddr)
            + bits<30, 28>(m_PhysAddr));

        m_PAKS = maskoff_lower_bits<14>(m_PhysAddr) |
            // want to interleave slices vertically across all pitches up to 256MB
            ((bit<13>(m_PhysAddr) ^ bit<0>(swizzle)

              ^ bit<18>(m_PhysAddr)  ^ bit<20>(m_PhysAddr) ^ bit<21>(m_PhysAddr)
              ^ bit<23>(m_PhysAddr) ^ bit<24>(m_PhysAddr) ^ bit<26>(m_PhysAddr)
              ^ bit<27>(m_PhysAddr)

             ) << 13) |

            (bits<2, 0>(swizzle) << 10) |

            ((bit<9>(m_PhysAddr) ^ bit<14>(m_PhysAddr)) << 9) |
            bits<8, 0>(m_PhysAddr);

    }
    else
    {  // old way
        swizzle = static_cast<UINT32>(bits<12, 10>(m_PhysAddr) + bits<15, 13>(m_PhysAddr)*5 + bits<17, 16>(m_PhysAddr));

        m_PAKS = maskoff_lower_bits<14>(m_PhysAddr) |
            // want to interleave slices vertically across all pitches up to 256MB
            ((bit<13>(m_PhysAddr) ^ bit<0>(swizzle)

              ^ bit<18>(m_PhysAddr)  ^ bit<19>(m_PhysAddr) ^ bit<20>(m_PhysAddr) ^ bit<21>(m_PhysAddr)
              ^ bit<22>(m_PhysAddr) ^ bit<23>(m_PhysAddr) ^ bit<24>(m_PhysAddr) ^ bit<25>(m_PhysAddr) ^ bit<26>(m_PhysAddr)
              ^ bit<27>(m_PhysAddr)

             ) << 13) |

            (bits<2, 0>(swizzle) << 10) |

            ((bit<9>(m_PhysAddr) ^ bit<14>(m_PhysAddr)) << 9) |
            bits<8, 0>(m_PhysAddr);
    }

    if (SliceLowerBit)
    {

        // 2 slice, slices share partition.  Slice = 512B gob.
        // subpartition imbalance.

        m_PAKS = maskoff_lower_bits<13>(m_PhysAddr) |

            // partition
            (bits<2, 0>(swizzle) << 10) |

            // slice 0
            ((bit<9>(m_PhysAddr) ^ bit<13>(m_PhysAddr)) << 9) |
            bits<8, 0>(m_PhysAddr);
    }
}

void GF100AddressDecode::CalcPAKSBlockLinearPS64KParts7()
{
    m_PAKS = maskoff_lower_bits<10>(m_PhysAddr) |
        // swizzle slice[0]
        ((bit<9>(m_PhysAddr)  ^ bit<14>(m_PhysAddr)  ^ bit<13>(m_PhysAddr) ) << 9) |
        bits<8, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSBlockLinearPS64KParts6()
{
    m_PAKS = maskoff_lower_bits<14>(m_PhysAddr) |
        // slice[1]
        ((bit<13>(m_PhysAddr)) << 13) |

        // part[2:1] divide by 3 bits
        (((bit<12>(m_PhysAddr))&1) << 12) |
        (((bit<11>(m_PhysAddr))&1) << 11) |

        // part[0], power of 2 bit
        ((bit<10>(m_PhysAddr) ) << 10) | // post divide R swizzle

        // ((bit<10>(m_PhysAddr) ^ bit<14>(m_PhysAddr)) << 10) | // part = R

        // slice[0]
        ((bit<9>(m_PhysAddr)  ^ bit<13>(m_PhysAddr) ) << 9) |
        bits<8, 0>(m_PhysAddr);

    if (SliceLowerBit)
    {

        // 2 slice, slices share partition.  Slice = 512B gob.
        // subpartition imbalance.

        m_PAKS = maskoff_lower_bits<13>(m_PhysAddr) |

            // partition
            (bits<12, 10>(m_PhysAddr) << 10) |

            // slice 0
            ((bit<9>(m_PhysAddr) ^ bit<13>(m_PhysAddr)) << 9) |
            bits<8, 0>(m_PhysAddr);
    }
}

void GF100AddressDecode::CalcPAKSBlockLinearPS64KParts5()
{
    m_PAKS = maskoff_lower_bits<16>(m_PhysAddr) |
        (bits<14, 13>(m_PhysAddr) << 14) |
        ((~bit<12>(m_PhysAddr)&1 ) << 13) |
        ((bit<11>(m_PhysAddr)&1 ) << 12) |
        ((bit<10>(m_PhysAddr)&1 ) << 11) |
        // swizzle slices
        ((bit<15>(m_PhysAddr) ^ bit<10>(m_PhysAddr)  ^ bit<14>(m_PhysAddr) ) << 10) |
        ((bit<9>(m_PhysAddr) ^ bit<13>(m_PhysAddr)) << 9) |
        bits<8, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSBlockLinearPS64KParts4()
{
    const UINT32 SwizzleTable[4][4] =
    {
        { 0, 1, 2, 3 },
        { 2, 3, 0, 1 },
        { 1, 2, 3, 0 },
        { 3, 0, 1, 2 }
    };

    // swizzle = SwizzleTable[ y[1:0] ][ x[1:0] ];
    const UINT32 SwizzleSel = static_cast<UINT32>(bits<11, 10>(m_PhysAddr));
    const UINT32 swizzle = SwizzleTable[ bits<14, 13>(m_PhysAddr) ][ SwizzleSel ];

    const UINT32 PAKSSwizzle = static_cast<UINT32>(bit<12>(m_PhysAddr) ^ bit<0>(swizzle));
    m_PAKS = maskoff_lower_bits<13>(m_PhysAddr) |

        ((PAKSSwizzle ^ bit<15>(m_PhysAddr)
         ) << 12) |

        (bits<1, 0>(swizzle) << 10) |

        ((bit<9>(m_PhysAddr) ^ bit<13>(m_PhysAddr)) << 9) |
        bits<8, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSBlockLinearPS64KParts3()
{
    const UINT32 CampFix = 0;
    m_PAKS = maskoff_lower_bits<16>(m_PhysAddr) |
        (bits<14, 12>(m_PhysAddr) << 13) |
        ((bit<11>(m_PhysAddr)&1 ) << 12) |
        ((bit<10>(m_PhysAddr)&1 ) << 11) |
        // swizzle slices
        ((bit<15>(m_PhysAddr) ^ bit<10>(m_PhysAddr) ^ CampFix ) << 10) |
        ((bit<9>(m_PhysAddr) ^ bit<13>(m_PhysAddr)) << 9) |
        bits<8, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSBlockLinearPS64KParts2()
{
    const UINT32 CampFix = 0;
    const UINT32 BitX = static_cast<UINT32>(bit<15>(m_PhysAddr));
    const UINT32 swizzle = static_cast<UINT32>(bit<10>(m_PhysAddr) ^ bit<13>(m_PhysAddr));

    m_PAKS = maskoff_lower_bits<12>(m_PhysAddr) |

        ((bit<11>(m_PhysAddr) ^ bit<0>(swizzle) ^ bit<14>(m_PhysAddr) ^ BitX
         ) << 11) |

        ((bit<0>(swizzle) ^ bit<0>(CampFix)) << 10) |

        ((bit<9>(m_PhysAddr) ^ bit<13>(m_PhysAddr)) << 9) |
        bits<8, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcPAKSBlockLinearPS64KParts1()
{
    const UINT32 swizzle = static_cast<UINT32>(bits<14, 13>(m_PhysAddr) + 
                                               bits<16, 15>(m_PhysAddr));

    m_PAKS = maskoff_lower_bits<11>(m_PhysAddr) |
        // don't want diagonals
        // using ^ here so that carry doesn't propagate into bit10 from changing bit9, allows bit
        // flipping in ROP
        ((bit<10>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<9>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        bits<8, 0>(m_PhysAddr);
}

void GF100AddressDecode::CalcL2SetSlices4L2Sets16Parts876421()
{
    m_L2Set = static_cast<UINT32>((bit<7>(m_Offset) ^ bit<3>(m_Quotient) ^ bit<7>(m_Quotient) ^ bit<11>(m_Quotient) ^ bit<15>(m_Quotient)) |
        ((bit<8>(m_Offset) ^ bit<4>(m_Quotient) ^ bit<8>(m_Quotient) ^ bit<12>(m_Quotient) ^ bit<16>(m_Quotient)) << 1) |
        (bits<1, 0>(bits<3, 1>(m_Quotient) + bits<7, 5>(m_Quotient) + bits<11, 9>(m_Quotient) + bits<15, 13>(m_Quotient) + bit<17>(m_Quotient)) << 2));
    m_L2Tag = static_cast<UINT32>(bit<2>(bits<3, 1>(m_Quotient) + bits<7, 5>(m_Quotient) + bits<11, 9>(m_Quotient) + bits<15, 13>(m_Quotient) + bit<17>(m_Quotient)));
    // additional swizzling                             + ((bit<4>(m_Quotient) + bit<8>(m_Quotient) + bit<12>(m_Quotient) + bit<16>(m_Quotient)) << 2));
    m_L2Tag |= ((bits<31, 4>(m_Quotient)) << 1); // ors with bit zero prepared in case above

    CalcL2Sector();
}

void GF100AddressDecode::CalcL2SetSlices4L2Sets16Parts53()
{
    m_L2Set = static_cast<UINT32>((bit<7>(m_Offset) ^ bit<2>(m_Quotient) ^ bit<6>(m_Quotient) ^ bit<10>(m_Quotient) ^ bit<14>(m_Quotient)) |
        ((bit<8>(m_Offset) ^ bit<3>(m_Quotient) ^ bit<7>(m_Quotient) ^ bit<11>(m_Quotient) ^ bit<15>(m_Quotient)) << 1) |
        (bits<1, 0>(bits<2, 0>(m_Quotient) + bits<6, 4>(m_Quotient) + bits<10, 8>(m_Quotient) + bits<14, 12>(m_Quotient) + bit<16>(m_Quotient)) << 2));
    m_L2Tag = static_cast<UINT32>(bit<2>(bits<2, 0>(m_Quotient) + bits<6, 4>(m_Quotient) + bits<10, 8>(m_Quotient) + bits<14, 12>(m_Quotient) + bit<16>(m_Quotient)));
    // additional swizzling                             + ((bit<3>(m_Quotient) + bit<7>(m_Quotient) + bit<11>(m_Quotient) + bit<15>(m_Quotient)) << 2));
    m_L2Tag |= ((bits<31, 3>(m_Quotient)) << 1); // ors with bit zero prepared in case above

    CalcL2Sector();
}

void GF100AddressDecode::CalcL2SetSlices4L2Sets32Parts876421()
{
    /*
      L2_Index[0] = O[7] ^ Q[4] ^ Q[9] ^ Q[14]
      L2_Index[1] = O[8] ^ Q[5] ^ Q[10] ^ Q[15]

      bank[2:0]
      L2_Index[4:2] = Q[3:1] + Q[8:6] + Q[13:11] + Q[17:16]
      + don't know what to do here for banks

      L2_tag[t-1:0] = Q[q-1:4]
    */
    m_L2Set = static_cast<UINT32>((bit<7>(m_Offset) ^ bit<4>(m_Quotient) ^ bit<9>(m_Quotient) ^ bit<14>(m_Quotient)) |
        ((bit<8>(m_Offset) ^ bit<5>(m_Quotient) ^ bit<10>(m_Quotient) ^ bit<15>(m_Quotient)) << 1) |
        (bits<2, 0>(bits<3, 1>(m_Quotient) + bits<8, 6>(m_Quotient) + bits<13, 11>(m_Quotient) + bits<17, 16>(m_Quotient)) << 2));
    m_L2Tag = static_cast<UINT32>(bits<31, 4>(m_Quotient)); // ors with bit zero prepared in case above

    CalcL2Sector();
}

void GF100AddressDecode::CalcL2SetSlices4L2Sets32Parts53()
{
    /*
      L2_Index[0] = O[7] ^ Q[3] ^ Q[8] ^ Q[13]
      L2_Index[1] = O[8] ^ Q[4] ^ Q[9] ^ Q[14]

      L2_Index[4:2] = Q[2:0] + Q[7:5] + Q[12:10] + Q[16:15]
      + don't know what to do here for banks

      L2_tag[t-1:0] = Q[q-1:3]
    */
    m_L2Set = static_cast<UINT32>((bit<7>(m_Offset) ^ bit<3>(m_Quotient) ^ bit<8>(m_Quotient) ^ bit<13>(m_Quotient)) |
        ((bit<8>(m_Offset) ^ bit<4>(m_Quotient) ^ bit<9>(m_Quotient) ^ bit<14>(m_Quotient)) << 1) |
        (bits<2, 0>(bits<2, 0>(m_Quotient) + bits<7, 5>(m_Quotient) + bits<12, 10>(m_Quotient) + bits<16, 15>(m_Quotient)) << 2));
    m_L2Tag = static_cast<UINT32>(bits<31, 3>(m_Quotient)); // ors with bit zero prepared in case above

    CalcL2Sector();
}

void GF100AddressDecode::CalcL2SetSlices2L2Sets16Parts876421()
{
    m_L2Set = static_cast<UINT32>((bit<7>(m_Offset) ^ bit<2>(m_Quotient) ^ bit<6>(m_Quotient) ^ bit<10>(m_Quotient) ^ bit<14>(m_Quotient)) |
        ((bit<8>(m_Offset) ^ bit<4>(m_Quotient) ^ bit<8>(m_Quotient) ^ bit<12>(m_Quotient) ^ bit<16>(m_Quotient)) << 1) |
        ((bit<9>(m_Offset) ^ bit<3>(m_Quotient) ^ bit<7>(m_Quotient) ^ bit<11>(m_Quotient) ^ bit<15>(m_Quotient)) << 2) |

        (bit<0>(bits<3, 1>(m_Quotient) + bits<7, 5>(m_Quotient) + bits<11, 9>(m_Quotient) + bits<15, 13>(m_Quotient) + bit<17>(m_Quotient)
                + (bit<8>(m_Quotient) << 2) + (bit<4>(m_Quotient) << 1) + (bit<16>(m_Quotient) << 2) + (bit<12>(m_Quotient) << 1) ) << 3));

    m_L2Tag = static_cast<UINT32>(bits<2, 1>(bits<3, 1>(m_Quotient) + bits<7, 5>(m_Quotient) + bits<11, 9>(m_Quotient) + bits<15, 13>(m_Quotient) + bit<17>(m_Quotient)
                        + (bit<8>(m_Quotient) << 2) + (bit<4>(m_Quotient) << 1) + (bit<16>(m_Quotient) << 2) + (bit<12>(m_Quotient) << 1) ));

    m_L2Tag |= ((bits<31, 4>(m_Quotient)) << 2); // ors with bit zero prepared in case above

    CalcL2Sector();
}

void GF100AddressDecode::CalcL2SetSlices2L2Sets16Parts53()
{
    m_L2Set = static_cast<UINT32>((bit<7>(m_Offset) ^ bit<1>(m_Quotient) ^ bit<5>(m_Quotient) ^ bit<9>(m_Quotient) ^ bit<13>(m_Quotient)) |
        ((bit<8>(m_Offset) ^ bit<3>(m_Quotient) ^ bit<7>(m_Quotient) ^ bit<11>(m_Quotient) ^ bit<15>(m_Quotient)) << 1) |
        ((bit<9>(m_Offset) ^ bit<2>(m_Quotient) ^ bit<6>(m_Quotient) ^ bit<10>(m_Quotient) ^ bit<14>(m_Quotient)) << 2) |

        (bit<0>(bits<2, 0>(m_Quotient) + bits<6, 4>(m_Quotient) + bits<10, 8>(m_Quotient) + bits<14, 12>(m_Quotient) + bit<16>(m_Quotient)
                + (bit<7>(m_Quotient) << 2) + (bit<3>(m_Quotient) << 1) + (bit<15>(m_Quotient) << 2) + (bit<11>(m_Quotient) << 1) ) << 3));

    m_L2Tag = static_cast<UINT32>(bits<2, 1>(bits<2, 0>(m_Quotient) + bits<6, 4>(m_Quotient) + bits<10, 8>(m_Quotient) + bits<14, 12>(m_Quotient) + bit<16>(m_Quotient)
            + (bit<7>(m_Quotient) << 2) + (bit<3>(m_Quotient) << 1) + (bit<15>(m_Quotient) << 2) + (bit<11>(m_Quotient) << 1) ));

    m_L2Tag |= ((bits<31, 3>(m_Quotient)) << 2); // ors with bit zero prepared in case above

    CalcL2Sector();
}

void GF100AddressDecode::CalcL2SetSlices2L2Sets32Parts876421()
{
    const UINT32 common = static_cast<UINT32>(
        bits<3, 1>(m_Quotient)   + bits<8, 6>(m_Quotient) + bits<13, 11>(m_Quotient) +
        bits<17, 16>(m_Quotient) + 
        ((bit<4>(m_Quotient) ^ bit<5>(m_Quotient) ^ bit<9>(m_Quotient) ^ bit<10>(m_Quotient) ^
          bit<14>(m_Quotient) ^ bit<15>(m_Quotient)) << 2));

    if (SliceLowerBit)
    {
        m_L2Set =  static_cast<UINT32>(
             (bit<7>(m_Offset) ^ bit<3>(m_Quotient) ^ bit<8>(m_Quotient) ^ bit<13>(m_Quotient)) |
            ((bit<8>(m_Offset) ^ bit<4>(m_Quotient) ^ bit<9>(m_Quotient) ^ bit<14>(m_Quotient)) << 1) |     //$
            ((bit<0>(m_Quotient) ^ bit<5>(m_Quotient) ^ bit<10>(m_Quotient) ^ bit<15>(m_Quotient)) << 2) |  //$
            (bits<1, 0>(common) << 3));
    }
    else
    {
        m_L2Set =  static_cast<UINT32>(
             (bit<7>(m_Offset) ^ bit<3>(m_Quotient) ^ bit<8>(m_Quotient) ^ bit<13>(m_Quotient)) |
            ((bit<8>(m_Offset) ^ bit<4>(m_Quotient) ^ bit<9>(m_Quotient) ^ bit<14>(m_Quotient)) << 1) |  //$
            ((bit<9>(m_Offset) ^ bit<5>(m_Quotient) ^ bit<10>(m_Quotient) ^ bit<15>(m_Quotient)) << 2) | //$
            (bits<1, 0>(common) << 3));
    }
    m_L2Tag = static_cast<UINT32>(bit<2>(common) |
        (bits<31, 4>(m_Quotient) << 1));

    CalcL2Sector();
}

void GF100AddressDecode::CalcL2SetSlices2L2Sets32Parts53()
{
    const UINT32 common = static_cast<UINT32>(bits<2, 0>(m_Quotient) + bits<7, 5>(m_Quotient) + bits<12, 10>(m_Quotient) + bits<16, 15>(m_Quotient)
        + ((bit<3>(m_Quotient) ^ bit<4>(m_Quotient) ^ bit<8>(m_Quotient) ^ bit<9>(m_Quotient) ^ bit<13>(m_Quotient) ^ bit<14>(m_Quotient)) << 2));

    m_L2Set =  static_cast<UINT32>((bit<7>(m_Offset) ^ bit<2>(m_Quotient) ^ bit<7>(m_Quotient) ^ bit<12>(m_Quotient)) |
        ((bit<8>(m_Offset) ^ bit<3>(m_Quotient) ^ bit<8>(m_Quotient) ^ bit<13>(m_Quotient)) << 1) |
        ((bit<9>(m_Offset) ^ bit<4>(m_Quotient) ^ bit<9>(m_Quotient) ^ bit<14>(m_Quotient)) << 2) |
        (bits<1, 0>(common) << 3));

    m_L2Tag = static_cast<UINT32>(bit<2>(common) |
        (bits<31, 3>(m_Quotient) << 1));

    CalcL2Sector();
}

void GF100AddressDecode::CalcL2Sector()
{
    //sector mapping is just about address offset, not L2 bank.
    m_L2Sector = (m_Offset / m_FBDataBusWidth) % m_NumL2Sectors;

    if (m_DRAMBankSwizzle)
    {
        m_L2Bank = (bits<2, 1>(m_L2Set) + bit<3>(m_L2Set) + m_L2Sector) % m_NumL2Banks;
    }
    else
    {
        m_L2Bank = m_L2Sector % m_NumL2Banks;  // without L2 bank swizzling
    }

    m_L2Offset = m_Offset % (m_FBDataBusWidth * m_NumL2Sectors);
}

void GF100AddressDecode::CalcRBCSlices4L2Sets16DRAMBanks4()
{
    CalcRBCSlices4Common();
    const UINT32 numColBits = ComputeColumnBits();
    const UINT32 rb = (m_L2Tag << 2) | (bits<3, 2>(m_L2Set));
    const UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);

    m_Bank = tmp % m_NumDRAMBanks;
    m_ExtBank = tmp / m_NumDRAMBanks;
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);
}

void GF100AddressDecode::CalcRBCSlices4L2Sets16DRAMBanks8()
{
    CalcRBCSlices4Common();
    const UINT32 numColBits = ComputeColumnBits();
    const UINT32 rb = (m_L2Tag << 2) | (bits<3, 2>(m_L2Set));
    const UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
    // swizzle banks for bank grouping
    m_Bank =  bit<2>(tmp) |
        (bit<1>(tmp) << 1) |
        (bit<0>(tmp) << 2);
    //    m_Bank = tmp % m_NumDRAMBanks;   // Wayne: overriding the bank swizzle temporarily

    m_ExtBank = bit<3>(tmp);
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);
}

void GF100AddressDecode::CalcRBCSlices4L2Sets16DRAMBanks16()
{
    CalcRBCSlices4Common();
    const UINT32 numColBits = ComputeColumnBits();
    const UINT32 rb = (m_L2Tag << 2) | (bits<3, 2>(m_L2Set));
    const UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
    // swizzle banks for bank grouping
    m_Bank =  bit<2>(tmp) |
        (bit<3>(tmp) << 1) |
        (bit<1>(tmp) << 2) |
        (bit<0>(tmp) << 3);
    //    m_Bank = tmp % m_NumDRAMBanks;   // Wayne: overriding the bank swizzle temporarily
    m_ExtBank = bit<4>(tmp);
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);
}

void GF100AddressDecode::CalcRBCSlices4L2Sets32DRAMBanks4()
{
    CalcRBCSlices4Common();
    const UINT32 numColBits = ComputeColumnBits();
    const UINT32 rb = (m_L2Tag << 3) | (bits<4, 2>(m_L2Set));
    const UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);

    m_Bank = tmp % m_NumDRAMBanks;
    m_ExtBank = tmp / m_NumDRAMBanks;
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);
}

void GF100AddressDecode::CalcRBCSlices4L2Sets32DRAMBanks8()
{
    CalcRBCSlices4Common();
    const UINT32 numColBits = ComputeColumnBits();
    const UINT32 rb = (m_L2Tag << 3) | (bits<4, 2>(m_L2Set));
    const UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
    // swizzle banks for bank grouping
    m_Bank =  bit<2>(tmp) |
        (bit<1>(tmp) << 1) |
        (bit<0>(tmp) << 2);
    //    m_Bank = tmp % m_NumDRAMBanks;   // Wayne: overriding the bank swizzle temporarily

    m_ExtBank = bit<3>(tmp);
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);
}

void GF100AddressDecode::CalcRBCSlices4L2Sets32DRAMBanks16()
{
    CalcRBCSlices4Common();
    const UINT32 numColBits = ComputeColumnBits();
    const UINT32 rb = (m_L2Tag << 3) | (bits<4, 2>(m_L2Set));
    const UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
    // swizzle banks for bank grouping
    m_Bank =  bit<2>(tmp) |
        (bit<3>(tmp) << 1) |
        (bit<1>(tmp) << 2) |
        (bit<0>(tmp) << 3);
    //    m_Bank = tmp % m_NumDRAMBanks;   // Wayne: overriding the bank swizzle temporarily
    m_ExtBank = bit<4>(tmp);
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);
}

void GF100AddressDecode::CalcRBCSlices2L2Sets16DRAMBanks4()
{
    CalcRBCSlices2Common();
    const UINT32 numColBits = ComputeColumnBits();
    const UINT32 rb = (m_L2Tag << 1) | bit<3>(m_L2Set);
    const UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
    m_Bank = tmp % m_NumDRAMBanks;
    m_ExtBank = tmp / m_NumDRAMBanks;
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);
}

void GF100AddressDecode::CalcRBCSlices2L2Sets16DRAMBanks8()
{
    CalcRBCSlices2Common();
    const UINT32 numColBits = ComputeColumnBits();
    const UINT32 rb = (m_L2Tag << 1) | bit<3>(m_L2Set);
    const UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
    // swizzle banks for bank grouping, 8 banks, 2 slice
    m_Bank =  bit<2>(tmp) |
        (bit<1>(tmp) << 1) |
        (bit<0>(tmp) << 2);
    //    m_Bank = tmp % m_NumDRAMBanks;   // Wayne: overriding the bank swizzle temporarily
    m_ExtBank = bit<3>(tmp);
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);
}

void GF100AddressDecode::CalcRBCSlices2L2Sets16DRAMBanks16()
{
    CalcRBCSlices2Common();
    const UINT32 numColBits = ComputeColumnBits();
    const UINT32 rb = (m_L2Tag << 1) | bit<3>(m_L2Set);
    const UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
    // swizzle banks for bank grouping, 8 banks, 2 slice
    m_Bank =  bit<2>(tmp) |
        (bit<3>(tmp) << 1) |
        (bit<1>(tmp) << 2) |
        (bit<0>(tmp) << 3);
    //  m_Bank = tmp % m_NumDRAMBanks;   // Wayne: overriding the bank swizzle temporarily
    m_ExtBank = bit<4>(tmp);
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);
}

void GF100AddressDecode::CalcRBCSlices2L2Sets32DRAMBanks4()
{
    CalcRBCSlices2Common();
    const UINT32 numColBits = ComputeColumnBits();
    const UINT32 rb = (m_L2Tag << 2) | bits<4, 3>(m_L2Set);
    const UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
    // swizzle banks for bank grouping, 4 banks, 2 slice
    m_Bank = tmp % m_NumDRAMBanks;
    m_ExtBank = tmp / m_NumDRAMBanks;
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);
}

void GF100AddressDecode::CalcRBCSlices2L2Sets32DRAMBanks8()
{
    CalcRBCSlices2Common();
    const UINT32 numColBits = ComputeColumnBits();
    const UINT32 rb = (m_L2Tag << 2) | bits<4, 3>(m_L2Set);
    const UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
    // swizzle banks for bank grouping, 8 banks, 2 slice
    m_Bank =  bit<2>(tmp) |
        (bit<1>(tmp) << 1) |
        (bit<0>(tmp) << 2);
    //    m_Bank = tmp % m_NumDRAMBanks;   // Wayne: overriding the bank swizzle temporarily
    m_ExtBank = bit<3>(tmp);
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);

    // bank swizzle fix
    //if( m_DRAMBankSwizzle ) {
      //  m_Bank= bits<2, 0>(UINT32(   m_Bank - bits<2, 0>(m_Row) - (bits<1, 0>(m_Row)<<1)  ));
      //m_Bank= bits<2, 0>(UINT32(m_Bank -3*m_Row));
    //}
}

void GF100AddressDecode::CalcRBCSlices2L2Sets32DRAMBanks16()
{
    CalcRBCSlices2Common();
    const UINT32 numColBits = ComputeColumnBits();
    const UINT32 rb = (m_L2Tag << 2) | bits<4, 3>(m_L2Set);
    const UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
    // swizzle banks for bank grouping, 8 banks, 2 slice
    m_Bank =  bit<2>(tmp) |
        (bit<3>(tmp) << 1) |
        (bit<1>(tmp) << 2) |
        (bit<0>(tmp) << 3);
    //    m_Bank = tmp % m_NumDRAMBanks;   // Wayne: overriding the bank swizzle temporarily
    m_ExtBank = bit<4>(tmp);
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);
}

void GF100AddressDecode::CalcRBCSlices2L2Sets16DRAMBanks4GF108()
{
    m_Partition = bit<0>(m_Slice);
    UINT32 rb = 0;

    if (m_DRAMBankSwizzle)
    {
        // m_SubPart is callwlated beforehand by extracting a bit (depending on subpartition stride)
        // while bank swizzling is on. So that callwlation is missing here but callwlated while
        // bank swizzling is off.
        m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
        rb = (m_L2Tag<<1) | bit<3>(m_L2Set);
    }
    else
    {
        switch (m_PartitionInterleave)
        {
        case 256:
            m_SubPart = bit<1>(m_L2Set);
            m_Col = (bits<3, 2>(m_L2Set) << 6) | (bit<0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = m_L2Tag;
            break;
        case 512:
            m_SubPart = bit<2>(m_L2Set);
            m_Col = (bit<3>(m_L2Set) << 7) | (bits<1, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = m_L2Tag;
            break;
        case 1024:
            m_SubPart = bit<3>(m_L2Set);
            m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = m_L2Tag;
            break;
        case 2048:
            m_SubPart = bit<0>(m_L2Tag);
            m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = (m_L2Tag & ~0x1) | bit<3>(m_L2Set);
            break;
        }
    }

    const UINT32 numColBits = ComputeColumnBits();
    const UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
    // swizzle banks for bank grouping, 4 banks, 2 slice
    m_Bank = tmp % m_NumDRAMBanks;
    m_ExtBank = tmp / m_NumDRAMBanks;
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);
}

void GF100AddressDecode::CalcRBCSlices2L2Sets16DRAMBanks8GF108()
{
    m_Partition = bit<0>(m_Slice);
    UINT32 rb = 0;

    if (m_DRAMBankSwizzle)
    {
        // m_SubPart is callwlated beforehand by extracting a bit (depending on subpartition stride)
        // while bank swizzling is on. So that callwlation is missing here but callwlated while
        // bank swizzling is off.
        m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
        rb = (m_L2Tag<<1) | bit<3>(m_L2Set);
    }
    else
    {
        switch (m_PartitionInterleave)
        {
        case 256:
            m_SubPart = bit<1>(m_L2Set);
            m_Col = (bits<3, 2>(m_L2Set) << 6) | (bit<0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = m_L2Tag;
            break;
        case 512:
            m_SubPart = bit<2>(m_L2Set);
            m_Col = (bit<3>(m_L2Set) << 7) | (bits<1, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = m_L2Tag;
            break;
        case 1024:
            m_SubPart = bit<3>(m_L2Set);
            m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = m_L2Tag;
            break;
        case 2048:
            m_SubPart = bit<0>(m_L2Tag);
            m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = (m_L2Tag & ~0x1) | bit<3>(m_L2Set);
            break;
        }
    }

    const UINT32 numColBits = ComputeColumnBits();
    const UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
    // swizzle banks for bank grouping, 8 banks, 2 slice
    m_Bank =  bit<2>(tmp) |
        (bit<1>(tmp) << 1) |
        (bit<0>(tmp) << 2);
    //    m_Bank = tmp % m_NumDRAMBanks;   // Wayne: overriding the bank swizzle temporarily
    m_ExtBank = bit<3>(tmp);
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);
}

void GF100AddressDecode::CalcRBCSlices2L2Sets16DRAMBanks16GF108()
{
    m_Partition = bit<0>(m_Slice);
    UINT32 rb = 0;

    if (m_DRAMBankSwizzle)
    {
        // m_SubPart is callwlated beforehand by extracting a bit (depending on subpartition stride)
        // while bank swizzling is on. So that callwlation is missing here but callwlated while
        // bank swizzling is off.
        m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
        rb = (m_L2Tag<<1) | bit<3>(m_L2Set);
    }
    else
    {
        switch (m_PartitionInterleave)
        {
        case 256:
            m_SubPart = bit<1>(m_L2Set);
            m_Col = (bits<3, 2>(m_L2Set) << 6) | (bit<0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = m_L2Tag;
            break;
        case 512:
            m_SubPart = bit<2>(m_L2Set);
            m_Col = (bit<3>(m_L2Set) << 7) | (bits<1, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = m_L2Tag;
            break;
        case 1024:
            m_SubPart = bit<3>(m_L2Set);
            m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = m_L2Tag;
            break;
        case 2048:
            m_SubPart = bit<0>(m_L2Tag);
            m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = (m_L2Tag & ~0x1) | bit<3>(m_L2Set);
            break;
        }
    }

    const UINT32 numColBits = ComputeColumnBits();
    const UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
    // swizzle banks for bank grouping, 8 banks, 2 slice
    m_Bank =  bit<2>(tmp) |
        (bit<3>(tmp) << 1) |
        (bit<1>(tmp) << 2) |
        (bit<0>(tmp) << 3);
    //    m_Bank = tmp % m_NumDRAMBanks;   // Wayne: overriding the bank swizzle temporarily
    m_ExtBank = bit<4>(tmp);
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);
}

void GF100AddressDecode::CalcRBCSlices2L2Sets32DRAMBanks4GF108()
{
    m_Partition = bit<0>(m_Slice);
    UINT32 rb = 0;

    if (m_DRAMBankSwizzle)
    {
        // m_SubPart is callwlated beforehand by extracting a bit (depending on subpartition stride)
        // while bank swizzling is on. So that callwlation is missing here but callwlated while
        // bank swizzling is off.
        m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
        rb = (m_L2Tag<<2) | bits<4, 3>(m_L2Set);
    }
    else
    {
        switch (m_PartitionInterleave)
        {
        case 256:
            m_SubPart = bit<1>(m_L2Set);
            m_Col = (bits<3, 2>(m_L2Set) << 6) | (bit<0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = (m_L2Tag << 1) | bit<4>(m_L2Set);
            break;
        case 512:
            m_SubPart = bit<2>(m_L2Set);
            m_Col = (bit<3>(m_L2Set) << 7) | (bits<1, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = (m_L2Tag << 1) | bit<4>(m_L2Set);
            break;
        case 1024:
            m_SubPart = bit<3>(m_L2Set);
            m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = (m_L2Tag << 1) | bit<4>(m_L2Set);
            break;
        case 2048:
            m_SubPart = bit<4>(m_L2Set);
            m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = (m_L2Tag << 1) | bit<3>(m_L2Set);
            break;
        }
    }

    const UINT32 numColBits = ComputeColumnBits();
    const UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
    // swizzle banks for bank grouping, 4 banks, 2 slice
    m_Bank = tmp % m_NumDRAMBanks;
    m_ExtBank = tmp / m_NumDRAMBanks;
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);
}

void GF100AddressDecode::CalcRBCSlices2L2Sets32DRAMBanks8GF108()
{
    m_Partition = bit<0>(m_Slice);
    UINT32 rb = 0;

    if (m_DRAMBankSwizzle)
    {
        // m_SubPart is callwlated beforehand by extracting a bit (depending on subpartition stride)
        // while bank swizzling is on. So that callwlation is missing here but callwlated while
        // bank swizzling is off.
        m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
        rb = (m_L2Tag<<2) | bits<4, 3>(m_L2Set);
    }
    else
    {
        switch (m_PartitionInterleave)
        {
        case 512:
            m_SubPart = bit<2>(m_L2Set);
            m_Col = (bits<1, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = (m_L2Tag << 2) | bits<4, 3>(m_L2Set);
            break;
        case 1024:
            m_SubPart = bit<3>(m_L2Set);
            m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
            rb = (m_L2Tag << 1) | bit<4>(m_L2Set);
            break;
        default:
            Printf(Tee::PriError, "DecodeAddress: "
                    "Unsupported memory configuration - "
                    "subpartition size %u on GF108\n",
                    m_PartitionInterleave);
            MASSERT(!"Unsupported memory configuration!");
            break;
        }
    }

    const UINT32 numColBits = ComputeColumnBits();
    UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);

    // swizzle banks for bank grouping, 8 banks, 2 slice
    m_Bank =  bit<2>(tmp) |
        (bit<1>(tmp) << 1) |
        (bit<0>(tmp) << 2);
    //    m_Bank = tmp % m_NumDRAMBanks;   // Wayne: overriding the bank swizzle temporarily
    m_ExtBank = bit<3>(tmp);
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    if (m_PartitionInterleave == 512)
    {
        // m_Col[c-1:7] = m_Row[c-9:0], where c > 7
        m_Col = ((m_Row << 7) | m_Col) & ((1 << numColBits) - 1);
        m_Row = m_Row >> (numColBits - 7);
    }
    else
    {
        // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
        m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
        m_Row = m_Row >> (numColBits - 8);
    }
}

void GF100AddressDecode::CalcRBCSlices2L2Sets32DRAMBanks16GF108()
{
    m_Partition = bit<0>(m_Slice);
    UINT32 rb = 0;

    if (m_DRAMBankSwizzle)
    {
        // m_SubPart is callwlated beforehand by extracting a bit (depending on subpartition stride)
        // while bank swizzling is on. So that callwlation is missing here but callwlated while
        // bank swizzling is off.
        m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
        rb = (m_L2Tag<<2) | bits<4, 3>(m_L2Set);
    }
    else
    {
        switch (m_PartitionInterleave)
        {
            case 256:
                m_SubPart = bit<1>(m_L2Set);
                m_Col = (bits<3, 2>(m_L2Set) << 6) | (bit<0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
                rb = (m_L2Tag << 1) | bit<4>(m_L2Set);
                break;
            case 512:
                m_SubPart = bit<2>(m_L2Set);
                m_Col = (bit<3>(m_L2Set) << 7) | (bits<1, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
                rb = (m_L2Tag << 1) | bit<4>(m_L2Set);
                break;
            case 1024:
                m_SubPart = bit<3>(m_L2Set);
                m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
                rb = (m_L2Tag << 1) | bit<4>(m_L2Set);
                break;
            case 2048:
                m_SubPart = bit<4>(m_L2Set);
                m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
                rb = (m_L2Tag << 1) | bit<3>(m_L2Set);
                break;
        }
    }


    const UINT32 numColBits = ComputeColumnBits();
    UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);

    // swizzle banks for bank grouping, 8 banks, 2 slice
    m_Bank =  bit<2>(tmp) |
        (bit<3>(tmp) << 1) |
        (bit<1>(tmp) << 2) |
        (bit<0>(tmp) << 3);
    //    m_Bank = tmp % m_NumDRAMBanks;   // Wayne: overriding the bank swizzle temporarily
    m_ExtBank = bit<4>(tmp);
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);
}

void GF100AddressDecode::CalcRBCSlices4Common()
{
    m_SubPart = bit<1>(m_Slice);
    // m_col = (bit<0>(m_Slice) << 7) | (bits<1, 0>(m_L2Set) << 5) | (m_L2Sector << 3);
    m_Col = (bit<0>(m_Slice) << 7) | (bits<1, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
}

void GF100AddressDecode::CalcRBCSlices2Common()
{
    m_SubPart = bit<0>(m_Slice);
    // m_col = (bits<2, 0>(m_L2Set) << 5) | (m_L2Sector << 3);
    m_Col = (bits<2, 0>(m_L2Set) << 5) | bits<6, 2>(m_L2Offset);
}

UINT32 GF100AddressDecode::ComputeColumnBits() const
{
    switch (m_DRAMPageSize)
    {
        case 1024:
            return 8;
        case 2048:
            return 9;
        case 4096:
            return 10;
        case 8192:
            return 11;
        case 16384:
            return 12;
        default:
            Printf(Tee::PriError, "DecodeAddress: "
                    "Unsupported DRAM page size - %u!\n",
                    m_DRAMPageSize);
            MASSERT(!"Unsupported DRAM page size!");
            return 0;
    }
}

UINT32 GF100AddressDecode::XbarRawPaddr() const
{
    return (m_L2Tag<<m_NumL2SetsBits)|m_L2Set;
}

GF100AddressEncode::GF100AddressEncode
(

    UINT32 NumPartitions,
    GpuSubdevice* pSubdev
)
: m_Row(0),
  m_Column(0),
  m_IntBank(0),
  m_ExtBank(0),
  m_SubPartition(0),
  m_Partition(0),
  m_PteKind(0),
  m_PageSize(0),
  m_pGpuSubdevice(pSubdev),
  m_NumPartitions(NumPartitions),
  m_EccOn(0),

  // GF100-specific:
  m_NumL2Sets(32),
  m_NumL2SetsBits(5),

  m_L2Offset(0),
  m_Slice(0),
  m_L2Set(0),
  m_L2Tag(0),
  m_Padr(0),
  m_PAKS(0),
  m_PAKS_9_8(0),
  m_PAKS_7(0),
  m_Quotient(0),
  m_Offset(0),
  m_Remainder(0),
  m_Sum(0),
  m_PdsReverseMap(0),
  m_PhysAddr(0),

  m_NumXbarL2Slices(0),
  m_FBAColumnBitsAboveBit8(0),
  m_FBADRAMBankBits(0)
{
    const FrameBuffer *pFb = m_pGpuSubdevice->GetFB();

    m_AmapColumnSize  = pFb->GetAmapColumnSize();
    m_NumDRAMBanks    = pFb->GetBankCount();
    m_NumExtDRAMBanks = Utility::Log2i(pFb->GetRankCount());
    m_DRAMPageSize    = pFb->GetRowSize();
    m_DRAMBankSwizzle = 0 !=
        DRF_VAL(_PFB, _FBPA_BANK_SWIZZLE, _ENABLE,
            pSubdev->RegRd32(LW_PFB_FBPA_BANK_SWIZZLE));

    m_NumXbarL2Slices =
        DRF_VAL(_PLTCG, _LTCS_LTSS_CBC_PARAM, _SLICES_PER_FBP,
                pSubdev->RegRd32(LW_PLTCG_LTCS_LTSS_CBC_PARAM));

    switch (m_DRAMPageSize)
    {
        case 1024:   m_FBAColumnBitsAboveBit8 = 0; break;
        case 2048:   m_FBAColumnBitsAboveBit8 = 1; break;
        case 4096:   m_FBAColumnBitsAboveBit8 = 2; break;
        case 8192:   m_FBAColumnBitsAboveBit8 = 3; break;
        case 16384:  m_FBAColumnBitsAboveBit8 = 4; break;
        case 32768:  m_FBAColumnBitsAboveBit8 = 5; break;
        case 65536:  m_FBAColumnBitsAboveBit8 = 6; break;
        case 131072: m_FBAColumnBitsAboveBit8 = 7; break;
    }

    switch (m_NumDRAMBanks)
    {
        case 4:  m_FBADRAMBankBits = 2; break;
        case 8:  m_FBADRAMBankBits = 3; break;
        case 16: m_FBADRAMBankBits = 4; break;
    }

    constexpr auto Pri = Tee::PriDebug;
    Printf(Pri, "FB config for encode:\n");
    Printf(Pri, "  NumPartitions   %u\n", m_NumPartitions);
    Printf(Pri, "  NumXbarL2Slices %u\n", m_NumXbarL2Slices);
    Printf(Pri, "  NumDRAMBanks    %u\n", m_NumDRAMBanks);
    Printf(Pri, "  NumExtDRAMBanks %u\n", m_NumExtDRAMBanks);
    Printf(Pri, "  DRAMPageSize    %u\n", m_DRAMPageSize);
    Printf(Pri, "  DRAMBankSwizzle %s\n", m_DRAMBankSwizzle?"true":"false");
    Printf(Pri, "  ECCOn           %s\n", m_EccOn?"true":"false");
    Printf(Pri, "  NumL2Sets       %u\n", m_NumL2Sets);
    Printf(Pri, "  NumL2SetsBits   %u\n", m_NumL2SetsBits);
}

UINT64 GF100AddressEncode::EncodeAddress
(
    const FbDecode &decode,
    UINT32 pteKind,
    UINT32 pageSize,
    bool eccOn)
{
    const FrameBuffer *pFb = m_pGpuSubdevice->GetFB();
    const UINT32 rowOffset = pFb->GetRowOffset(decode.burst, decode.beat,
                                               decode.beatOffset);
    m_Partition = pFb->VirtualFbioToVirtualFbpa(decode.virtualFbio);
    m_SubPartition = decode.subpartition;
    m_ExtBank = decode.rank;
    m_IntBank = decode.bank;
    m_Row = decode.row;
    m_Column = rowOffset / m_AmapColumnSize;
    m_EccOn = eccOn;

    m_PteKind = pteKind;
    m_PageSize = pageSize;

    MapRBCToXBar();
    MapXBarToPA();

    // 0 out the lower 12 bits since we just need the page offset.
    m_PhysAddr = maskoff_lower_bits<12>(m_PhysAddr);

    Printf(Tee::PriDebug,
           "GF10x Address Encode 0x%llx, kind 0x%02x, page %uKB: "
           "numpart %u, "
           "fbpa %u, "
           "subpart %u, "
           "extbank %u, "
           "bank %u, "
           "row 0x%x, "
           "column 0x%x\n",
           m_PhysAddr, m_PteKind, m_PageSize, m_NumPartitions,
           m_Partition, m_SubPartition, m_ExtBank, m_IntBank, m_Row,
           m_Column);
    Printf(Tee::PriDebug, "Detailed Encode for 0x%llx: "
           "paks 0x%llx, q 0x%llx, r 0x%x, o 0x%x, s %u, "
           "l2set %u, l2t %u, l2o %u, paks98 0x%llx, paks7 0x%llx\n",
           m_PhysAddr, m_PAKS, m_Quotient, m_Remainder, m_Offset, m_Slice,
           m_L2Set, m_L2Tag, m_L2Offset, m_PAKS_9_8, m_PAKS_7);
    return m_PhysAddr + (rowOffset % m_AmapColumnSize);
}

void GF100AddressEncode::MapRBCToXBar()
{
    UINT64 RBLWpper = 0;
    UINT32 BankShift = m_FBADRAMBankBits;
    UINT32 OldBank = m_IntBank;

    switch (m_NumDRAMBanks)
    {
        case 4:
            OldBank = m_IntBank;
            if (m_NumExtDRAMBanks > 0)
            {
                OldBank = m_ExtBank << 2 | m_IntBank;
                BankShift ++;
            }
            break;
        case 8:
            OldBank = (bit<0>(m_IntBank) << 2) | (bit<1>(m_IntBank) << 1) |
                      (bit<2>(m_IntBank));
            if (m_NumExtDRAMBanks > 0)
            {
                OldBank = m_ExtBank << 3 | OldBank;
                BankShift ++;
            }
            break;
        case 16:
            OldBank = (bit<1>(m_IntBank) << 3) | (bit<0>(m_IntBank) << 2) |
                      (bit<2>(m_IntBank) << 1) | bit<3>(m_IntBank);
            if (m_NumExtDRAMBanks > 0)
            {
                OldBank = m_ExtBank << 4 | OldBank;
                BankShift ++;
            }
            break;
    }

    //common RBC_upper except 512B stride in 32 L2sets
    RBLWpper = (m_Row << m_FBAColumnBitsAboveBit8) | (m_Column >> 8);
    RBLWpper = (RBLWpper << BankShift) | OldBank;

    // calc m_L2Set and m_L2Tag
    m_L2Offset = bits<4, 0>(m_Column) << 2;

    switch (m_NumXbarL2Slices)
    {
        case 2:
            m_Slice = m_SubPartition;
            m_L2Set = bits<7, 5>(m_Column);

            switch (m_NumL2Sets)
            {
                case 32:
                    m_L2Set |= (bits<1, 0>(RBLWpper)) << 3;
                    m_L2Tag = (UINT32)(bits<28, 2>(RBLWpper));
                    break;
                case 16:
                    m_L2Set |= (bit<0>(RBLWpper)) << 3;
                    m_L2Tag = (UINT32)(bits<28, 1>(RBLWpper));
                    break;
                default:
                    MASSERT(!"Unrecognized L2Set\n");
            }
            break;
        case 4:
            m_Slice = m_SubPartition << 1 | bit<7>(m_Column);
            m_L2Set = bits<6, 5>(m_Column);

            switch (m_NumL2Sets)
            {
                case 32:
                    m_L2Set |= (bits<2, 0>(RBLWpper)) << 2;
                    m_L2Tag =  (UINT32)(bits<28, 3>(RBLWpper));
                    break;
                case 16:
                    m_L2Set |= (bits<1, 0>(RBLWpper)) << 2;
                    m_L2Tag =  (UINT32)(bits<28, 2>(RBLWpper));
                    break;
                default:
                    MASSERT(!"Unrecognized L2Set\n");
            }
            break;
        default:
            break;
    }

    m_Padr = (m_L2Tag << m_NumL2SetsBits) | m_L2Set;

    if (m_EccOn)
    {
        m_Padr = (m_Padr >> 3) * 7 + m_Padr % 8;
    }
    if (m_DRAMBankSwizzle)
    {
        m_Padr = (m_Padr & ~0x38) |
                 ((bits<2, 0>( bits<5, 3>(m_Padr) +
                              3 * (bits<8, 6>(m_Padr)) )) << 3);
    }
}

void GF100AddressEncode::MapXBarToPA()
{
    m_L2Tag = (UINT32)(m_Padr >> m_NumL2SetsBits);
    m_L2Set = (UINT32)(m_Padr & ((1 << m_NumL2SetsBits) - 1));

    UINT32 TempSum;

    switch (m_NumL2Sets)
    {
        case 32:
            switch (m_NumPartitions)
            {
            case 1: case 2: case 4: case 6: case 7: case 8:
                m_Quotient = bits<27, 1>(m_L2Tag) << 4;
                m_Sum = static_cast<UINT32>(bits<2, 0>(bits<8, 6>(m_Quotient) + bits<13, 11>(m_Quotient) + bits<17, 16>(m_Quotient) +
                                  ((bit<4>(m_Quotient) ^ bit<5>(m_Quotient) ^ bit<9>(m_Quotient) ^ bit<10>(m_Quotient) ^ bit<14>(m_Quotient) ^ bit<15>(m_Quotient))<<2)));
                m_Quotient |= ( bits<2, 0>( ((bit<0>(m_L2Tag)<<2) | bits<4, 3>(m_L2Set)) - m_Sum ) << 1 );
                m_PAKS_9_8 = bits<1, 0>(bits<2, 1>(m_L2Set) ^ bits<5, 4>(m_Quotient) ^ bits<10, 9>(m_Quotient) ^ bits<15, 14>(m_Quotient));
                m_PAKS_7 = bit<0>(bit<0>(m_L2Set) ^ bit<3>(m_Quotient) ^ bit<8>(m_Quotient) ^ bit<13>(m_Quotient));
                m_PdsReverseMap = static_cast<UINT32>((m_Quotient << 2) | ( m_PAKS_9_8 << 1) | m_PAKS_7);
                switch (m_NumPartitions)
                {
                    case 1: case 2: case 4:
                        m_Quotient |= m_Slice;
                        break;
                    case 6:
                        m_Quotient |= bit<0>(m_Slice ^ bit<0>(m_Partition) ^  bit<2>(m_Quotient) ^  bit<3>(m_Quotient) ^  bit<4>(m_Quotient) ^
                                             bit<5> (m_Quotient) ^  bit<6> (m_Quotient) ^  bit<7> (m_Quotient) ^  bit<8> (m_Quotient) ^  bit<9> (m_Quotient) ^
                                             bit<10>(m_Quotient) ^  bit<11>(m_Quotient) ^  bit<12>(m_Quotient) ^  bit<13>(m_Quotient) ^  bit<14>(m_Quotient) ^
                                             bit<15>(m_Quotient) ^  bit<16>(m_Quotient) ^  bit<17>(m_Quotient) ^  bit<18>(m_Quotient) ^  bit<19>(m_Quotient) ^
                                             bit<20>(m_Quotient) ^  bit<21>(m_Quotient) ^  bit<22>(m_Quotient) ^  bit<23>(m_Quotient) ^  bit<24>(m_Quotient) ^
                                             bit<25>(m_Quotient) ^  bit<26>(m_Quotient) ^  bit<27>(m_Quotient) ^  bit<28>(m_Quotient) ^  bit<29>(m_Quotient) ^
                                             bit<30>(m_Quotient) ^  bit<31>(m_Quotient) );
                        break;
                    default:
                        break;
                }
                break;
            case 3: case 5:
                m_Quotient = bits<27, 1>(m_L2Tag) << 3;
                m_Sum = static_cast<UINT32>(bits<2, 0>(bits<7, 5>(m_Quotient) + bits<12, 10>(m_Quotient) + bits<16, 15>(m_Quotient) +
                                  ((bit<3>(m_Quotient) ^ bit<4>(m_Quotient) ^ bit<8>(m_Quotient) ^ bit<9>(m_Quotient) ^ bit<13>(m_Quotient) ^ bit<14>(m_Quotient))<<2)));
                m_Quotient |= ( bits<2, 0>( ((bit<0>(m_L2Tag)<<2) | bits<4, 3>(m_L2Set)) - m_Sum ));
                m_PAKS_9_8 = bits<1, 0>(bits<2, 1>(m_L2Set) ^ bits<4, 3>(m_Quotient) ^ bits<9, 8>(m_Quotient) ^ bits<14, 13>(m_Quotient));
                m_PAKS_7 = bit<0>(bit<0>(m_L2Set) ^ bit<2>(m_Quotient) ^ bit<7>(m_Quotient) ^ bit<12>(m_Quotient));
                m_PdsReverseMap = static_cast<UINT32>((m_Quotient << 3) | ( m_PAKS_9_8 << 1) | m_PAKS_7);
                break;
            default:
                MASSERT(!"Unexpected number of partitions");
                break;
            }
            break;
        case 16:
            switch ( m_NumXbarL2Slices )
            {
            case 4:
                m_Quotient = bits<27, 1>(m_L2Tag) << 4;

                TempSum = static_cast<UINT32>(bits<2, 0>(bits<7, 5>(m_Quotient) + bits<11, 9>(m_Quotient) + bits<15, 13>(m_Quotient) + bit<17>(m_Quotient)));

                m_Quotient |= ( bits<2, 0>(((bit<0>(m_L2Tag) << 2) | bits<3, 2>(m_L2Set)) - TempSum) << 1 );
                m_Quotient |= bit<1>(m_Slice);

                m_PAKS_9_8 = (bit<0>(m_Slice)<<1) | bit<0>(bit<1>(m_L2Set)^bit<4>(m_Quotient)^bit<8>(m_Quotient)^bit<12>(m_Quotient)^bit<16>(m_Quotient));
                m_PAKS_7 = bit<0>(bit<0>(m_L2Set)^bit<3>(m_Quotient)^bit<7>(m_Quotient)^bit<11>(m_Quotient)^bit<15>(m_Quotient));
                break;
            case 2:
                UINT32 R_0;
                switch (m_NumPartitions)
                {
                case 1: case 2: case 4: case 6: case 7: case 8:
                    m_Quotient = bits<27, 2>(m_L2Tag) << 4;

                    TempSum = static_cast<UINT32>(bits<2, 0>(bits<7, 5>(m_Quotient) + bits<11, 9>(m_Quotient) + bits<15, 13>(m_Quotient) + bit<17>(m_Quotient)
                                        + (((bit<8>(m_Quotient)<<1) | bit<4>(m_Quotient)) << 1) + (((bit<16>(m_Quotient)<<1) | bit<12>(m_Quotient)) << 1)));

                    m_Quotient |= ( bits<2, 0>(((bits<1, 0>(m_L2Tag) << 1) | bit<3>(m_L2Set)) - TempSum) << 1 );

                    switch (m_NumPartitions)
                    {
                    case 1: case 2: case 4: case 8:
                        m_Quotient |= m_Slice;
                        break;
                    case 6:
                        R_0 =  static_cast<UINT32>(bit<0>(m_Partition) ^ bit<2>(m_Quotient));

                        m_Quotient |= bit<0> ( bit<0>(m_Slice) ^ bit<0>(R_0) ^  bit<3>(m_Quotient) ^  bit<4>(m_Quotient) ^
                                               bit<5> (m_Quotient) ^  bit<6> (m_Quotient) ^  bit<7> (m_Quotient) ^  bit<8> (m_Quotient) ^  bit<9> (m_Quotient) ^
                                               bit<10>(m_Quotient) ^  bit<11>(m_Quotient) ^  bit<12>(m_Quotient) ^  bit<13>(m_Quotient) ^  bit<14>(m_Quotient) ^
                                               bit<15>(m_Quotient) ^  bit<16>(m_Quotient) ^  bit<17>(m_Quotient) ^  bit<18>(m_Quotient) ^  bit<19>(m_Quotient) ^
                                               bit<20>(m_Quotient) ^  bit<21>(m_Quotient) ^  bit<22>(m_Quotient) ^  bit<23>(m_Quotient) ^  bit<24>(m_Quotient) ^
                                               bit<25>(m_Quotient) ^  bit<26>(m_Quotient) ^  bit<27>(m_Quotient) ^  bit<28>(m_Quotient) ^  bit<29>(m_Quotient) ^
                                               bit<30>(m_Quotient) ^  bit<31>(m_Quotient) );
                        break;
                    case 7:
                        m_Quotient |= bit<0>(bit<0>(m_Slice) ^ bit<0>(m_Partition) ^  bit<2>(m_Quotient));
                        break;
                    default:
                        break;
                    }

                    m_PAKS_9_8 = bits<1, 0>(((bit<0>(bit<2>(m_L2Set)^bit<3>(m_Quotient)^bit<7>(m_Quotient)^bit<11>(m_Quotient)^bit<15>(m_Quotient)))
                                            << 1) | (bit<0>(bit<1>(m_L2Set)^bit<4>(m_Quotient)^bit<8>(m_Quotient)^bit<12>(m_Quotient)^bit<16>(m_Quotient))));
                    m_PAKS_7 = bit<0>(bit<0>(m_L2Set)^bit<2>(m_Quotient)^bit<6>(m_Quotient)^bit<10>(m_Quotient)^bit<14>(m_Quotient));
                    m_PdsReverseMap = static_cast<UINT32>((m_Quotient << 2) | ( m_PAKS_9_8 << 1) | m_PAKS_7);
                    break;
                case 3: case 5:
                    m_Quotient = bits<27, 2>(m_L2Tag) << 3;

                    TempSum = static_cast<UINT32>(bits<2, 0>(bits<6, 4>(m_Quotient) + bits<10, 8>(m_Quotient) + bits<14, 12>(m_Quotient) + bit<16>(m_Quotient) +
                                        (((bit<7>(m_Quotient)<< 1) | bit<3>(m_Quotient)) << 1) + (((bit<15>(m_Quotient)<< 1)| bit<11>(m_Quotient))<< 1)));

                    m_Quotient |= bits<2, 0>(((bits<1, 0>(m_L2Tag) << 1) | bit<3>(m_L2Set)) - TempSum);

                    m_PAKS_9_8 = bits<1, 0>(((bit<0>(bit<2>(m_L2Set)^bit<2>(m_Quotient)^bit<6>(m_Quotient)^bit<10>(m_Quotient)^bit<14>(m_Quotient))) << 1) |
                                           (bit<0>(bit<1>(m_L2Set)^bit<3>(m_Quotient)^bit<7>(m_Quotient)^bit<11>(m_Quotient)^bit<15>(m_Quotient))));
                    m_PAKS_7 = bit<0>(bit<0>(m_L2Set)^bit<1>(m_Quotient)^bit<5>(m_Quotient)^bit<9>(m_Quotient)^bit<13>(m_Quotient));
                    m_PdsReverseMap = static_cast<UINT32>((m_Quotient << 3) | ( m_PAKS_9_8 << 1) | m_PAKS_7);
                    break;
                default:
                    MASSERT(!"Unexpected number of partitions");
                    break;
                }
            }
            break;
    }

    // calc R
    switch ( m_NumPartitions )
    {
        case 1: case 2: case 3: case 4: case 5: case 8:
            m_Remainder = m_Partition;
            break;
        case 6:
            m_Remainder = static_cast<UINT32>((m_Partition & 6) | (bit<0>(m_Partition) ^ bit<2>(m_Quotient)));
            break;
        case 7:
            m_Remainder = (7 + m_Partition - bits<2, 0>(m_Quotient))%7;
            break;
        default:
            break;
    }

    // calc O
    switch ( m_NumPartitions )
    {
        case 1: case 2: case 4: case 6: case 7: case 8:
            m_Offset = static_cast<UINT32>((m_PAKS_9_8 << 8) | (m_PAKS_7 << 7));
            break;
        case 3: case 5:
            switch ( m_NumXbarL2Slices )
            {
            case 4:
                m_Offset = static_cast<UINT32>(( (bit<1>(m_Slice)) << 10) | (m_PAKS_9_8 << 8) | (m_PAKS_7 << 7)); break;
            case 2:
                m_Offset = static_cast<UINT32>(( (bit<0>(m_Slice)) << 10) | (m_PAKS_9_8 << 8) | (m_PAKS_7 << 7)); break;
            }
            break;
        default:
            break;
    }

    // calc PAKS

    switch ( m_NumPartitions )
    {
        case 1: case 2: case 4: case 6: case 7: case 8:
            m_PAKS = (UINT64)(((UINT64)m_Quotient*m_NumPartitions+m_Remainder) << 10) + m_Offset;
            break;
        case 3: case 5:
            m_PAKS = (UINT64)(((UINT64)m_Quotient*m_NumPartitions+m_Remainder) << 11) + m_Offset;
            break;
        default:
            break;
    }

    // reverse paks to pa
    switch ( m_PteKind )
    {
      case LW_MMU_PTE_KIND_PITCH:
          switch ( m_PageSize )
          {
            case 4:
                switch ( m_NumPartitions )
                {
                    case 1:
                        CalcPhysAddrPitchLinearPS4KParts1();
                        break;
                    case 2:
                        CalcPhysAddrPitchLinearPS4KParts2();
                        break;
                    case 3:
                        CalcPhysAddrPitchLinearPS4KParts3();
                        break;
                    case 4:
                        CalcPhysAddrPitchLinearPS4KParts4();
                        break;
                    case 5:
                        CalcPhysAddrPitchLinearPS4KParts5();
                        break;
                    case 6:
                        CalcPhysAddrPitchLinearPS4KParts6();
                        break;
                    default:
                        break;
                }

                break;
            case 64:
            case 128:
                MASSERT(!"Address encode functionality not implemented for page size 64K and 128K");
                break;
            default:
                break;
        }
        break;
        case LW_MMU_PTE_KIND_PITCH_NO_SWIZZLE:
        switch ( m_PageSize )
        {
            case 4:
                switch ( m_NumPartitions )
                {
                    case 1: case 2: case 6:
                        m_PhysAddr = m_PAKS;
                        break;
                    case 4:
                        m_PhysAddr = ((bits<PA_MSB, 13>(m_PAKS) << 13)
                                      | (( bit<10>(m_PAKS) ^ bit<12>(m_PAKS) ^ bit<15>(m_PAKS) ) << 12)
                                      | bits<11, 0>(m_PAKS));
                        break;
                    case 3:
                        m_PhysAddr = ((bits<PA_MSB, 13>(m_PAKS) << 13)
                                      | (( bit<10>(m_PAKS) ^ bit<11>(m_PAKS) ^ bit<12>(m_PAKS) ^ bit<13>(m_PAKS) ) << 12)
                                      | bits<11, 0>(m_PAKS));
                        break;
                    case 5:
                        m_PhysAddr = ((bits<PA_MSB, 13>(m_PAKS) << 13)
                                | (( bit<10>(m_PAKS) ^ bit<12>(m_PAKS)) << 12)
                                | bits<11, 0>(m_PAKS));
                        break;
                    default:
                        m_PhysAddr = m_PAKS;
                        break;
                }
                break;
            case 64:
            case 128:
                m_PhysAddr = m_PAKS;
                break;
            default:
                break;
        }
        break;
        case LW_MMU_PTE_KIND_GENERIC_16BX2:
        default:
        // TODO: Lwrrently, the need is only for Pitch PTE
        Printf(Tee::PriWarn, "*** NOT YET IMPLEMENTED ***");
    }
}

void GF100AddressEncode::CalcPhysAddrPitchLinearPS4KParts1()
{
    UINT64 swizzle = (UINT32)
    (( (bit<7>(m_PAKS)<<1 | bit<11>(m_PAKS)) + (bit<12>(m_PAKS)<<1 | bit<8>(m_PAKS))
       + bits<14, 13>(m_PAKS) + bits<16, 15>(m_PAKS) + bits<18, 17>(m_PAKS) + bits<20, 19>(m_PAKS)
       + bits<22, 21>(m_PAKS) + bits<24, 23>(m_PAKS) + bits<26, 25>(m_PAKS) + bits<28, 27>(m_PAKS)
       + bits<30, 29>(m_PAKS) + bit<31>(m_PAKS)));
    m_PhysAddr = ((bits<PA_MSB, 12>(m_PAKS) << 12)
                  | (bit<8>(m_PAKS) << 11)
                  | (bit<7>(m_PAKS) << 10)
                  | (bit<11>(m_PAKS) << 9)
                  | ((bit<10>(m_PAKS) ^ bit<1>(swizzle)) << 8)
                  | ((bit<9>(m_PAKS) ^ bit<0>(swizzle)) << 7)
                  | bits<6, 0>(m_PAKS));
}

void GF100AddressEncode::CalcPhysAddrPitchLinearPS4KParts2()
{
    UINT64 swizzle = (UINT32)(( bits<8, 7>(m_PAKS) + bits<13, 12>(m_PAKS) + bits<15, 14>(m_PAKS) + bits<17, 16>(m_PAKS)
                            + bits<19, 18>(m_PAKS) + bits<21, 20>(m_PAKS) + bits<23, 22>(m_PAKS) + bits<25, 24>(m_PAKS)
                            + bits<27, 26>(m_PAKS) + bits<29, 28>(m_PAKS) + bits<31, 30>(m_PAKS)));

    UINT32 bit9 = (UINT32)(( bit<7>(m_PAKS) ^ bit<8>(m_PAKS) ^ bit<10>(m_PAKS) ^ bit<12>(m_PAKS) ^ bit<13>(m_PAKS) ^
                                bit<14>(m_PAKS) ^ bit<15>(m_PAKS) ^ bit<16>(m_PAKS) ^ bit<17>(m_PAKS) ^
                                bit<18>(m_PAKS) ^ bit<19>(m_PAKS) ^ bit<20>(m_PAKS) ^ bit<21>(m_PAKS) ^
                                bit<22>(m_PAKS) ^ bit<23>(m_PAKS) ^ bit<24>(m_PAKS) ^ bit<25>(m_PAKS) ^
                                bit<26>(m_PAKS) ^ bit<27>(m_PAKS) ^ bit<28>(m_PAKS) ^ bit<29>(m_PAKS) ^
                                bit<30>(m_PAKS) ^ bit<31>(m_PAKS)));

    m_PhysAddr = ((bits<PA_MSB, 12>(m_PAKS) << 12)
                  | (bit<8>(m_PAKS) << 11)
                  | (bit<7>(m_PAKS) << 10)
                  | (bit9 << 9)
                  | ((bit<11>(m_PAKS) ^ bit<1>(swizzle)) << 8)
                  | ((bit<9>(m_PAKS) ^ bit<0>(swizzle)) << 7)
                  | bits<6, 0>(m_PAKS));
}

void GF100AddressEncode::CalcPhysAddrPitchLinearPS4KParts3()
{
    UINT64 swizzle = (UINT32)(bits<1, 0>( (((bit<10>(m_PAKS) ^ bit<11>(m_PAKS) ^ bit<12>(m_PAKS) ^ bit<13>(m_PAKS))<<1) | bit<8>(m_PAKS)) ^
                                         bits<14, 13>(m_PAKS) ^ bits<16, 15>(m_PAKS) ^ bits<18, 17>(m_PAKS) ^ bits<20, 19>(m_PAKS) ^
                                         bits<22, 21>(m_PAKS) ^ bits<24, 23>(m_PAKS) ^ bits<26, 25>(m_PAKS) ^ bits<28, 27>(m_PAKS) ^
                                         bits<30, 29>(m_PAKS)));

    m_PhysAddr = ((bits<PA_MSB, 13>(m_PAKS) << 13)
                  | ((bit<10>(m_PAKS) ^ bit<11>(m_PAKS) ^ bit<12>(m_PAKS) ^ bit<13>(m_PAKS)) << 12)
                  | (bits<8, 7>(m_PAKS) << 10)
                  | (bit<11>(m_PAKS) << 9)
                  | ((bit<10>(m_PAKS) ^ bit<1>(swizzle) ^ bit<0>(swizzle)) << 8)
                  | (bit<9>(m_PAKS) << 7)
                  | bits<6, 0>(m_PAKS));
}

void GF100AddressEncode::CalcPhysAddrPitchLinearPS4KParts4()
{
    UINT32 bits9_8 = (UINT32)(bits<1, 0>( bits<11, 10>(m_PAKS) - (bits<8, 7>(m_PAKS) +
                                                                (bit<13>(m_PAKS)<<1) + bits<15, 14>(m_PAKS) + bits<17, 16>(m_PAKS) + bits<19, 18>(m_PAKS) +
                                                                bits<21, 20>(m_PAKS) + bits<23, 22>(m_PAKS) + bits<25, 24>(m_PAKS) + bits<27, 26>(m_PAKS) + bits<29, 28>(m_PAKS) +
                                                                bits<31, 30>(m_PAKS))));
    m_PhysAddr = ((bits<PA_MSB, 13>(m_PAKS) << 13)
                  | ((bit<12>(m_PAKS) ^ bit<10>(m_PAKS) ^ bit<15>(m_PAKS)) << 12)
                  | (bits<8, 7>(m_PAKS) << 10)
                  | (bits9_8 << 8)
                  | ((bit<9>(m_PAKS) ^ bit<13>(m_PAKS)) << 7)
                  | bits<6, 0>(m_PAKS));
}

void GF100AddressEncode::CalcPhysAddrPitchLinearPS4KParts5()
{
    UINT64 swizzle = (UINT32)(bits<1, 0>( ((bit<13>(m_PAKS)<<1) | (bit<12>(m_PAKS) ^ bit<10>(m_PAKS)))
                                         + bits<15, 14>(m_PAKS) + bits<17, 16>(m_PAKS) + bits<19, 18>(m_PAKS)
                                         + bits<21, 20>(m_PAKS) + bits<23, 22>(m_PAKS) + bits<25, 24>(m_PAKS) + bits<27, 26>(m_PAKS) + bits<29, 28>(m_PAKS)
                                         + bits<31, 30>(m_PAKS)));
    m_PhysAddr = ((bits<PA_MSB, 13>(m_PAKS) << 13)
                  | ((bit<12>(m_PAKS) ^ bit<10>(m_PAKS)) << 12)
                  | (bits<8, 7>(m_PAKS) << 10)
                  | (bit<11>(m_PAKS) << 9)
                  | ((bit<10>(m_PAKS) ^ bit<1>(swizzle)) << 8)
                  | ((bit<9>(m_PAKS) ^ bit<0>(swizzle)) << 7)
                  | bits<6, 0>(m_PAKS));
}

void GF100AddressEncode::CalcPhysAddrPitchLinearPS4KParts6()
{
    UINT32 bit8 = (UINT32)
    (bit<0>( bit<7> (m_PAKS) ^ bit<10>(m_PAKS) ^ bit<11>(m_PAKS) ^ bit<13>(m_PAKS) ^
             bit<15>(m_PAKS) ^ bit<17>(m_PAKS) ^ bit<19>(m_PAKS) ^ bit<21>(m_PAKS) ^
             bit<23>(m_PAKS) ^ bit<25>(m_PAKS) ^ bit<27>(m_PAKS) ^ bit<29>(m_PAKS) ^
             bit<31>(m_PAKS)));

    UINT32 bit7 = (UINT32)
    (bit<0>( bit<9> (m_PAKS) ^ bit<12>(m_PAKS) ^ bit<14>(m_PAKS) ^ bit<16>(m_PAKS) ^
             bit<18>(m_PAKS) ^ bit<20>(m_PAKS) ^ bit<22>(m_PAKS) ^ bit<24>(m_PAKS) ^
             bit<26>(m_PAKS) ^ bit<28>(m_PAKS) ^ bit<30>(m_PAKS)));

    m_PhysAddr = ((bits<PA_MSB, 12>(m_PAKS) << 12)
                  | (bits<8, 7>(m_PAKS) << 10)
                  | (bit<11>(m_PAKS) << 9)
                  | (bit8 << 8)
                  | (bit7 << 7)
                  | bits<6, 0>(m_PAKS));
}
