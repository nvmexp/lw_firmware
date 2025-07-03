/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2017,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gmlitxadr.h"
#include "bitop.h"
#include "maxwell/gm108/dev_mmu.h"
#include "maxwell/gm108/dev_fbpa.h"
#include "maxwell/gm108/dev_ltc.h"

using namespace BitOp;

//------------------------------------------------------------------------------
GMlitxFbAddrMap :: GMlitxFbAddrMap(GpuSubdevice* pSubdev) :
    m_PhysAddr(0),
    m_PteKind(0),
    m_PageSize(0),
    m_NumL2Sets(32),
    m_NumL2SetsBits(5),
    m_FBDataBusWidth(32),
    m_NumL2Sectors(4),
    m_NumL2Banks(4),
    m_pGpuSubdevice(pSubdev),
    m_NumPartitions(pSubdev->GetFB()->GetLtcCount()),
    m_FbioChannelWidth(0),
    m_PartitionInterleave(0),
    m_IsGF10x(false),
    m_IsGF108(false),
    m_IsGF119(false),
    m_Fb_Padr(0),
    m_FbPadr(0),
    m_NumDramsperDramc(0),
    m_Padr(0),
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
    m_NumDramsPerDramc(1),  //By default non SDDR4 mem
    m_Bank(0),
    m_ExtBank(0),
    m_Row(0),
    m_Col(0),
    m_L2BankSwizzleSetting(false),
    m_8BgSwizzle(false)
{
    const FrameBuffer *pFb = pSubdev->GetFB();

    m_NumDramsPerDramc = 2 *
       DRF_VAL(_PFB, _FBPA_FBIO_BROADCAST, _2T_CMD_MODE,
              pSubdev->RegRd32(LW_PFB_FBPA_FBIO_BROADCAST)); // 0 or 1
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
    m_Padr = ~0x00;
    m_L2addrin = ~0x00;
    m_L2SectorMsk = 0;
}

//------------------------------------------------------------------------------
void GMlitxFbAddrMap :: PrintFbConfig(INT32 Pri)
{
    Printf(Pri, "====== FB config ======\n");
    Printf(Pri, "  numPartitions   %u\n", m_NumPartitions);
    Printf(Pri, "  NumXbarL2Slices %u\n", m_NumXbarL2Slices);
    Printf(Pri, "  NumDRAMBanks    %u\n", m_NumDRAMBanks);
    Printf(Pri, "  NumExtDRAMBanks %u\n", m_NumExtDRAMBanks);
    Printf(Pri, "  DRAMPageSize    %u\n", m_DRAMPageSize);
    Printf(Pri, "  DRAMBankSwizzle %s\n", m_DRAMBankSwizzle?"true":"false");
    Printf(Pri, "  NumL2Sets       %u\n", m_NumL2Sets);
    Printf(Pri, "  ECCOn           %s\n", m_ECCOn?"true":"false");
    Printf(Pri, "======================\n");
}

//------------------------------------------------------------------------------
/*virtual*/ UINT32 GMlitxFbAddrMap::ComputePartitionInterleave
(
    UINT32 numPartitions
)
{
    UINT32 partitionInterleave = 0;
    switch (numPartitions)
    {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
            partitionInterleave = (m_NumXbarL2Slices==2) ? 1024:2048; //works gmlit1 and gmlit2
            break;
        default:
            Printf(Tee::PriError, "%s Invalid number of partitions [%u]\n",
                    __FUNCTION__, numPartitions);
            MASSERT(!"Invalid number of partitions");
            break;
    }
    return partitionInterleave;
}

//Decode Tool
//------------------------------------------------------------------------------
UINT64 GMlitxFbAddrMap::PhysicalToRaw(UINT64 physFbOffset, UINT32 pteKind)
{
    switch (pteKind)
    {
        case LW_MMU_PTE_KIND_GENERIC_16BX2:
            {
                //Based on
                static const UINT32 physicalToRawMap[32] =
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
                const UINT32 physicalGobOffset =
                    static_cast<UINT32>(physFbOffset & 0x1F0);
                const UINT32 rawGobOffset =
                    physicalToRawMap[physicalGobOffset>>4];
                return (physFbOffset & ~0x1F0) | rawGobOffset;
            }
        case LW_MMU_PTE_KIND_PITCH_NO_SWIZZLE:
        case LW_MMU_PTE_KIND_PITCH:
            return physFbOffset;
        default:
            Printf(Tee::PriError, "%s Invalid PTE Kind [%u]\n",
                    __FUNCTION__, pteKind);
            MASSERT(!"Unsupported PTE Kind!");
            return 0;
    }
}

