/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2017,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// The address mapping is dolwments are usually here (but not available yet):
// //hw/doc/gpu/maxwell/maxwell/design/Functional_Descriptions/...

#include "gmlit1adr.h"
#include "bitop.h"
#include "maxwell/gm108/dev_mmu.h"
#include "maxwell/gm108/dev_fbpa.h"
#include "maxwell/gm108/dev_ltc.h"
#include <math.h>

using namespace BitOp;

//------------------------------------------------------------------------------
GMLIT1AddressDecode::GMLIT1AddressDecode(GpuSubdevice* pSubdev) :
    GMlitxFbAddrMap(pSubdev),
    m_PrintDecodeDetails(false)
{
    m_PartitionInterleave = 0;
}

//------------------------------------------------------------------------------
/*virtual*/ GMLIT1AddressDecode::~GMLIT1AddressDecode()
{
}

//------------------------------------------------------------------------------
RC GMLIT1AddressDecode::DecodeAddress
(
    FbDecode* pDecode,
    UINT64 physicalFbOffset,
    UINT32 pteKind,
    UINT32 pageSize
)
{
    const FrameBuffer *pFb = m_pGpuSubdevice->GetFB();
    m_PartitionInterleave = ComputePartitionInterleave(m_NumPartitions);

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
            Printf(Tee::PriError,
                   "DecodeAddress: Unsupported page size - %uKB!\n",
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

//------------------------------------------------------------------------------
/*virtual*/ UINT32 GMLIT1AddressDecode::XbarRawPaddr() const
{
    return (static_cast <UINT32> (m_L2Tag) <<m_NumL2SetsBits)|m_L2Set;
}
//------------------------------------------------------------------------------
/*virtual*/ void GMLIT1AddressDecode::ComputePAKS()
{
    MASSERT(m_PageSize == 4 ||
            m_PageSize == 64 ||
            m_PageSize == 128);
    MASSERT(m_NumPartitions == 1 ||
            m_NumPartitions == 2);
    switch (m_PteKind)
    {
        case LW_MMU_PTE_KIND_PITCH_NO_SWIZZLE:
        case LW_MMU_PTE_KIND_PITCH:
            switch (m_NumPartitions)
            {
                case 1:
                    CalcPAKSPitchLinearPS64KParts1();
                    break;
                case 2:
                    CalcPAKSPitchLinearPS64KParts2();
                    break;
            }
            break;
        // All block linear kinds
        case LW_MMU_PTE_KIND_GENERIC_16BX2:
            switch (m_PageSize)
            {
                case 4:
                    switch (m_NumPartitions)
                    {
                        case 1:
                            CommonCalcPAKSBlockLinearPS4KParts1();
                            break;
                        case 2:
                            CommonCalcPAKSBlockLinearPS4KParts2();
                            break;
                        default:
                            Printf(Tee::PriError, "Page size 4K "
                                " Unsupported number of partition - %u!\n",
                                m_NumPartitions);
                            MASSERT(!"Unsupported NumParts!");
                    }
                    break;
                case 64:
                case 128:
                    switch (m_NumPartitions)
                    {
                        case 1:
                            CommonCalcPAKSBlockLinearPS64KParts1();
                            break;
                        case 2:
                            CommonCalcPAKSBlockLinearPS64KParts2();
                            break;
                        default:
                            Printf(Tee::PriError, "Page size 64-128K "
                                " Unsupported number of partition - %u!\n",
                                m_NumPartitions);
                            MASSERT(!"Unsupported NumParts!");
                    }
                    break;
                default:
                    Printf(Tee::PriError, "DecodeAddress: "
                            "Unsupported page size - 0x%x!\n",
                            m_PageSize);
                    MASSERT(!"Unsupported page size!");
                    break;
            }
            break;
        default:
            MASSERT(!"Unspported PteKind");
            return;
    }
    return;
}

//------------------------------------------------------------------------------
/*virtual*/ void GMLIT1AddressDecode::CommonCalcPAKSBlockLinearPS4KParts1()
{
    CommonCalcPAKSBlockLinearPS64KParts1();
}

//------------------------------------------------------------------------------
/*virtual*/ void GMLIT1AddressDecode::CommonCalcPAKSBlockLinearPS4KParts2()
{
    CommonCalcPAKSBlockLinearPS64KParts2();
}

//------------------------------------------------------------------------------
/*virtual*/ void GMLIT1AddressDecode::CommonCalcPAKSBlockLinearPS64KParts1()
{
    // Galois Swizzle
    const INT32 NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS_2;

    //7 bit overlap in sliding window on address range, no conselwtive two randoms repeat
    UINT32 galois[] =
    {3, 2, 1, 3, 1, 3, 2, 1, 3, 1, 2, 1, 2, 1, 3, 1, 2, 3, 2, 3, 2,
        1, 2, 1, 3, 1, 3, 1, 3, 1};

    // start at bit 18 so slices don't interrupt the bank pattern across 4x64KB horizontal
    UINT64 upper_address = m_PhysAddr >> 18;
    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    m_PAKS = (bits<PADDR_MSB, 11>(m_PhysAddr) << 11) |
        // don't want diagonals
        // using ^ here so that carry doesn't propagate into bit10 from changing bit9,
        // allows bit flipping in ROP
        ((bit<10>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<9>(m_PhysAddr) ^ bit<14>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |
            bits<8, 0>(m_PhysAddr);
}

//------------------------------------------------------------------------------
/*virtual*/ void GMLIT1AddressDecode::CommonCalcPAKSBlockLinearPS64KParts2()
{
    // Galois Swizzle
    const INT32 NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS_2;

    //7 bit overlap in sliding window on address range, no conselwtive two randoms repeat
    UINT32 galois[] =
    {7, 6, 1, 3, 5, 7, 2, 1, 3, 5, 6, 5, 6, 1, 7, 6, 2, 3, 6, 3, 2, 5,
        6, 1, 7, 1, 3, 5, 7, 1};

    // start at bit 18 so slices don't interrupt the bank pattern across 4x64KB horizontal
    UINT64 upper_address = m_PhysAddr >> 18;
    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    m_PAKS = (bits<PADDR_MSB, 12>(m_PhysAddr) << 12) |
        // part
        ((bit<11>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ bit<2>(swizzle)) << 11) |
        // slice1
        ((bit<10>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ bit<11>(m_PhysAddr) ^
          bit<15>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        // slice0
        ((bit<9>(m_PhysAddr)  ^ bit<14>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |
        bits<8, 0>(m_PhysAddr);
}

//------------------------------------------------------------------------------
/*virtual*/ void GMLIT1AddressDecode::CalcPAKSPitchLinearPS64KParts1()
{
    //Galois swizzle
    const UINT32 NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS_2;
    UINT32 Galois[] =
    {2, 3, 2, 1, 3, 1, 2, 3, 1, 3, 1, 3, 2, 3, 2, 1, 3, 1, 2, 3, 1, 2,
        1, 2, 1, 3, 2, 1, 2, 3};

    UINT64 upper_address = m_PhysAddr >> 11;
    // start at bit 11 for 1 partition pitch, since slice[1] is PA[10]
    // generate swizzle
    UINT32 swizzle= 0;
    for (UINT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*Galois[i]);
    }
    m_PAKS = (bits<PADDR_MSB, 11>(m_PhysAddr) << 11) |
        // Don't swizzle above PA[12] since 4KB swizzle uses same mapping
        // using ^ here so that carry doesn't propagate into bit10 from changing bit9,
        // allows bit flipping in ROP
        ((bit<10>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<8>(m_PhysAddr)  ^ bit<0>(swizzle)) << 9) |
        (bit<9>(m_PhysAddr) << 8) |
        bits<7, 0>(m_PhysAddr);
}

//------------------------------------------------------------------------------
/*virtual*/ void GMLIT1AddressDecode::CalcPAKSPitchLinearPS64KParts2()
{
    //Galois swizzle
    const UINT32 NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS_2;
    UINT32 Galois[] =
    {6, 7, 2, 1, 2, 3, 6, 5, 7, 2, 1, 3, 5, 2, 7, 5, 7, 1, 2, 5, 3, 1,
        7, 6, 5, 3, 5, 2, 1, 3};

    // start at bit 11 for 1 partition pitch, since slice[1] is PA[10]
    UINT64 upper_address = m_PhysAddr >> 12;
    // generate swizzle
    UINT32 swizzle= 0;
    for (UINT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*Galois[i]);
    }
    // Don't swizzle above PA[12] since 4KB swizzle uses same mapping
    // using ^ here so that carry doesn't propagate into bit10 from changing bit9,
    //allows bit flipping in ROP
    m_PAKS = (bits<PADDR_MSB, 12>(m_PhysAddr) << 12) |
        ((bit<11>(m_PhysAddr) ^ bit<2>(swizzle)) << 11) |
        ((bit<10>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<8>(m_PhysAddr)  ^ bit<0>(swizzle)) << 9) |
        (bit<9>(m_PhysAddr) << 8) |
        bits<7, 0>(m_PhysAddr);
}

//------------------------------------------------------------------------------
/*virtual*/ void GMLIT1AddressDecode::ComputePartition()
{
    switch (m_NumPartitions)
    {
        case 4:
        case 3:
        case 2:
        case 1:
            m_Partition = m_Remainder;
            break;
        default:
            Printf(Tee::PriError, "DecodeAddress: "
                "Unsupported partition number - 0x%x!\n",
                m_NumPartitions);
            MASSERT(!"Unsupported partition number!");
    }
}

//------------------------------------------------------------------------------
/*virtual*/ void GMLIT1AddressDecode::CommonCalcSliceParts8421()
{
    m_Slice = bits<10, 9>(m_Offset);

    // O[10:9] are slice[1:0] address into the L2 slice
    // Truncating
    m_L2addrin = static_cast<UINT32>((m_Quotient << 9) | bits<8, 0>(m_Offset));
    m_Padr = static_cast<UINT32> ((m_Quotient << 2) | bits<8, 7>(m_Offset));
}

//------------------------------------------------------------------------------
/*virtual*/ void GMLIT1AddressDecode::ComputeSlice()
{
    switch (m_NumPartitions)
    {
        case 8:
        case 4:
        case 2:
        case 1:
            CommonCalcSliceParts8421();
            break;
        default:
            Printf(Tee::PriError, "DecodeAddress: "
                "Unsupported partition number - 0x%x!\n",
                m_NumPartitions);
            MASSERT(!"Unsupported partition number!");
        }
}

//------------------------------------------------------------------------------
/*virtual*/ void GMLIT1AddressDecode::ComputeRBSC()
{
    bool ans = false;
    m_FbPadr = m_Padr; //Observed in Fmodel
    if (!ComputeFbSubPartition())
    {
        MASSERT(!"Cannot compute Subpartition!");
        return;
    }
    switch (m_NumDRAMBanks)
    {
        case 4:
            ans = CalcRBCS4Parts2Banks4Bool();
            break;
        case 8:
            ans = CalcRBCS4Parts2Banks8Bool();
            break;
        case 16:
            ans = CalcRBCS4Parts2Banks16Bool();
            break;
        default:
            Printf(Tee::PriError, "DecodeAddress: "
            "Unsupported number of DRAM banks - %u!\n",
            m_NumDRAMBanks);
            MASSERT(!"Unsupported number of DRAM banks!");
    }
    if (!ans)
    {
        MASSERT(!"Cannot compute RBSC!");
    }
}

//------------------------------------------------------------------------------
/*virtual*/ bool GMLIT1AddressDecode::ComputeFbSubPartition()
{
    m_DRAMBankSwizzle = false;
    if (m_NumXbarL2Slices!= 4)
    {
        return false;
    }

    m_SubPart = bit<1>(m_Slice);
    //The following is unreachable code in fmodel. I am keeping this code here to be
    //verified on actual SDDR4 ram

    if (m_NumDramsPerDramc==2)
    {
        // SDDR4 pairing
        // Slice 0 wil become one of the column bits later. We will push it into the fb_padr.
        // This has been done in past projects. I am going to trust that it works.
        m_FbPadr =  ((this->m_FbPadr & ~3) << 1 ) | (bit<0>(m_Slice) << 2)
            | bits<1, 0>(this->m_FbPadr);
        // For the pairing studies, there is no reason to callwlate the sub-parition.
        // We should however callwlate the physical dram mask we want to goto
        // note that slice[1] will goto the lower bit of the address.
    }
    else
    {  // 64b case: inject slice bit into the padr field for case where 2 slices feed 1 subpa
        // existing fb_padr mapping
        // altered fb_padr for floorsweeping
        m_FbPadr =  ((m_FbPadr & ~3) << 1 ) | (bit<9>(m_Offset) << 2)
            | bits<1, 0>(m_FbPadr);

    }
    return true;
}

//------------------------------------------------------------------------------
/*virtual*/ bool GMLIT1AddressDecode::CalcRBCS4Parts2Banks4Bool()
{

    UINT32 num_Col_bits = ComputeColumnBits();
    UINT32 rb=0;

    UINT32 aligned_sector =  (m_L2SectorMsk & 0xF)
        | ((m_L2SectorMsk >> 4) & 0xF);
    aligned_sector = (UINT32)ceil( log((double)aligned_sector)/log(2.0) );

    m_Col = (bits<2, 0>(m_FbPadr) << 5)
        | (bits<1, 0>(aligned_sector)<<3) |  bits<4, 2>(m_L2Offset);

    // m_FbPadr = { Q[n-1:0], O[9:7] }, O[10] = subpartition, R[0] = partition
    rb = bits<31, 3>(m_FbPadr);

    // For bank hit 2 blocks apart, row[n-1:18], bank[17:16], col[15], bank[14:13]
    // 2KB pages, col bit is mixed in with the bank bits
    // We only support col_bits >= 9
    UINT32 tmp = rb % (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1));
    m_ExtBank = bit<3>(tmp);

    // stick on 2KB col bit
    m_Col = m_Col | (bit<2>(tmp) << 8);

    m_Bank =
        (bit<1>(tmp) << 1) |
        (bit<0>(tmp) );

    // includes "column-as-row" bits
    m_Row = rb / (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1));

    // Swizzle
    // swizzles are tailored to number bank groups.
    const INT32 NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS;

    // No conselwtive two randoms repeat, no 0 element
    UINT32 galois[] =
    {3, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3, 1, 2, 1, 3, 1, 2, 1, 3, 1, 2, 3, 2,
        1, 2, 3, 1, 3, 2, 3};

    // generate swizzle
    UINT64 upper_address = m_Row >> 3;
    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    // manilwred bank swizzling
    UINT32 swizzle10 =
        (bit<1>(m_Row) << 1) |
        ((bit<0>(m_Row) ^ bit<1>(m_Row)) ^ bit<2>(m_Row));

    // Must ^ only against banks to preserve bank group relationships in mapping.
    m_Bank = m_Bank ^ swizzle10 ^ swizzle;

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 9) | m_Col) & ((1 << num_Col_bits) - 1);
    MASSERT(num_Col_bits>=9);
    m_Row = m_Row >> (num_Col_bits - 9);
    return true;
}

//------------------------------------------------------------------------------
/*virtual*/ bool GMLIT1AddressDecode::CalcRBCS4Parts2Banks8Bool()
{
    UINT32 aligned_sector =  (m_L2SectorMsk & 0xF)
        | ((m_L2SectorMsk >> 4) & 0xF);
    aligned_sector = (UINT32)ceil( log((double)aligned_sector)/log(2.0) );

    m_Col = (bits<2, 0>(m_FbPadr) << 5)
        | (bits<1, 0>(aligned_sector)<<3) |  bits<4, 2>(m_L2Offset);

    UINT32 num_Col_bits = ComputeColumnBits();
    UINT32 rb=0;

    // m_FbPadr = { Q[n-1:0], O[9:7] }, O[10] = subpartition, R[0] = partition
    rb = bits<31, 3>(m_FbPadr);

    // For bank hit 2 blocks apart, row[n-1:18], bank[17:16], col[15], bank[14:13]
    // 2KB pages, col bit is mixed in with the bank bits
    // We only support col_bits >= 9
    UINT32 tmp = rb % (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1));
    m_ExtBank = bit<4>(tmp);

    // stick on 2KB col bit
    m_Col = m_Col | (bit<2>(tmp) << 8);

    // Want to step through bank groups.
    // 4 bank groups: BG[1:0] = bank[2:1]
    // 2 bank groups: BG[0] = bank[2]
    m_Bank =
        (bit<0>(tmp) << 2) | // LSBs in top bits for BG
        (bit<1>(tmp) << 1) |
        (bit<3>(tmp) << 0);

    // includes "column-as-row" bits
    m_Row = rb / (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1));

    // Swizzle
    // swizzles are tailored to number bank groups.
    const INT32 NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS;

    // For 4 bank group, R[2:1] cannot be 00, two conselwtive R[2:1] the same not allowed.
    // e.g. 6, 6 not allowed.
    // 6 bit overlap in sliding window on address range
    // No conselwtive two randoms repeat, no 1 element
    // No two conselwtive with top two bits equal - used for 4 bank groups
    UINT32 galois_4BG[] =
    {2, 5, 7, 3, 4, 2, 7, 2, 4, 3, 6, 4, 7, 2, 6, 5, 3, 5, 2, 4, 7, 3, 7, 3,
        5, 7, 5, 7, 4};

    // generate swizzle
    UINT64 upper_address = m_Row >> 3;

    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois_4BG[i]);
    }

    // manilwred bank swizzling
    UINT32 swizzle20 =
        ((bit<2>(m_Row) ^ bit<0>(m_Row) ^ bit<1>(m_Bank) ^ bit<0>(m_Bank)) << 2) |
        ((bit<1>(m_Row) ^ bit<0>(m_Row)) << 1) |
        (bit<0>(m_Row) ^ bit<1>(m_Row));

    // Must ^ only against banks to preserve bank group relationships in mapping.
    m_Bank = m_Bank ^ swizzle20 ^ swizzle;
    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 9) | m_Col) & ((1 << num_Col_bits) - 1);
    MASSERT(num_Col_bits>=9);
    m_Row = m_Row >> (num_Col_bits - 9);
    return true;
}