//------------------------------------------------------------------------------
/*virtual*/ void GMlitxFbAddrMap::ComputeQRO()
{
    m_PartitionInterleave = ComputePartitionInterleave(m_NumPartitions);
    m_Offset = (UINT32) (m_PAKS % m_PartitionInterleave);
    const UINT64 tmp = m_PAKS / m_PartitionInterleave;
    m_Quotient = tmp / m_NumPartitions;
    m_Remainder = (UINT32)(tmp % m_NumPartitions);
}

//------------------------------------------------------------------------------
UINT32 GMlitxFbAddrMap::ComputeColumnBits() const
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
            Printf(Tee::PriError,
                   "DecodeAddress: Unsupported DRAM page size - %u!\n",
                    m_DRAMPageSize);
            MASSERT(!"Unsupported DRAM page size!");
            return 0;
    }
}

//------------------------------------------------------------------------------
UINT32 GMlitxFbAddrMap::ComputeXbarRawPaddr()
{
    return (static_cast <UINT32> (m_L2Tag) << m_NumL2SetsBits) | m_L2Set;
}

//------------------------------------------------------------------------------
/*virtual*/ void GMlitxFbAddrMap::CalcL2Sector(UINT32 offset)
{
    //sector mapping is just about address offset, not L2 bank.
    m_L2Sector = (offset / m_FBDataBusWidth ) % m_NumL2Sectors;
    if (m_L2BankSwizzleSetting)
    {
        m_L2Bank = (bits<2, 1>(m_L2Set) + bit<3>(m_L2Set) + m_L2Sector) % m_NumL2Banks;
    }
    else
    {
        m_L2Bank = m_L2Sector % m_NumL2Banks;  // without L2 bank swizzling
    }
    m_L2Offset = offset % (m_FBDataBusWidth * m_NumL2Sectors);
    if (m_PteKind == LW_MMU_PTE_KIND_PITCH||
        m_PteKind == LW_MMU_PTE_KIND_PITCH_NO_SWIZZLE)
    {
        m_L2SectorMsk = (1  << ((offset & 0x7F)/m_FBDataBusWidth) );
        if (m_Padr & 0x1)
        {
            //padr[0]==1 with pitch will have mask in upper portion.
            m_L2SectorMsk <<= 4;
        }
    }
    else
    {
        m_L2SectorMsk = (1  << ((offset & 0xFF)/m_FBDataBusWidth));
    }
}

//------------------------------------------------------------------------------
/*virtual*/ void GMlitxFbAddrMap::CalcL2AddressSet16()
{
    // galois Swizzle
    // FFT ranking, score, 28 randoms
    // 0, 22.8583, 14, 7, 10, 5, 11, 12, 6, 3, 8, 4, 2, 1, 9, 13, 15, 14, 7, 10,
    // 5, 11, 12, 6, 3, 8, 4, 2, 1, 9
    const INT32 numSwizzleBits = 28;
    UINT32 galois[] =
    {14, 7, 10, 5, 11, 12, 6, 3, 8, 4, 2, 1, 9, 13, 15, 14, 7, 10, 5,
        11, 12, 6, 3, 8, 4, 2, 1, 9};

    // shift down by l2sets + 128B L2 cacheline size
    m_L2Tag = m_L2addrin >> (4+7);
    // generate swizzle
    UINT32 L2set_swizzle= 0;
    for (INT32 i=0; i < numSwizzleBits; ++i)
    {
        L2set_swizzle = L2set_swizzle ^ (((m_L2Tag>>i)&1)*galois[i]);
    }

    UINT32 L2Set = bits<3, 0>(m_L2addrin>>7);
    m_L2Set = L2Set ^ L2set_swizzle;
    CalcL2Sector(m_L2addrin);
}