//------------------------------------------------------------------------------
/*virtual*/ bool GMLIT1AddressDecode::CalcRBCS4Parts2Banks16Bool()
{
    UINT32 aligned_sector =  (m_L2SectorMsk & 0xF)
        | ((m_L2SectorMsk >> 4) & 0xF);
    aligned_sector = (UINT32)ceil( log((double)aligned_sector)/log(2.0) );

    m_Col = (bits<2, 0>(m_FbPadr) << 5) | (bits<1, 0>(aligned_sector)<<3)
        |  bits<4, 2>(m_L2Offset);

    UINT32 num_Col_bits = ComputeColumnBits();
    UINT32 rb=0;

    // m_FbPadr = { Q[n-1:0], O[9:7] }, O[10] = subpartition, R[0] = partition
    rb = bits<31, 3>(m_FbPadr);
    // For bank hit 2 blocks apart, row[n-1:18], bank[17:16], col[15], bank[14:13]
    // 2KB pages, col bit is mixed in with the bank bits
    // We only support col_bits >= 9
    UINT32 tmp = rb % (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1));
    m_ExtBank = bit<5>(tmp);

    // stick on 2KB col bit
    m_Col = m_Col | (bit<2>(tmp) << 8);

    // Want to step through bank groups.  BG[1:0] = bank[3:2]
    m_Bank =
        (bit<1>(tmp) << 3) | // LSBs in top bits for BG
        (bit<0>(tmp) << 2) | //
        (bit<4>(tmp) << 1) |
        (bit<3>(tmp) << 0);

    // 8x2 bank grouping
    // this needs to be verified
    if (m_8BgSwizzle)
    {
        m_Bank =
            (bit<3>(m_Bank) << 3) |
            (bit<2>(m_Bank) << 2) |
            (bit<0>(m_Bank) << 1) |
            (bit<1>(m_Bank) << 0);
    }
    // includes "column-as-row" bits
    m_Row = rb / (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1));

    // Galois Swizzle
    const INT32 NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS;
    UINT32 galois[] =
    {1, 2, 4, 8, 9, 11, 15, 7, 14, 5, 10, 13, 3, 6, 12, 1, 2, 4, 8, 9, 11, 15,
        7, 14, 5, 10, 13, 3};

    UINT64 upper_address = m_Row >> 3;
    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }
    // manilwred bank swizzling
    UINT32 swizzle30 =
        (((bit<0>(m_Bank) ^ bit<1>(m_Bank)) ^ (bit<1>(m_Row) ^ bit<2>(m_Row))) << 3) |
        (((bit<0>(m_Bank) ^ bit<0>(m_Row)) ^ bit<1>(m_Row)) << 2) |
        (bit<2>(m_Row) << 1) | (bit<0>(m_Row) ^ bit<2>(m_Row));

    // Must ^ only against banks to preserve bank group relationships in mapping
    m_Bank = bits<3, 0>(m_Bank) ^ swizzle ^ swizzle30;
    // extract column as row bits
    // m_Col[c-1:9] = m_Row[c-10:0], where c > 9
    m_Col = ((m_Row << 9) | m_Col) & ((1 << num_Col_bits) - 1);
    MASSERT(num_Col_bits>=9);
    m_Row = m_Row >> (num_Col_bits - 9);
    return true;
}

//------------------------------------------------------------------------------
/*virtual*/ void GMLIT1AddressDecode::PrintDecodeDetails(UINT32 PteKind, UINT32 PageSize)
{
    Printf(Tee::PriNormal, "PA = 0x%x Kind = 0x%x PageSize=%uK\n"
            , unsigned(m_PhysAddr), PteKind, PageSize);
    Printf(Tee::PriNormal, "MMU  Q:R:O:PAKS = 0x%llx:0x%x:0x%x:0x%llx\n",
            m_Quotient, m_Remainder, m_Offset, m_PAKS);
    Printf(Tee::PriNormal, "XBAR Slice:Partition:Padr:P.I. = %u:%u:0x%x:0x%x\n",
            m_Slice, m_Partition,
            m_Padr, m_PartitionInterleave);
    Printf(Tee::PriNormal, "L2   Tag:Set:Bank:Sector:Offset = 0x%llx:0x%x:0x%x:0x%x:0x%x\n",
            m_L2Tag, m_L2Set, m_L2Bank, m_L2Sector, m_L2Offset);
    Printf(Tee::PriNormal, "DRAM Extbank:R:B:S:C = %u:0x%x:0x%x:%u:0x%x\n",
            m_ExtBank, m_Row, m_Bank, m_SubPart, m_Col);
}