//------------------------------------------------------------------------------
/*virtual*/ void GMlitxFbAddrMap::CalcL2AddressSet32()
{
    // galois Swizzle
    // FFT ranking, score, 27 randoms
    // 0, 13.5892, 15, 30, 21, 3, 6, 12, 24, 25, 27, 31, 23, 7, 14, 28, 17, 11,
    // 22, 5, 10, 20, 1, 2, 4, 8, 16, 9, 18
    const INT32 numSwizzleBits = 27;
    UINT32 galois[] =
    {15, 30, 21, 3, 6, 12, 24, 25, 27, 31, 23, 7, 14, 28, 17, 11, 22, 5,
        10, 20, 1, 2, 4, 8, 16, 9, 18};

    // shift down by l2sets + 128B L2 cacheline size
    m_L2Tag = m_L2addrin >> (5+7);
    // generate swizzle
    UINT32 l2set_swizzle= 0;
    for (INT32 i=0; i < numSwizzleBits; ++i)
    {
        l2set_swizzle = l2set_swizzle ^ (((m_L2Tag>>i)&1)*galois[i]);
    }

    UINT32 L2Set = bits<4, 0>(m_L2addrin>>7);
    m_L2Set = L2Set ^ l2set_swizzle;
    CalcL2Sector(m_L2addrin);
}

//------------------------------------------------------------------------------
/*virtual*/ void GMlitxFbAddrMap::CalcL2AddressSet1024()
{
     // galois Swizzle
     // FFT ranking, score, 22 randoms
     // randomly chosen
     const INT32 numSwizzleBits = 22;
     UINT32 galois[] =
     {1017, 11, 22, 44, 88, 176, 352, 704, 633, 779, 495, 990, 69, 138, 276, 552,
         937, 171, 342, 684, 673, 699};

     // shift down by l2sets + 128B L2 cacheline size
     m_L2Tag = m_L2addrin >> (10+7);
     // generate swizzle
     UINT32 l2set_swizzle= 0;
     for (INT32 i=0; i < numSwizzleBits; ++i)
     {
         l2set_swizzle = l2set_swizzle ^ (((m_L2Tag>>i)&1)*galois[i]);
     }

     UINT32 l2set = bits<9, 0>(m_L2addrin>>7);
     m_L2Set = l2set ^ l2set_swizzle;
     CalcL2Sector(m_L2addrin);
}

//------------------------------------------------------------------------------
/*virtual*/ void GMlitxFbAddrMap::CalcL2AddressSet512()
{
    // galois Swizzle
    // FFT ranking, score, 23 randoms
    // randomly chosen
    const INT32 numSwizzleBits = 23;
    UINT32 galois[23] =
    {507, 13, 26, 52, 104, 208, 416, 187, 374, 279, 469, 81, 162, 324, 371, 285,
        449, 121, 242, 484, 51, 102, 204};

    // shift down by l2sets + 128B L2 cacheline size
    m_L2Tag = m_L2addrin >> (9+7);
    // generate swizzle
    UINT32 l2set_swizzle= 0;
    for (INT32 i=0; i < numSwizzleBits; ++i)
    {
        l2set_swizzle = l2set_swizzle ^ (((m_L2Tag>>i)&1)*galois[i]);
    }

    UINT32 l2set = bits<8, 0>(m_L2addrin>>7);
    m_L2Set = l2set ^ l2set_swizzle;
    CalcL2Sector(m_L2addrin);
}

//------------------------------------------------------------------------------
/*virtual*/ void GMlitxFbAddrMap::CalcL2AddressSet256()
{
    // galois Swizzle
    // FFT ranking, score, 24 randoms
    // 0, 3.78199, 34, 17, 206, 103, 245, 188, 94, 47, 209, 174, 87, 237, 176,
    //88, 44, 22, 11, 195, 167, 149, 140, 70, 35, 215
    const INT32 numSwizzleBits = 24;
    UINT32 galois[] =
    {34, 17, 206, 103, 245, 188, 94, 47, 209, 174, 87, 237, 176, 88, 44, 22, 11,
        195, 167, 149, 140, 70, 35, 215};

    // shift down by l2sets + 128B L2 cacheline size
    m_L2Tag = m_L2addrin >> (8+7);
    // generate swizzle
    UINT32 l2set_swizzle= 0;
    for (INT32 i=0; i < numSwizzleBits; ++i)
    {
        l2set_swizzle = l2set_swizzle ^ (((m_L2Tag>>i)&1)*galois[i]);
    }

    UINT32 l2set = bits<7, 0>(m_L2addrin>>7);
    m_L2Set = l2set ^ l2set_swizzle;
    CalcL2Sector(m_L2addrin);
}