//--------------------------ADDRESS ENCODE----------------------------------
GMLIT1AddressEncode::GMLIT1AddressEncode(GpuSubdevice* pSubdev) :
    GMlitxFbAddrMap(pSubdev),
    m_NumPartitions(pSubdev->GetFB()->GetLtcCount()),
    m_PAKS_9_8(0),
    m_PAKS_7(0)
{
    CalcColumnBankBits();
    m_PartitionInterleave = 0;
    PrintFbConfig(Tee::PriLow);
}

//--------------------------------------------------------------------------
void GMLIT1AddressEncode :: CalcColumnBankBits()
{
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
}

//--------------------------------------------------------------------------
void GMLIT1AddressEncode :: MapPAKSToAddress()
{
    switch (m_PteKind)
    {
        case LW_MMU_PTE_KIND_GENERIC_16BX2:
            switch (m_NumPartitions)
            {
                case 1:
                    m_PhysAddr =
                        ReversePAKSBlocklinearPS64KParts1();
                    break;
                case 2:
                    m_PhysAddr =
                        ReversePAKSBlocklinearPS64KParts2();
                    break;
            }
            break;
        case LW_MMU_PTE_KIND_PITCH_NO_SWIZZLE:
        case LW_MMU_PTE_KIND_PITCH:
            switch (m_NumPartitions)
            {
                case 1:
                    m_PhysAddr = ReversePAKSPitchPS64KParts1();
                    break;
                case 2:
                    m_PhysAddr = ReversePAKSPitchPS64KParts2();
                    break;
            }
            break;
        default:
            Printf(Tee::PriError, "DecodeAddress: "
                    "Unsupported PTE kind - 0x%02x!\n",
                    m_PteKind);
            MASSERT(!"Unsupported PTE Kind!");
    }
}

//--------------------------------------------------------------------------
UINT64 GMLIT1AddressEncode::EncodeAddress
(
    const FbDecode &decode,
    UINT32 pteKind,
    UINT32 pageSize,
    bool eccOn
)
{
    const FrameBuffer *pFb = m_pGpuSubdevice->GetFB();
    const UINT32 rowOffset = pFb->GetRowOffset(decode.burst, decode.beat,
                                               decode.beatOffset);
    m_Partition = pFb->VirtualFbioToVirtualFbpa(decode.virtualFbio);
    m_SubPart = decode.subpartition;
    m_ExtBank = decode.rank;
    m_Bank = decode.bank;
    m_Row = decode.row;
    m_Col = rowOffset / m_AmapColumnSize;
    m_ECCOn = eccOn;

    m_PteKind = pteKind;
    m_PageSize = pageSize;

    m_PartitionInterleave = ComputePartitionInterleave(m_NumPartitions);

    MapRBCToXBar();
    MapXBarToPAKS();
    MapPAKSToAddress();

    // 0 out the lower 12 bits since we just need the page offset.
    m_PhysAddr = maskoff_lower_bits<12>(m_PhysAddr);

    Printf(Tee::PriLow, "GMLIT1 Address Encode 0x%llx, kind 0x%02x, page %uKB: "
            "numpart %u, "
            "fbpa %u, "
            "subpart %u, "
            "extbank %u, "
            "bank %u, "
            "row 0x%x, "
            "column 0x%x\n",
            m_PhysAddr, m_PteKind, m_PageSize
            , m_NumPartitions, m_Partition, m_SubPart,
            m_ExtBank, m_Bank, m_Row, m_Col);
    Printf(Tee::PriLow, "Detailed Encode for 0x%llx: "
            "paks 0x%llx, q 0x%llx, r 0x%x, o 0x%x, s %u, "
            "l2set %u, l2t %llu, l2o %u, paks98 0x%llx, paks7 0x%llx\n",
            m_PhysAddr, m_PAKS, m_Quotient, m_Remainder, m_Offset, m_Slice,
            m_L2Set, m_L2Tag, m_L2Offset, m_PAKS_9_8, m_PAKS_7);

    return m_PhysAddr + (rowOffset % m_AmapColumnSize);
}