//------------------------------------------------------------------------------
/*virtual*/ void GMlitxFbAddrMap::CalcL2AddressSet64()
{
    // galois Swizzle
    // FFT ranking, score, 26 randoms
    // 0, 7.9006, 51, 61, 33, 25, 50, 63, 37, 17, 34, 31, 62, 39, 21, 42, 15,
    //30, 60, 35, 29, 58, 47, 5, 10, 20, 40, 11
    const INT32 numSwizzleBits = 26;
    UINT32 galois[] =
    {51, 61, 33, 25, 50, 63, 37, 17, 34, 31, 62, 39, 21, 42, 15, 30, 60, 35, 29,
        58, 47, 5, 10, 20, 40, 11};

    // shift down by l2sets + 128B L2 cacheline size
    m_L2Tag = m_L2addrin >> (6+7);
    // generate swizzle
    UINT32 l2set_swizzle= 0;
    for (INT32 i=0; i < numSwizzleBits; ++i)
    {
        l2set_swizzle = l2set_swizzle ^ (((m_L2Tag>>i)&1)*galois[i]);
    }

    UINT32 l2set = bits<5, 0>(m_L2addrin>>7);
    m_L2Set = l2set ^ l2set_swizzle;
    CalcL2Sector(m_L2addrin);
}

//------------------------------------------------------------------------------
/*virtual*/ void GMlitxFbAddrMap::CalcL2AddressSet128()
{
    //UINT64 PA_value = m_PAKS;
    // galois Swizzle
    const INT32 numSwizzleBits = 25;

    //2 bit overlap in sliding window on address range
    UINT32 galois[] =
    {58, 29, 82, 41, 72, 36, 18, 9, 88, 44, 22, 11, 89, 112, 56, 28, 14, 7,
        95, 115, 101, 110, 55, 71, 127};

    // shift down by l2sets + 128B L2 cacheline size
    m_L2Tag = m_L2addrin >> (7+7);
    // generate swizzle
    UINT32 l2set_swizzle= 0;
    for (INT32 i=0; i < numSwizzleBits; ++i)
    {
        l2set_swizzle = l2set_swizzle ^ (((m_L2Tag>>i)&1)*galois[i]);
    }

    UINT32 l2set = bits<6, 0>(m_L2addrin>>7);
    m_L2Set = l2set ^ l2set_swizzle;
    CalcL2Sector(m_L2addrin);
}

//------------------------------------------------------------------------------
/*virtual*/ void GMlitxFbAddrMap::ComputeL2Address()
{
    switch (m_NumL2Sets)
    {
        case 16:
            CalcL2AddressSet16();
            break;
        case 32:
            CalcL2AddressSet32();
            break;
        case 64:
            CalcL2AddressSet64();
            break;
        case 128:
            CalcL2AddressSet128();
            break;
        case 256:
            CalcL2AddressSet256();
            break;
        case 512:
            CalcL2AddressSet512();
            break;
        case 1024:
            CalcL2AddressSet1024();
            break;
        default:
            Printf(Tee::PriError, "Unspported number of L2Sets [%u]\n",
                    m_NumL2Sets);
        MASSERT(!"Unsupported L2 config!");
    }
}

//------------------------------------------------------------------------------
/*virtual*/ UINT64 GMlitxFbAddrMap :: ReversePAKSPitchPS64KParts2()
{
    UINT64 PA_value = m_PAKS;
    // galois Swizzle
    const INT32 numSwizzleBits = 30;
    UINT32 galois[] =
    {6, 7, 2, 1, 2, 3, 6, 5, 7, 2, 1, 3, 5, 2, 7, 5, 7, 1, 2, 5, 3, 1
        , 7, 6, 5, 3, 5, 2, 1, 3};
    // start at bit 11 for 1 partition pitch, since slice[1] is PA[10]
    UINT64 upperAddress = PA_value >> 12;
    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i=0; i < numSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upperAddress>>i)&1)*galois[i]);
    }

    return (bits<PADDR_MSB, 12>(PA_value) << 12) |
        // Don't swizzle above PA[12] since 4KB swizzle uses same mapping
        // using ^ here so that carry doesn't propagate into bit10 from changing
        // bit9, allows bit flipping in ROP
        ((bit<11>(PA_value) ^ bit<2>(swizzle)) << 11) |
        ((bit<10>(PA_value) ^ bit<1>(swizzle)) << 10) |
        (bit<8>(PA_value) << 9) |  // swap swizzle location for reverse mapping
        ((bit<9>(PA_value)  ^ bit<0>(swizzle)) << 8) | // swap swizzle location for reverse mapping
        bits<7, 0>(PA_value);
}