//------------------------------------------------------------------------------
void GMLIT1AddressEncode::MapRBCToXBARPart2Banks8()
{
    // inputs: row, bank, extbank, col
    // want to recover fb_padr
    // Assuming at least 9 column bits, 2KB pages
    // Put upper column bits back into row

    UINT32 num_col_bits = ComputeColumnBits();
    //UINT32 num_col_bits = CalcColumnBits();
    MASSERT(num_col_bits>=9);
    UINT32 row = m_Row << (num_col_bits - 9) | (m_Col >> 9);

    // Galois Swizzle - make sure this is the same as forward swizzling
    const INT32 NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS;
    UINT32 galois[] = {2, 5, 7, 3, 4, 2, 7, 2, 4, 3, 6, 4, 7, 2, 6, 5, 3, 5,
        2, 4, 7, 3, 7, 3, 5, 7, 5, 7, 4};

    UINT64 upper_address = row >> 3;
    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    // reverse manilwred swizzle
    UINT32 swizzle20_minus_bank =
        ( (bit<2>(row) ^ bit<0>(row)) << 2) |
        ( (bit<1>(row) ^ bit<0>(row)) << 1) |
        (bit<0>(row) ^ bit<1>(row));

    // reverse swizzle the bank
    UINT32 bank = m_Bank ^ swizzle ^ swizzle20_minus_bank;
    bank = bank ^ ( (bit<1>(bank) ^ bit<0>(bank)) << 2);
    // undo bank grouping arranging
    UINT32 tmp =
        (bit<0>(bank) << 3) |
        (bit<1>(bank) << 1) |
        (bit<2>(bank) << 0);

    // recover column bit 8
    tmp = tmp | (((m_Col >> 8) & 1) << 2);

    // recover extbank
    tmp = tmp | (m_ExtBank << 4);
    // recover rb
    UINT32 rb = row * (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1)) | tmp;
    // recover O[9:7] from column
    m_Fb_Padr = (rb << 3) | bits<7, 5>(m_Col);
    // recover O[8:7] from column
    m_Padr = (rb << 2) | bits<6, 5>(m_Col);

    m_L2SectorMsk = 1 << bits<4, 3>(m_Col);
    m_Slice = bit<7>(m_Col)|(m_SubPart<<1);
    m_L2Offset = (bits<4, 3>(m_Col)<<5) | bits<2, 0>(m_Col)<<2;
}

//--------------------------------------------------------------------------
void GMLIT1AddressEncode::MapRBCToXBARPart2Banks16()
{
    // inputs: row, bank, extbank, col
    // want to recover fb_padr

    // Assuming at least 9 column bits, 2KB pages
    // Put upper column bits back into row
    UINT32 num_col_bits = ComputeColumnBits();
    MASSERT(num_col_bits>=9);
    UINT32 row = m_Row << (num_col_bits - 9) | (m_Col >> 9);

    // Galois Swizzle - make sure this is the same as forward swizzling
    const INT32 NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS;
    UINT32 galois[] =
    {1, 2, 4, 8, 9, 11, 15, 7, 14, 5, 10, 13, 3, 6, 12, 1, 2, 4, 8, 9, 11,
        15, 7, 14, 5, 10, 13, 3};

    UINT64 upper_address = row >> 3;

    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    // reverse manilwred swizzle
    UINT32 swizzle30_minus_bank =
        ((bit<1>(row) ^ bit<2>(row)) << 3) |
        ((bit<0>(row) ^ bit<1>(row)) << 2) |
        (bit<2>(row) << 1) |
        (bit<0>(row) ^ bit<2>(row));

    // reverse swizzle the bank
    UINT32 bank = m_Bank ^ swizzle ^ swizzle30_minus_bank;
    bank = bank ^ ((bit<0>(bank) ^ bit<1>(bank)) << 3);
    bank = bank ^ (bit<0>(bank) << 2);

    // undo bank grouping arranging
    UINT32 tmp =
        (bit<1>(bank) << 4) |
        (bit<0>(bank) << 3) |
        (bit<3>(bank) << 1) |
        (bit<2>(bank) << 0);

    // recover column bit 8
    tmp = tmp | (((m_Col >> 8) & 1) << 2);

    // recover extbank
    tmp = tmp | (m_ExtBank << 5);

    // recover rb
    UINT32 rb = row * (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1)) | tmp;

    // recover O[9:7] from column
    m_Fb_Padr = (rb << 3) | bits<7, 5>(m_Col);
    // recover O[8:7] from column
    m_Padr = (rb << 2) | bits<6, 5>(m_Col);
    m_L2SectorMsk = 1 << bits<4, 3>(m_Col);
    m_Slice = bit<7>(m_Col)|(m_SubPart<<1);
    m_L2Offset = (bits<4, 3>(m_Col)<<5) | bits<2, 0>(m_Col)<<2;
}

//--------------------------------------------------------------------------
void GMLIT1AddressEncode::MapRBCToXBARPart1Banks8()
{
    // inputs: row, bank, extbank, col
    // want to recover fb_padr
    // Assuming at least 9 column bits, 2KB pages
    // Put upper column bits back into row
    UINT32 num_col_bits = ComputeColumnBits();
    MASSERT(num_col_bits>=9);
    UINT32 row = m_Row << (num_col_bits - 9) | (m_Col >> 9);

    // Galois Swizzle - make sure this is the same as forward swizzling
    const INT32 NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS;
    UINT32 galois[] = {2, 5, 7, 3, 4, 2, 7, 2, 4, 3, 6, 4, 7, 2, 6, 5, 3, 5,
        2, 4, 7, 3, 7, 3, 5, 7, 5, 7, 4};

    UINT64 upper_address = row >> 3;
    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    // reverse manilwred swizzle
    UINT32 swizzle20_minus_bank =
        (bit<0>(row) << 2) |
        (bit<1>(row) <<1) |
        bit<2>(row);

    // reverse swizzle the bank
    UINT32 bank = m_Bank ^ swizzle ^ swizzle20_minus_bank;
    bank = bank ^ (bit<0>(bank) << 1);

    // undo bank grouping arranging
    UINT32 tmp =
        (bit<0>(bank) << 2) |
        (bit<1>(bank) << 1) |
        (bit<2>(bank) << 0);

    // recover column bit 8
    tmp = tmp | (((m_Col >> 8) & 1) << 3);

    // recover extbank
    tmp = tmp | (m_ExtBank << 4);

    // recover rb
    UINT32 rb = row * (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1)) | tmp;

    // recover O[9:7] from column
    m_Fb_Padr = (rb << 3) | bits<7, 5>(m_Col);
    // recover O[8:7] from column
    m_Padr = (rb << 2) | bits<6, 5>(m_Col);
    m_L2SectorMsk = 1 << bits<4, 3>(m_Col);
    m_Slice = bit<7>(m_Col)|(m_SubPart<<1);
    m_L2Offset = (bits<4, 3>(m_Col)<<5) | bits<2, 0>(m_Col)<<2;
}