//------------------------------------------------------------------------------
/*virtual*/ UINT64 GMlitxFbAddrMap :: ReversePAKSPitchPS64KParts1()
{
    UINT64 PA_value = m_PAKS;
    // galois Swizzle
    const INT32 numSwizzleBits = 30;
    UINT32 galois[] =
    {2, 3, 2, 1, 3, 1, 2, 3, 1, 3, 1, 3, 2, 3, 2, 1, 3, 1, 2, 3, 1, 2,
        1, 2, 1, 3, 2, 1, 2, 3};

    // start at bit 11 for 1 partition pitch, since slice[1] is PA[10]
    UINT64 upperAddress = PA_value >> 11;
    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i=0; i < numSwizzleBits ; ++i)
    {
        swizzle = swizzle ^ (((upperAddress>>i)&1)*galois[i]);
    }
    return (bits<PADDR_MSB, 11>(PA_value) << 11) |
        // Don't swizzle above PA[11] since 4KB swizzle uses same mapping
        // using ^ here so that carry doesn't propagate into bit10 from changing
        // bit 9 allows bit flipping in ROP
        ((bit<10>(PA_value) ^ bit<1>(swizzle)) << 10) |
        (bit<8>(PA_value) << 9) |  // swap swizzle location for reverse mapping
        ((bit<9>(PA_value)  ^ bit<0>(swizzle)) << 8) |
        // swap swizzle location for reverse mapping
        bits<7, 0>(PA_value);
}

//------------------------------------------------------------------------------
/*virtual*/ UINT64 GMlitxFbAddrMap :: ReversePAKSBlocklinearPS64KParts1()
{
    UINT64 PA_value = m_PAKS;
    // galois Swizzle

    //Due to regularly changing value of Swizzle bits use of enum has been
    //avoided. Please refer to HW amap code for latest trial

    const INT32 numSwizzleBits = 30;
    UINT32 galois[] =
    {2, 3, 2, 1, 3, 1, 2, 3, 1, 3, 1, 3, 2, 3, 2, 1, 3, 1, 2, 3, 1, 2,
        1, 2, 1, 3, 2, 1, 2, 3};

    // start at bit 11 for 1 partition pitch, since slice[1] is PA[10]
    UINT64 upperAddress = PA_value >> 11;
    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i=0; i < numSwizzleBits ; ++i)
    {
        swizzle = swizzle ^ (((upperAddress>>i)&1)*galois[i]);
    }
    return (bits<PADDR_MSB, 11>(PA_value) << 11) |
        // Don't swizzle above PA[11] since 4KB swizzle uses same mapping
        // using ^ here so that carry doesn't propagate into bit10 from changing bit9, allows bit flipping in ROP
        ((bit<10>(PA_value) ^ bit<1>(swizzle)) << 10) |
        (bit<8>(PA_value) << 9) |  // swap swizzle location for reverse mapping
        ((bit<9>(PA_value)  ^ bit<0>(swizzle)) << 8) |   // swap swizzle location for reverse mapping
        bits<7, 0>(PA_value);
}

//------------------------------------------------------------------------------
/*virtual*/ UINT64 GMlitxFbAddrMap :: ReversePAKSBlocklinearPS64KParts2()
{
    UINT64 PA_value = m_PAKS;
    // galois Swizzle
    const INT32 numSwizzleBits = 30;

    //7 bit overlap in sliding window on address range, no conselwtive two randoms repeat
    UINT32 galois[] =
    {3, 2, 1, 3, 1, 3, 2, 1, 3, 1, 2, 1, 2, 1, 3, 1, 2, 3, 2,
        3, 2, 1, 2, 1, 3, 1, 3, 1, 3, 1};

    // start at bit 18 so slices don't interrupt the bank pattern across 4x64KB horizontal
    UINT64 upperAddress = PA_value >> 18;
    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i=0; i < numSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upperAddress>>i)&1)*galois[i]);
    }

    return (bits<PADDR_MSB, 11>(PA_value) << 11) |
        // don't want diagonals
        // using ^ here so that carry doesn't propagate into bit10 from changing
        // bit9, allows bit flipping in ROP
        ((bit<10>(PA_value) ^ bit<13>(PA_value) ^ bit<1>(swizzle)) << 10) |
        ((bit<9>(PA_value) ^ bit<14>(PA_value) ^ bit<0>(swizzle)) << 9) |
        bits<8, 0>(PA_value);

}