//--------------------------------------------------------------------------
void GMLIT1AddressEncode :: MapRBCToXBARPart1Banks16()
{
    // inputs: row, bank, extbank, col
    // want to recover fb_padr

    // Assuming at least 9 column bits, 2KB pages
    // Put upper column bits back into row
    UINT32 num_col_bits = ComputeColumnBits();
    MASSERT(num_col_bits>=9);
    UINT32 row = m_Row << (num_col_bits - 9) | (m_Col >> 9);

    // Galois Swizzle - make sure this is the same as forward swizzling
    const INT32 NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS;
    UINT32 galois[] = {1, 2, 4, 8, 9, 11, 15, 7, 14, 5, 10, 13, 3, 6, 12, 1
        , 2, 4, 8, 9, 11, 15, 7, 14, 5, 10, 13, 3};

    UINT64 upper_address = row >> 3;

    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i=0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    // reverse manilwred swizzle
    UINT32 swizzle30_minus_bank =
        (bit<0>(row) << 3) |
        (bit<1>(row) << 2) |
        bit<2>(row);

    // reverse swizzle the bank
    UINT32 bank = m_Bank ^ swizzle ^ swizzle30_minus_bank;
    bank = bank ^ ((bit<0>(bank) << 3) | (bit<1>(bank) << 2));

    // undo bank grouping arranging
    UINT32 tmp =
        (bit<1>(bank) << 4) |
        (bit<0>(bank) << 2) |
        (bit<3>(bank) << 1) |
        (bit<2>(bank) << 0);

    // recover column bit 8
    tmp = tmp | (((m_Col >> 8) & 1) << 3);

    // recover extbank
    tmp = tmp | (m_ExtBank << 5);

    // recover rb
    UINT32 rb = row * (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1)) | tmp;

    // recover O[9:7] from column
    m_Fb_Padr = (rb << 3) | bits<7, 5>(m_Col);
    // recover O[8:7] from column
    m_Padr = (rb << 2) | bits<6, 5>(m_Col);
    m_L2SectorMsk = 1 << bits<4, 3>(m_Col);
    m_Slice = bit<7>(m_Col)|(m_SubPart<<1);
    m_L2Offset = (bits<4, 3>(m_Col)<<5) | bits<2, 0>(m_Col)<<2;
}

//--------------------------------------------------------------------------
void GMLIT1AddressEncode::MapRBCToXBar()
{
    switch (m_NumDRAMBanks)
    {
        case 4:
            break;

        case 8:
            switch (m_NumPartitions)
            {
                case 2:
                case 1:
                    MapRBCToXBARPart2Banks8();
                    break;
            }

            break;

        case 16:
            switch (m_NumPartitions)
            {
                case 2:
                    MapRBCToXBARPart2Banks16();
                    break;
                case 1:
                    MapRBCToXBARPart1Banks16();
                    break;
            }
            break;
        default:
            Printf(Tee::PriWarn, " Can encode only 4, 8, 16 number of dram banks\n");
            break; //Condition being let go as pass with  print as per fmodel
    }
}

/***************************************************************************
* L2 slice
*
**************************************************************************/
//--------------------------------------------------------------------------
void GMLIT1AddressEncode::CalcSlicesFromQRO(UINT64 Q, UINT64 R, UINT64 O)
{
    m_Slice = static_cast <UINT32> (bits<10, 9>(O));

    // O[10:9] are slice[1:0]
    // address into the L2 slice
    m_L2addrin = static_cast <UINT32> ((Q << 9) | bits<8, 0>(O));
    // other testbenches use padr (xbar addr)
    m_Padr = static_cast <UINT32> ((Q << 2) | bits<8, 7>(O));
}

//--------------------------------------------------------------------------
void GMLIT1AddressEncode::XBARtoPAKSpart1()
{
    // need partition, subpa, fb_padr, col
    // Want to generate Q, R, O, slice, L2 address, PAKS
    // Generate Q, R, O from fb_padr, col, and subpart

    UINT32 hacksector;
    switch (m_L2SectorMsk)
    {
        case 1:
            hacksector = 0; break;
        case 2:
            hacksector = 1; break;
        case 4:
            hacksector = 2; break;
        case 8:
            hacksector = 3; break;
        case 16:
            hacksector = 4; break;
        case 32:
            hacksector = 5; break;
        case 64:
            hacksector = 6; break;
        case 128:
            hacksector = 7; break;
        case 256:
            hacksector = 8; break;
        case 512:
            hacksector = 9; break;
        case 1024:
            hacksector = 10; break;
        case 2048:
            hacksector = 11; break;
        default:
            hacksector = 0;
            Printf(Tee::PriLow,
                    "Sector mask needs to be 1, 2, 4, 8\n");
            break;
    }

    m_Quotient = m_Padr >> 2;
    // padr starts on byte address 7, 256B aligned has padr[0]=0
    // client can set both sector_mask[7:4] and padr[0]=1 for upper 128B
    m_Offset = (m_Slice << 9) | (bits<1, 0>(m_Padr) << 7) | (hacksector << 5)
        | (m_L2Offset & 31);

    m_Remainder = m_Partition;

    // Callwlate slices
    CalcSlicesFromQRO(m_Quotient, m_Remainder, m_Offset);

    // Generate L2 address
    ComputeL2Address();

    // Generate PAKS from q, r, o
    m_PAKS = (m_Quotient * m_NumPartitions + m_Remainder) *
        m_PartitionInterleave | m_Offset;
}

//--------------------------------------------------------------------------
/*virtual*/ void GMLIT1AddressEncode::MapXBarToPAKS()
{
    switch (m_NumPartitions)
    {
        case 1:
        case 2:
            XBARtoPAKSpart1();
            break;
        default:
            Printf(Tee::PriError, "Unsupported number of partitions - %u\n",
                    m_NumPartitions);
            MASSERT(!"Unsupported number of parts!");
            break;
    }
}
