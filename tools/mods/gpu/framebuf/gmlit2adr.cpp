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

// "Ecc On" case of decode is to be verified.
// Reverse Decode is to be added

#include "gmlit2adr.h"
#include "bitop.h"
#include "maxwell/gm200/dev_mmu.h"
#include "maxwell/gm200/dev_fbpa.h"
#include "maxwell/gm200/dev_ltc.h"
#include "maxwell/gm200/dev_fb.h"
#include "maxwell/gm200/dev_top.h"

#ifndef MATS_STANDALONE
#include "gpu/include/floorsweepimpl.h"
#endif

using namespace BitOp;

/*  [Ref:
          //hw/doc/gpu/maxwell/maxwell/design/Functional_Descriptions/address_mapping/gmlit2_address_mapping.doc,
          //hw/lwgpu/clib/lwshared/Memory/FermiAddressMapping.cpp,
          //hw/lwgpu/clib/lwshared/Memory/gmlit2.h and
          //hw/lwgpu/clib/lwshared/Memory/gmlit2_chip.h ]

    Maxwell GMLIT2 has:
    Ltcs 8, 10, 12
    Slices Per Ltc: 2, 3
    Ltcs/Partition Size: 1KB, Partition Interleave: 4KB
    *
    Order of translation:
    PAKS (Physical Address Kind Swap):
    (PA) -> (swizzled PA)

    Divide/Modulo:
    (swizzled PA) -> (Quotient, Remainder, Offset)

    XBar address:
    (Q, R, O) -> (part, slice, padr)

    L2 Address:
    (padr) -> (tag, set)

    CBC Address:
    (padr, comptagline, comptag) -> (comp bit address)

    FB Address:
    (padr) -> (row, bank, column, extbank, subp)

    RAMIF:
    (row, bank, column, extbank, subp, part) -> (RAMIF)

    Addresses are 128B aligned.
    Implicit in these addresses is the 4 bit mask specifying 4 x 32B subpackets within 128B.

    Note: Fmodel code still prints Ltcs as *partitions*. While verifying please match mods 'LtcId' with fmodel 'Part'.
*/

namespace
{
    const UINT32 s_MaxL2MaskPerFbp = 3;
    const UINT32 s_Gm20xUpperMemoryBit=36; //From //hw/lwgpu/clib/tools/utilities/AddressMappingUtilFModel/amap.cpp
    const UINT32 s_Gmlit2Obit9LocInPadr = 2;
    const UINT32 s_FbpaPadrBitWidth = 26;
    const UINT32 s_FbpaBitEnableUpperPartition = 31;
    inline UINT32 log2i(UINT32 x)
    {
        UINT32 ret = 0;
        while (x >>= 1)
        {
            ret++;
        }
        return ret;
    }
}

//------------------------------------------------------------------------------
GMLIT2AddressDecode::GMLIT2AddressDecode(GpuSubdevice* pSubdev) :
    GMLIT1AddressDecode(pSubdev),
    m_NumLtcs(pSubdev->GetFB()->GetLtcCount()),
    m_Partition(0),
    m_LtcId(0),
    m_DRAMBankSwizzle(false),
    m_EccLinesPerPage(15), //hw/lwgpu/clib/lwshared/Memory/FermiAddressMapping.cpp
    m_LtcLinesPerPage(16),
    m_LowerStart(0),
    m_UpperStart(0),
    m_LowerEnd(0),
    m_UpperEnd(0)
{
    m_PartitionInterleave = 0;
    m_ECCPageSize = m_DRAMPageSize;
#ifdef MATS_STANDALONE
    m_ECCOn = (0 !=
        DRF_VAL(_PFB, _FBPA_ECC_CONTROL, _MASTER_EN,
        pSubdev->RegRd32(LW_PFB_FBPA_ECC_CONTROL)));
#else
    UINT32 eccMask = 0;
    bool isEccSupported = false;
    if (OK == (pSubdev->GetEccEnabled(&isEccSupported, &eccMask)))
    {
        if (isEccSupported)
        {
            m_ECCOn = Ecc::IsUnitEnabled(Ecc::Unit::DRAM, eccMask);
        }
        else
        {
            m_ECCOn = false;
        }
    }
    else
    {
        m_ECCOn = false;
    }
#endif
    const UINT32 fbMask = pSubdev->GetFsImpl()->FbMask();
    const UINT32 fbps = (UINT32)Utility::CountBits(fbMask);

#ifdef MATS_STANDALONE
    m_NumUpperLtcs = 0;
#else
    m_NumUpperLtcs = DRF_VAL(_PFB, _FBHUB_NUM_ACTIVE_LTCS, _UPPER_COUNT,
            pSubdev->RegRd32(LW_PFB_FBHUB_NUM_ACTIVE_LTCS));
#endif
    //First hwFbp
    UINT32  hwFbNum = (UINT32)Utility::BitScanForward(fbMask, 0);
    for (UINT32 ii = 0; ii < fbps; ii++)
    {
        //Get the next HwFbNum from fbMask;
        UINT32 ltcMask = pSubdev->GetFsImpl()->L2Mask(hwFbNum);
#ifdef MATS_STANDALONE
        //This formula should not be used in full mods. In standalone mats
        // we cannot read the ACTIVE_LTCS register
        if (ltcMask == 2) //upper ltc is active
            m_NumUpperLtcs++; //Bug 1553815
#endif
        UINT32 numLtcPerFbp = Utility::CountBits(ltcMask);
        UINT32 rowBits = DRF_VAL(_PFB, _FBPA_0_CFG1, _ROWA,
                pSubdev->RegRd32(LW_PFB_FBPA_0_CFG1 + hwFbNum*0x1000));
        switch (rowBits)
        {
            case LW_PFB_FBPA_5_CFG1_ROWA_10:
                rowBits = 10;
                break;
            case LW_PFB_FBPA_5_CFG1_ROWA_11:
                rowBits = 11;
                break;
            case LW_PFB_FBPA_5_CFG1_ROWA_12:
                rowBits = 12;
                break;
            case LW_PFB_FBPA_5_CFG1_ROWA_13:
                rowBits = 13;
                break;
            case LW_PFB_FBPA_5_CFG1_ROWA_14:
                rowBits = 14;
                break;
            case LW_PFB_FBPA_5_CFG1_ROWA_15:
                rowBits = 15;
                break;
            case LW_PFB_FBPA_5_CFG1_ROWA_16:
                rowBits = 16;
                break;
            case LW_PFB_FBPA_5_CFG1_ROWA_17:
                rowBits = 17;
                break;
            default:
                Printf(Tee::PriWarn,
                       "%u Seems incorrect number of row bits as per refmanual."
                       " MODS will fallback to default value 13\n",
                       rowBits);
                rowBits = 13;
                break;
        }
        for (;numLtcPerFbp; numLtcPerFbp--)
        {
            m_FbpForLtcId.push_back(ii); //Per ltcId
            m_NumRowBitsPerLtc.push_back(rowBits);
        }
        m_LtcMaskForLFbp.push_back(ltcMask); //Per logical fbp
        //Next hwFbp
        hwFbNum = (UINT32)Utility::BitScanForward(fbMask, hwFbNum + 1);
    }
    FindAddressRanges(); //Determine lower and upper physicalfb offset range
}

//------------------------------------------------------------------------------
GMLIT2AddressDecode::~GMLIT2AddressDecode()
{
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode :: PrintFbConfig(INT32 Pri)
{
    Printf(Pri, "====== FB config ======\n");
    Printf(Pri, "  NumLtcs         %u\n", m_NumLtcs);
    Printf(Pri, "  NumUpperLtcs    %u\n", m_NumUpperLtcs);
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
UINT32 GMLIT2AddressDecode::ComputePI()
{
    // all maxwell has 2KB partition interleave so that SDDR4 pairing has good perf
    // good pairing locality depends on it
    UINT32 m_PartitionInterleave = 0;
    switch (m_NumLtcs)
    {
         // all maxwell has 2KB partition interleave so that SDDR4 pairing has good perf
        case 16:  //For full config of 8 Fb partitions in gmlit4 case
            m_PartitionInterleave = 4096;
            break;
        case 15:
        case 14:
        case 13:
        case 12:
        case 11:
        case 10:
        case 9:
        case 8:
        case 7:
        case 6:
        case 5:
        case 4:
        case 3: //3 Ltcs is also 2K PI.
        case 2:
        case 1:
            m_PartitionInterleave = 2048;
            break;

        default:
            //Confirm with Memory team what happens for fbps > 8 < 12
            Printf(Tee::PriError, "%s Invalid number of NumLtcs [%u]\n",
                    __FUNCTION__, m_NumLtcs);
            MASSERT(!"Invalid number of NumLtcs");
    }
    return m_PartitionInterleave;
}

//------------------------------------------------------------------------------
/*virtual*/ void GMLIT2AddressDecode::ComputeQRO()
{
    m_Offset = (UINT32) (m_PAKS % m_PartitionInterleave);
    const UINT32 tmp = static_cast <UINT32> (m_PAKS / m_PartitionInterleave);
    UINT32 divisor;

    switch (m_NumXbarL2Slices)
    {
        case 2:
        case 3:
            switch (m_NumLtcs)
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
                    //use 7 LTCs as an example for floorsweeping, and LTC 7 is bad.
                    // There are 8 DRAMs in the system, and 8 LTCs.
                    // When we floorsweep in 2, 3 slice mode, we'll step through
                    // 14 logical LTCs, which will map through 13 logical LTCs.
                    // But only 7 physical LTCs will be used.
                    // Good LTCs will be stepped through twice, the bad LTC will
                    // be skipped. All 8 DRAMs will be used (4 partitions x 2 subpartitions).
                    // Half of the FB DRAM pair belonging to the swept LTC will
                    // be used in lower memory, and the other half will be used
                    // in the "box on top" remainder single partition mapping at high mem.
                    // numPart is set to numLTC.  For floorsweeping, 7, 9, 11.
                    divisor = m_NumLtcs;
                    m_Quotient = tmp / divisor;
                    m_Remainder = tmp % divisor;
                    break;
            }
            break;
        case 4: // 4 slices
            divisor = m_NumLtcs;
            m_Quotient = tmp / divisor;
            m_Remainder = tmp % divisor;
            break;
        default:
            Printf(Tee::PriError, " Invalid number of L2 slices %d\n",
                    m_NumXbarL2Slices);
            MASSERT(0);
    } //switch (m_NumXbarL2Slices)
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::ComputePartition()
{
    switch (m_NumXbarL2Slices)
    {
        case 2:
        case 3:
            switch (m_NumLtcs)
            {
                case 1:
                case 2:
                case 4:
                case 6:
                case 8:
                case 10:
                case 12:
                    m_LtcId = m_Remainder;
                    break;
                    // Floorswept partitions are 2KB m_partitioninterleave.
                    // We want to separate interleaved traffic from box on top traffic by subpartition.
                case 3: //2KB PI
                    {
                        UINT32 partTable[3] = {0, 1, 2};
                        // Part[2:0] For perfsim floorsweep LTC7
                        m_LtcId = partTable[m_Remainder];
                        break;
                    }
                case 5:
                    {
                        UINT32 partTable[5] = {0, 1, 2, 3, 4};
                        // Part[2:0] For perfsim floorsweep LTC7
                        m_LtcId = partTable[m_Remainder];
                        break;
                    }
                case 7:
                    {
                        // Floorsweeping Table, other part of table in commoncalc_QRO()
                        // Use good LTCs twice, bad LTCs once.
                        UINT32 partTable[7] = {0, 1, 2, 3, 4, 5, 6}; // Part[2:0] For perfsim floorsweep LTC7
                        m_LtcId = partTable[m_Remainder];
                        break;
                    }
                case 9:
                    {
                        // Floorsweeping Table, other part of table in commoncalc_QRO()
                        // Use good LTCs twice, bad LTCs once.
                        // Part[3:0] For perfsim floorsweep LTC9
                        UINT32 partTable[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
                        // Route through good LTC only
                        m_LtcId = partTable[m_Remainder];
                        break;
                    }
                case 11:
                    {
                        // Floorsweeping Table, other part of table in commoncalc_QRO()
                        // Use good LTCs twice, bad LTCs once.
                        // Part[3:0] For perfsim floorsweep LTC11
                        UINT32 partTable[11] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
                        // Route through good LTC only
                        m_LtcId = partTable[m_Remainder];
                        break;
                    }

                default:
                    Printf(Tee::PriError,
                           "Unsupported number of Ltcs %d\n", m_NumLtcs);
                    MASSERT(0);
            } //End switch (m_NumLtcs)
            break;
        case 4:
        case 6:
        case 8:
            {
                switch (m_NumLtcs)
                {
                    case 1:
                    case 2:
                    case 3:
                    case 4:
                    case 5:
                    case 6:
                        m_LtcId = m_Remainder;
                        break;
                    case 8:
                    case 10:
                    case 12:
                        m_LtcId = (m_Remainder << 1) | bit<10> (m_Offset);
                        break;
                    default:
                        Printf(Tee::PriError,
                               "Unsupported number of Ltcs %d\n", m_NumLtcs);
                        MASSERT(0);
                        break;
                }
            }
            break;
        default:
            Printf(Tee::PriError,
                   "Unsupported number of Slices %d\n", m_NumXbarL2Slices);
            MASSERT(0);
    }//End switch (m_NumXbarL2Slices)
    MASSERT(m_LtcId < m_FbpForLtcId.size());
    m_Partition = m_FbpForLtcId[m_LtcId];
    return;
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::ComputeSlice()
{
    switch (m_NumXbarL2Slices)
    {
        case 2:
            if (m_NumLtcs > 12 || m_NumLtcs < 1)
            {
                Printf(Tee::PriError,
                       "Incorrect number of Ltcs %d", m_NumLtcs);
                MASSERT(0);
            }
            else
            {
                CalcCommonSlice2Local();
            }
            break;
        case 3:
            CalcCommonSlice3Local();
            break;
        case 4:
            CalcCommonSlice4Local();
            break;
        default:
            Printf(Tee::PriError, "Unsupported number of slices\n");
            MASSERT(0);
    }//switch (m_NumXbarL2Slices)
    return;
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::CalcCommonSlice4Local()
{
    UINT32 Q = static_cast <UINT32> (m_Quotient);
    UINT32 O = m_Offset;
    // 4 slice
    // O[10:9] are slice[1:0]
    // address into the L2 slice
    m_Slice = bits<10, 9>(O);
    // other testbenches use padr (xbar addr)
    m_Padr = (Q << 2) | bits<8, 7>(O);

    // we need to agree on changing these at the same time in perfsim
    m_L2addrin = (Q << 9) | bits<8, 0>(O);
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::CalcCommonSlice3Local()
{
    UINT32 Q = static_cast <UINT32> (m_Quotient);
    UINT32 R = m_Remainder;
    UINT32 O = m_Offset;
    UINT64 bits_swizzled =
        // randomly ordered upper bit swizzle
        // Truth is, only odd/even bit position matters.
        // Even bit positions add 1 %3.
        // Odd bit positions add 2 %3.
        (bit<21>(Q) << 30) |
        (bit<11>(Q) << 29) |
        (bit<23>(Q) << 28) |
        (bit<9>(Q) << 27) |
        (bit<14>(Q) << 26) |
        (bit<7>(Q) << 25) |
        (bit<16>(Q) << 24) |
        (bit<18>(Q) << 23) |
        (bit<12>(Q) << 22) |
        (bit<25>(Q) << 21) |
        (bit<13>(Q) << 20) |
        (bit<27>(Q) << 19) |
        (bit<20>(Q) << 18) |
        (bit<5>(Q) << 6) |
        (bit<28>(Q) << 17) |
        (bit<19>(Q) << 16) |
        (bit<8>(Q) << 15) |
        (bit<10>(Q) << 14) |
        (bit<24>(Q) << 13) |
        (bit<4>(Q) << 5) |
        (bit<15>(Q) << 12) |
        (bit<26>(Q) << 11) |
        (bit<6>(Q) << 10) |
        (bit<22>(Q) << 9) |
        (bit<17>(Q) << 8) |
        (bit<29>(Q) << 7) | // Addr[16]
        // in order starting here, keep CBC happy in 64KB VM page
        (bit<3>(Q) << 4) |
        (bit<2>(Q) << 3) |
        (bit<1>(Q) << 2) |
        (bit<0>(Q) << 1) |
        (bit<9>(O) << 0);
    bits_swizzled = (Q << 3) | (bits<1, 0>(R) << 1) | bit<9>(O);

    UINT32 slice1_0 = bits_swizzled %3;

    m_Slice = slice1_0;

    // other testbenches use padr (xbar addr)
    // 4KB m_PartitionInterleave

    if (m_PartitionInterleave == 4096)
    {
        m_Padr = (Q << 3) | (bit<11>(O) << 2) | bits<8, 7>(O);
        //Insert O[9] at GMLIT2_OBIT9_LOC_IN_PADR
        m_Padr = ((((m_Padr >> GMLIT2_OBIT9_LOC_IN_PADR) << 1 ) | bit<9>(O))
                << GMLIT2_OBIT9_LOC_IN_PADR) |
            (m_Padr & ((1 << GMLIT2_OBIT9_LOC_IN_PADR) - 1));

        // insert PA[14] = column8 into L2addr
        m_L2addrin = (Q << 10) | (bit<11>(O) << 9) | bits<8, 0>(O);
    }
    else
    { // 2048
        // O[10] is column bit 8, R[0] is subpartition for lower mapping.
        // This is what to use in RTL:
        // m_Padr = (Q << 4) | (bit<0>(R) << 3) | (bit<10>(O) << 2) | bits<8, 7>(O);
        // O[10] is PA[14], which is col[8]
        m_Padr = (Q << 3) | (bit<10>(O) << 2) | bits<8, 7>(O);
        //Insert O[9] at GMLIT2_OBIT9_LOC_IN_PADR
        m_Padr = ((((m_Padr >> GMLIT2_OBIT9_LOC_IN_PADR) << 1 ) | bit<9>(O))
                << GMLIT2_OBIT9_LOC_IN_PADR) |
            (m_Padr & ((1 << GMLIT2_OBIT9_LOC_IN_PADR) - 1));
        // L2 address will really need to stuff R[0] into tag, and recover it for giving to FB.
        // insert PA[14] = column8 into L2addr
        // FB uses R[0] for subpartition.
        m_L2addrin = (Q << 10) | (bit<10>(O) << 9) | bits<8, 0>(O);
    }
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::CalcCommonSlice2Local()
{
    UINT32 Q = static_cast <UINT32> (m_Quotient);
    UINT32 O = m_Offset;

    // other testbenches use padr (xbar addr)
    // need to include O[9] 6 slice mapping, because that bit is not longer in slice[0]

    // insert PA[14] = column8 into padr
    m_Slice = bit<9>(O);  // subpartition = slice[0] in 6 slices
    if (m_PartitionInterleave == 4096)
    {
        m_Padr = (Q << 4) | (bits<11, 10>(O) << 2) | bits<8, 7>(O);
        // insert PA[14] = column8 into L2addr
        // L2 address will really need to stuff R[0] into tag, and recover it for giving to FB.
        // FB uses R[0] for subpartition.
        m_L2addrin = (Q << 10) | (bits<11, 10>(O) << 9) | bits<8, 0>(O);
    }
    else
    { // 2048
        // O[10] is column bit 8, R[0] is subpartition for lower mapping.
        // This is what to use in RTL:
        // m_Padr = (Q << 4) | (bit<0>(R) << 3) | (bit<10>(O) << 2) | bits<8, 7>(O);
        // O[10] is PA[14], which is col[8]
        m_Padr = (Q << 3) | (bit<10>(O) << 2) | bits<8, 7>(O);
        // insert PA[14] = column8 into L2addr
        // L2 address will really need to stuff R[0] into tag, and recover it for giving to FB.
        // FB uses R[0] for subpartition.
        m_L2addrin = (Q << 10) | (bit<10>(O) << 9) | bits<8, 0>(O);
    }
}
//------------------------------------------------------------------------------
bool GMLIT2AddressDecode::CalcRBCS4Parts1Banks4Bool()
{

    // assembles offset bits O[9:2] into m_Col
    m_Col = (bits<2, 0>(m_FbPadr) << 5) | bits<6, 2>(m_L2Offset);

    UINT32 numColBits = ComputeColumnBits();
    MASSERT(numColBits >= 9);
    UINT32 rb=0;

    // m_FbPadr = { Q[n-1:0], O[9:7] }
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
        (bit<0>(tmp) << 0);

    m_Row = rb / (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1)); // includes "column-as-row" bits

    // Swizzle
    // swizzles are tailored to number bank groups.
    const int NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS;

    // No conselwtive two randoms repeat, no 0 element
    UINT32 galois[] =
    {2, 3, 1, 2, 3, 1, 2, 3, 2, 3, 1, 2, 1, 3, 1, 2, 1, 3,
        1, 2, 3, 2, 1, 2, 3, 1, 3, 2, 3, 1};

    // generate swizzle
    UINT64 upper_address = m_Row >> 2;
    UINT32 swizzle= 0;
    for (int i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    // manilwred bank swizzling
    UINT32 swizzle10 =
        ((bit<2>(tmp) ^ bit<1>(m_Row)) << 1) |
        bit<0>(m_Row);

    // Must ^ only against banks to preserve bank group relationships in mapping.
    m_Bank = m_Bank ^ swizzle10 ^ swizzle;

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 9) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 9);
    return true;
}

//------------------------------------------------------------------------------
bool GMLIT2AddressDecode::CalcRBCS4Parts1Banks8Bool()
{
    // assembles offset bits O[9:2] into m_Col
    m_Col = (bits<2, 0>(m_FbPadr) << 5) | bits<6, 2>(m_L2Offset);

    UINT32 numColBits = ComputeColumnBits();
    MASSERT(numColBits >= 9);
    UINT32 rb=0;

    // m_FbPadr = { Q[n-1:0], O[9:7] }
    rb = bits<31, 3>(m_FbPadr);

    // For bank hit 2 blocks apart, row[n-1:18], bank[17:16], col[15], bank[14:13]
    // 2KB pages, col bit is mixed in with the bank bits
    // We only support col_bits >= 9
    UINT32 tmp = rb % (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1));
    m_ExtBank = bit<4>(tmp);

    // stick on 2KB col bit
    m_Col = m_Col | (bit<3>(tmp) << 8);

    m_Bank =
        (bit<0>(tmp) << 2) | // LSBs in top bits for BG
        (bit<1>(tmp) << 1) |
        (bit<2>(tmp) << 0);
    // includes "column-as-row" bits
    m_Row = rb / (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1));

    // Swizzle
    // swizzles are tailored to number bank groups.
    const int NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS;

    // For 4 bank group, R[2:1] cannot be 00, two conselwtive R[2:1] the same
    // not allowed. e.g. 6, 6 not allowed.
    // 6 bit overlap in sliding window on address range
    // No conselwtive two randoms repeat, no 1 element, no two conselwtive with
    // top two bits equal - used for 4 bank groups
    UINT32 galois_4BG[] =
    {2, 5, 7, 3, 4, 2, 7, 2, 4, 3, 6, 4, 7, 2, 6, 5, 3, 5, 2, 4,
        7, 3, 7, 3, 5, 7, 5, 7, 4};

    // generate swizzle
    UINT64 upper_address = m_Row >> 3;
    UINT32 swizzle= 0;

    for (int i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois_4BG[i]);
    }

    // manilwred bank swizzling
    UINT32 swizzle20 =
        (bit<0>(m_Row) << 2) |
        ((bit<1>(m_Row) ^ bit<0>(m_Bank)) << 1) |
        (bit<2>(m_Row));

     // Must ^ only against banks to preserve bank group relationships in mapping.
     m_Bank = m_Bank ^ swizzle20 ^ swizzle;
     // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
     m_Col = ((m_Row << 9) | m_Col) & ((1 << numColBits) - 1);
     m_Row = m_Row >> (numColBits - 9);
     return true;
 }

//------------------------------------------------------------------------------
bool GMLIT2AddressDecode::CalcRBCS4Parts1Banks16Bool()
{
    // assembles offset bits O[9:2] into m_Col
    m_Col = (bits<2, 0>(m_FbPadr) << 5) | bits<6, 2>(m_L2Offset);

    UINT32 numColBits = ComputeColumnBits();
    MASSERT(numColBits >= 9);
    UINT32 rb=0;

    // m_FbPadr = { Q[n-1:0], O[9:7] }
    rb = bits<31, 3>(m_FbPadr);

    // For bank hit 2 blocks apart, row[n-1:18], bank[17:16], col[15], bank[14:13]
    // 2KB pages, col bit is mixed in with the bank bits
    // We only support col_bits >= 9
    UINT32 tmp = rb % (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1));
    m_ExtBank = bit<5>(tmp);

    // stick on 2KB col bit
    m_Col = m_Col | (bit<3>(tmp) << 8);

    m_Bank =
        (bit<1>(tmp) << 3) | // LSBs in top bits for BG
        (bit<0>(tmp) << 2) | //
        (bit<4>(tmp) << 1) |
        (bit<2>(tmp) << 0);

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

    // Swizzle
    // swizzles are tailored to number bank groups.
    const int NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS;
    UINT32 galois[] =
    {1, 2, 4, 8, 9, 11, 15, 7, 14, 5, 10, 13, 3, 6, 12, 1, 2, 4, 8
        , 9, 11, 15, 7, 14, 5, 10, 13, 3};

    // generate swizzle
    UINT64 upper_address = m_Row >> 3;
    UINT32 swizzle= 0;
    for (int i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    // manilwred bank swizzling
    UINT32 swizzle32 = bitsr<0, 1>(m_Bank) ^ bitsr<0, 1>(m_Row) ^ bits<3, 2>(swizzle);
    UINT32 swizzle10 = bit<2>(m_Row) ^ bits<1, 0>(swizzle);

    // Must ^ only against banks to preserve bank group relationships in mapping.
    m_Bank = bits<1, 0>((m_Bank) ^ swizzle10)|((bits<3, 2>(m_Bank) ^ swizzle32) << 2);

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 9) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 9);
    return true;
}

//------------------------------------------------------------------------------
bool GMLIT2AddressDecode::CalcRBCS4Parts2Banks4Bool()
{

    // assembles offset bits O[9:2] into m_Col
    m_Col = (bits<2, 0>(m_FbPadr) << 5) | bits<6, 2>(m_L2Offset);

    UINT32 numColBits = ComputeColumnBits();
    MASSERT(numColBits >= 9);
    UINT32 rb=0;

    // m_FbPadr = { Q[n-1:0], O[9:7] }
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
        (bit<0>(tmp) << 0);

    // includes "column-as-row" bits
    m_Row = rb / (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1));

    // Swizzle
    // swizzles are tailored to number bank groups.
    const int NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS;

    // No conselwtive two randoms repeat, no 0 element
    UINT32 galois[] =
    {3, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3, 1, 2, 1, 3, 1, 2, 1, 3,
        1, 2, 3, 2, 1, 2, 3, 1, 3, 2, 3};

    // generate swizzle
    UINT64 upper_address = m_Row >> 3;
    UINT32 swizzle= 0;
    for (int i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    // manilwred bank swizzling
    UINT32 swizzle10 =
        ((bit<1>(m_Row)) << 1) |
        (bit<0>(m_Row) ^ bit<1>(m_Row) ^ bit<2>(m_Row));

    // Must ^ only against banks to preserve bank group relationships in mapping.
    m_Bank = m_Bank ^ swizzle10 ^ swizzle;

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 9) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 9);
    return true;
}

//------------------------------------------------------------------------------
bool GMLIT2AddressDecode::CalcRBCS4Parts2Banks8Bool()
{
    // assembles offset bits O[9:2] into m_Col
    m_Col = (bits<2, 0>(m_FbPadr) << 5) | bits<6, 2>(m_L2Offset);

    UINT32 numColBits = ComputeColumnBits();
    MASSERT(numColBits >= 9);
    UINT32 rb=0;

    // m_FbPadr = { Q[n-1:0], O[9:7] }
    rb = bits<31, 3>(m_FbPadr);

    // For bank hit 2 blocks apart, row[n-1:18], bank[17:16], col[15], bank[14:13]
    // 2KB pages, col bit is mixed in with the bank bits
    // We only support col_bits >= 9
    UINT32 tmp = rb % (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1));
    m_ExtBank = bit<4>(tmp);

    // stick on 2KB col bit
    m_Col = m_Col | (bit<3>(tmp) << 8);

    m_Bank =
        (bit<0>(tmp) << 2) | // LSBs in top bits for BG
        (bit<1>(tmp) << 1) |
        (bit<2>(tmp) << 0);

    // includes "column-as-row" bits
    m_Row = rb / (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1));

    // Swizzle
    // swizzles are tailored to number bank groups.
    const int NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS;

    // For 4 bank group, R[2:1] cannot be 00, two conselwtive R[2:1] the same
    // not allowed. e.g. 6, 6 not allowed.
    // 6 bit overlap in sliding window on address range
    // No conselwtive two randoms repeat, no 1 element, no two conselwtive with
    // top two bits equal - used for 4 bank groups
    UINT32 galois_4BG[] =
    {2, 5, 7, 3, 4, 2, 7, 2, 4, 3, 6, 4, 7, 2, 6, 5, 3,
        5, 2, 4, 7, 3, 7, 3, 5, 7, 5, 7, 4};

    // generate swizzle
    UINT64 upper_address = m_Row >> 3;
    UINT32 swizzle= 0;

    for (int i = 0; i < NumSwizzleBits; ++i)
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
    m_Col = ((m_Row << 9) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 9);
    return true;
}

//------------------------------------------------------------------------------
bool GMLIT2AddressDecode::CalcRBCS4Parts2Banks16Bool()
{
    // assembles offset bits O[9:2] into m_Col
    m_Col = (bits<2, 0>(m_FbPadr) << 5) | bits<6, 2>(m_L2Offset);

    UINT32 numColBits = ComputeColumnBits();
    MASSERT(numColBits >= 9);
    UINT32 rb=0;

    // m_FbPadr = { Q[n-1:0], O[9:7] }
    rb = bits<31, 3>(m_FbPadr);

    // For bank hit 2 blocks apart, row[n-1:18], bank[17:16], col[15], bank[14:13]
    // 2KB pages, col bit is mixed in with the bank bits
    // We only support col_bits >= 9
    UINT32 tmp = rb % (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1));
    m_ExtBank = bit<5>(tmp);

    // stick on 2KB col bit
    m_Col = m_Col | (bit<3>(tmp) << 8);

    m_Bank =
        (bit<1>(tmp) << 3) | // LSBs in top bits for BG
        (bit<0>(tmp) << 2) | //
        (bit<4>(tmp) << 1) |
        (bit<2>(tmp) << 0);

    // this needs to be verified
    if (m_8BgSwizzle)
{
        m_Bank =
            (bit<3>(m_Bank) << 3) |
            (bit<2>(m_Bank) << 2) |
            (bit<0>(m_Bank) << 1) |
            (bit<1>(m_Bank) << 0);
    }
    m_Row = rb / (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1)); // includes "column-as-row" bits

    // Swizzle
    // swizzles are tailored to number bank groups.
    const int NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS;
    UINT32 galois[] = {1, 2, 4, 8, 9, 11, 15, 7, 14, 5, 10, 13, 3, 6, 12, 1, 2, 4, 8, 9, 11, 15, 7, 14, 5, 10, 13, 3};

    // generate swizzle
    UINT64 upper_address = m_Row >> 3;
    UINT32 swizzle= 0;
    for (int i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    // manilwred bank swizzling
    UINT32 swizzle30 = ((bit<0>(m_Bank) ^ bit<1>(m_Bank) ^ bit<1>(m_Row) ^ bit<2>(m_Row)) << 3) |
        ((bit<0>(m_Bank) ^ bit<0>(m_Row) ^ bit<1>(m_Row)) << 2) |
        (bit<2>(m_Row) << 1) |
        (bit<0>(m_Row) ^ bit<2>(m_Row));

    // Must ^ only against banks to preserve bank group relationships in mapping.
    m_Bank = bits<3, 0>(m_Bank) ^ swizzle ^ swizzle30;

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 9) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 9);
    return true;
}

//------------------------------------------------------------------------------
bool GMLIT2AddressDecode::CalcRBCS4Parts4Banks8Bool()
{
    // assembles offset bits O[9:2] into m_Col
    m_Col = (bits<2, 0>(m_FbPadr) << 5) | bits<6, 2>(m_L2Offset);

    UINT32 numColBits = ComputeColumnBits();
    MASSERT(numColBits >= 9);
    UINT32 rb=0;

    // m_FbPadr = { Q[n-1:0], O[9:7] }
    rb = bits<31, 3>(m_FbPadr);

    // For bank hit 2 blocks apart, row[n-1:18], bank[17:16], col[15], bank[14:13]
    // 2KB pages, col bit is mixed in with the bank bits
    // We only support col_bits >= 9
    UINT32 tmp = rb % (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1));
    m_ExtBank = bit<4>(tmp);

    // stick on 2KB col bit
    m_Col = m_Col | (bit<3>(tmp) << 8);

    m_Bank =
        (bit<0>(tmp) << 2) | // LSBs in top bits for BG
        (bit<1>(tmp) << 1) |
        (bit<2>(tmp) << 0);

    m_Row = rb / (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1)); // includes "column-as-row" bits

    // Swizzle
    // swizzles are tailored to number bank groups.
    const int NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS;

    // For 4 bank group, R[2:1] cannot be 00, two conselwtive R[2:1] the same
    // not allowed. e.g. 6, 6 not allowed.
    // 6 bit overlap in sliding window on address range
    // No conselwtive two randoms repeat, no 1 element, no two conselwtive
    // with top two bits equal - used for 4 bank groups
    UINT32 galois_4BG[] =
    {2, 5, 7, 3, 4, 2, 7, 2, 4, 3, 6, 4, 7, 2, 6, 5, 3, 5, 2, 4,
        7, 3, 7, 3, 5, 7, 5, 7, 4};

    // generate swizzle
    UINT64 upper_address = m_Row >> 0;
    UINT32 swizzle= 0;

    for (int i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois_4BG[i]);
    }

    // Must ^ only against banks to preserve bank group relationships in mapping.
    m_Bank = m_Bank ^ swizzle;
    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 9) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 9);
    return true;
}

//------------------------------------------------------------------------------
bool GMLIT2AddressDecode::CalcRBCS4Parts4Banks16Bool()
{
    // assembles offset bits O[9:2] into m_Col
    m_Col = (bits<2, 0>(m_FbPadr) << 5) | bits<6, 2>(m_L2Offset);

    UINT32 numColBits = ComputeColumnBits();
    MASSERT(numColBits >= 9);
    UINT32 rb=0;

    // m_FbPadr = { Q[n-1:0], O[9:7] }
    rb = bits<31, 3>(m_FbPadr);

    // For bank hit 2 blocks apart, row[n-1:18], bank[17:16], col[15], bank[14:13]
    // 2KB pages, col bit is mixed in with the bank bits
    // We only support col_bits >= 9
    UINT32 tmp = rb % (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1));
    m_ExtBank = bit<5>(tmp);

    // stick on 2KB col bit
    m_Col = m_Col | (bit<1>(tmp) << 8);

    m_Bank =
        (bit<2>(tmp) << 3) | // LSBs in top bits for BG
        (bit<0>(tmp) << 2) | //
        (bit<4>(tmp) << 1) |
        (bit<3>(tmp) << 0);

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

    // Swizzle
    // swizzles are tailored to number bank groups.
    const int NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS;
    UINT32 galois[] =
    {1, 2, 4, 8, 9, 11, 15, 7, 14, 5, 10, 13, 3, 6, 12, 1, 2, 4, 8,
        9, 11, 15, 7, 14, 5, 10, 13, 3};

    // generate swizzle
    UINT64 upper_address = m_Row >> 0;
    UINT32 swizzle= 0;
    for (int i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    // Must ^ only against banks to preserve bank group relationships in mapping.
    m_Bank = bits<3, 0>(m_Bank) ^ swizzle;

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 9) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 9);
    return true;
}

//------------------------------------------------------------------------------
bool GMLIT2AddressDecode::CalcRBCS4Parts8Banks8Bool()
{
    // assembles offset bits O[9:2] into m_Col
    m_Col = (bits<3, 0>(m_FbPadr) << 5) | bits<6, 2>(m_L2Offset);

    UINT32 numColBits = ComputeColumnBits();
    MASSERT(numColBits >= 9);
    UINT32 rb=0;

    // m_FbPadr = { Q[n-1:0], O[9:7] }
    rb = bits<31, 4>(m_FbPadr);

    // For bank hit 2 blocks apart, row[n-1:18], bank[17:16], col[15], bank[14:13]
    // 2KB pages, col bit is mixed in with the bank bits
    // We only support col_bits >= 9
    UINT32 tmp = rb % (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1));
    m_ExtBank = bit<3>(tmp);

    // stick on 2KB col bit
    m_Col = m_Col | (bit<3>(tmp) << 8);

    // Want to step through bank groups.
    // 4 bank groups: BG[1:0] = bank[2:1]
    // 2 bank groups: BG[0] = bank[2]
    m_Bank =
        (bit<0>(tmp) << 2) | // LSBs in top bits for BG
        (bit<1>(tmp) << 1) |
        (bit<2>(tmp) << 0);

    m_Row = rb / (m_NumDRAMBanks << (m_NumExtDRAMBanks)); // includes "column-as-row" bits

    // Swizzle
    // swizzles are tailored to number bank groups.
    const int NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS;

    UINT32 galois_4BG[] =
    {2, 5, 7, 3, 4, 2, 7, 2, 4, 3, 6, 4, 7, 2, 6, 5, 3, 5, 2, 4, 7,
        3, 7, 3, 5, 7, 5, 7, 4};

    // generate swizzle
    UINT64 upper_address = m_Row >> 0;
    UINT32 swizzle= 0;

    for (int i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois_4BG[i]);
    }

    // Must ^ only against banks to preserve bank group relationships in mapping.
    m_Bank = m_Bank ^ swizzle;
    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 9) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 9);
    return true;
}

//------------------------------------------------------------------------------
bool GMLIT2AddressDecode::CalcRBCS4Parts8Banks16Bool()
{
    // assembles offset bits O[9:2] into m_Col
    m_Col = (bits<2, 0>(m_FbPadr) << 5) | bits<6, 2>(m_L2Offset);

    UINT32 numColBits = ComputeColumnBits();
    MASSERT(numColBits >= 9);
    UINT32 rb=0;

    // m_FbPadr = { Q[n-1:0], O[9:7] }
    rb = bits<31, 4>(m_FbPadr);

    // For bank hit 2 blocks apart, row[n-1:18], bank[17:16], col[15], bank[14:13]
    // 2KB pages, col bit is mixed in with the bank bits
    // We only support col_bits >= 9
    UINT32 tmp = rb % (m_NumDRAMBanks << (m_NumExtDRAMBanks + 1));
    m_ExtBank = bit<4>(tmp);

    // stick on 2KB col bit
    m_Col = m_Col | (bit<3>(tmp) << 8);

    m_Bank =
        (bit<1>(tmp) << 3) | // LSBs in top bits for BG
        (bit<0>(tmp) << 2) | //
        (bit<3>(tmp) << 1) |
        (bit<2>(tmp) << 0);

    // includes "column-as-row" bits
    m_Row = rb / (m_NumDRAMBanks << (m_NumExtDRAMBanks));

    // Swizzle
    // swizzles are tailored to number bank groups.
    const int NumSwizzleBits = NUM_GALOIS_BANK_SWIZZLE_BITS;
    UINT32 galois[] = {1, 2, 4, 8, 9, 11, 15, 7, 14, 5, 10, 13, 3, 6, 12, 1, 2,
        4, 8, 9, 11, 15, 7, 14, 5, 10, 13, 3};

    // generate swizzle
    UINT64 upper_address = m_Row >> 0;
    UINT32 swizzle= 0;
    for (int i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    // Must ^ only against banks to preserve bank group relationships in mapping.
    m_Bank = bits<3, 0>(m_Bank) ^ swizzle;

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 9) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 9);
    return true;
}

//------------------------------------------------------------------------------
UINT32 GMLIT2AddressDecode :: ComputeEccPadr()
{
    // We must group all 2KB of column bits together.
    // Do the 16/15 multiply, then put them back.
    // Some of the row bits are actually banks bits.
    UINT64 compPadr, multPadr;

    // fbPadr is a byte address
    compPadr = m_FbPadr;
    multPadr = (compPadr / m_EccLinesPerPage)
                *(m_LtcLinesPerPage)
                |(compPadr % m_EccLinesPerPage);
    return static_cast <UINT32> (multPadr); //Safe for GM20x
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode :: ComputeFbPadr()
{
    //Turned off here because this is turned of in gmlit2 fmodel
    m_DRAMBankSwizzle = false;
    if (m_NumXbarL2Slices == 6 || m_NumXbarL2Slices == 3)
    {
        m_SubPart = bit<0>(m_Slice);
    }
    else if (m_NumXbarL2Slices ==2)
    {
        UINT32 lFbp = m_FbpForLtcId[m_LtcId];
        if (m_LtcMaskForLFbp[lFbp] != s_MaxL2MaskPerFbp)
        {
            //This means this is a floorswept part
            // m_fb_padr is 128b aligned and has no slice bit (for the 2-slice cases),
            // so subtract numSliceBits and subtract 7 bits (128b)
            // column bits are DWORD aligned, so add 2 bits to undo the alignment
            // Figure out where top of the row bits are.
            UINT32 numBankBits = log2i(m_NumDRAMBanks);
            // Amap equivalent to m_number_dram_banks_partition[m_Partition]);
            UINT32 numExtbankBits = m_NumExtDRAMBanks? 1 : 0;
            UINT32 numSliceBits = log2i(m_NumXbarL2Slices);
            UINT32 numColBits = ComputeColumnBits();
            MASSERT(numColBits >= 9);
            UINT32 numFBPadrBits = (numColBits + 2) +  numBankBits
                + numExtbankBits + m_NumRowBitsPerLtc[m_LtcId] - numSliceBits - 7;
            m_SubPart = 1 & (m_FbPadr >> numFBPadrBits);
        }
        else
        {
            //For 2 subparts
            //m_SubPart = bit<0>(m_LtcId); from AMAP looks wrong
            //lt = m_LtcId >> 1; It == lFbp
            if (!m_LtcId)
            {
                //First Ltc
                m_SubPart = 0;
            }
            else if (m_FbpForLtcId[m_LtcId - 1] == lFbp)
            {
                //Ltcn in FBPx: ltcm:ltcn.
                m_SubPart = 1;
            }
            else if (m_LtcId + 1 == m_FbpForLtcId.size())
            {
                //last ltc in last Fbp. This FBP has all the ltc enabled
                    m_SubPart = 0;
            }
            else
            {
                //Ltcm in FBPx: ltcm:ltcn
                // This Ltc is neither first nor last one.
                // The lower ltcid belongs to a different Fbp
                m_SubPart = 0;
            }
        }
    }
    else
    {

        m_SubPart = bit<1> (m_Slice);
    }
    // 64b case:
    // Inject slice bit into the padr field for case where 2 slices feed 1 subpa
    if (m_NumXbarL2Slices !=6 && m_NumXbarL2Slices !=3)
    {
        m_FbPadr = ((m_FbPadr & ~3) << 1) |
            (bit<9>(m_Offset) << 2) | (bits<1, 0>(m_FbPadr));
    }
    if (m_DRAMBankSwizzle)
    {
        m_FbPadr = (m_FbPadr & ~0x38) |
            ((bits<2, 0>( bits<5, 3>(m_FbPadr) - 3*bits<8, 6>(m_FbPadr))) << 3);
    }
    if (m_ECCOn)
    {
        m_FbPadr = ComputeEccPadr();
    }
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::FindAddressRanges()
{
    //Loop through all active FBPs and keep track of the size of the smallest FBP based
    // on dram row bits
    UINT64 minSize = ~0;
    bool floorsweepingActive = false;

    //For GM20x, fmodel takes argument for row bits to determine if fbp is floorswept or not
    //As per decoder ring ltc floorsweeping is active in GM20x if!(numLTCs == 4 || numLTCs == 8 || numLTCs == 12);
    for (UINT32 fbp = 0; fbp < m_LtcMaskForLFbp.size(); ++fbp)
    {
        if (m_LtcMaskForLFbp[fbp] != s_MaxL2MaskPerFbp)
        {
            floorsweepingActive = true;
            break;
        }
    }

    bool doubleDensityActive = false;
    for (UINT32 ltc = 0; ltc < m_NumLtcs; ++ltc)
    {
        UINT64 lwrSize = m_NumRowBitsPerLtc[ltc];
        if (lwrSize < minSize)
        {
            minSize = lwrSize;
        }

        if (ltc > 0)
        {
            doubleDensityActive |= (m_NumRowBitsPerLtc[ltc] != m_NumRowBitsPerLtc[ltc-1]);
        }

    }
    minSize += log2i(m_NumDRAMBanks);
    minSize += log2i(m_DRAMPageSize);
    minSize += m_NumExtDRAMBanks? 1 : 0;
    MASSERT(minSize < 64);

    // bits to size
    minSize = UINT64(1) << minSize; //in Bytes

    // From fmodel code: //hw/lwgpu/clib/tools/utilities/AddressMappingUtilFModel/amap.cpp
    // "ECC http://lwbugs/1416451
    //  with double density, multiply everything by 15/16
    //  with floorsweeping, lower memory size is 15/16, upper memory starts at POT (full partition size * m_NumUpperLtcs) but is sized at 15/16
    //  with double density AND floorsweeping, lower memory size is 15/16, upper memory starts at (full partition size * m_NumUpperLtcs) but is sized at 14/16 "
    UINT64 minSizeWithEcc = (minSize / 16) * 15;
    UINT64 minSizeWithEcc14 = (minSize / 16) * 14;
    m_LowerStart = 0;
    if (!m_ECCOn)
    {
        m_LowerEnd = m_NumLtcs * minSize;
    }
    else
    {
        m_LowerEnd = m_NumLtcs * minSizeWithEcc;
    }
    //Upper mem start address = num_upper_partitions * (lower memory size per ltc) with bit 36 (defined in RefMan) additionally set in physical address
    m_UpperStart = UINT64(1) << s_Gm20xUpperMemoryBit;
    if (!m_ECCOn)
    {
        m_UpperStart += m_NumUpperLtcs * minSize;
        m_UpperEnd = m_UpperStart + (m_NumUpperLtcs * minSize);
    }
    else
    {
        if (doubleDensityActive && !floorsweepingActive)
        {
            m_UpperStart += m_NumUpperLtcs * minSizeWithEcc;
            m_UpperEnd = m_UpperStart + (m_NumUpperLtcs * minSizeWithEcc);
        }
        else if (!doubleDensityActive && floorsweepingActive)
        {
            m_UpperStart += m_NumUpperLtcs * minSize;
            m_UpperEnd = m_UpperStart + (m_NumUpperLtcs * minSizeWithEcc);
        }
        else
        { // doubleDensityActive && floorsweepingActive
            m_UpperStart += m_NumUpperLtcs * minSize;
            m_UpperEnd = m_UpperStart + (m_NumUpperLtcs * minSizeWithEcc14);
        }
    }
}

//------------------------------------------------------------------------------
RC GMLIT2AddressDecode::DecodeAddress
(
    FbDecode* pDecode,
    UINT64 physicalFbOffset,
    UINT32 pteKind,
    UINT32 pageSize
)
{
    const FrameBuffer *pFb = m_pGpuSubdevice->GetFB();

    //Validate if address is in FbOffset range
    if (physicalFbOffset >= m_LowerStart && physicalFbOffset < m_LowerEnd)
    {
        Printf(Tee::PriLow, "fboffset 0x%llx is in lower memory range\n", physicalFbOffset);
    }
    else if (physicalFbOffset >= m_UpperStart && physicalFbOffset < m_UpperEnd)
    {
        Printf(Tee::PriLow, "fboffset 0x%llx is in higher memory range\n", physicalFbOffset);
    }
    else
    {
        Printf(Tee::PriError,
               "Physical offset 0x%llx is not in address range"
               " Lower 0x%llx - 0x%llx or higher 0x%llx - 0x%llx\n",
               physicalFbOffset, m_LowerStart, m_LowerEnd,
               m_UpperStart, m_UpperEnd);
        MASSERT(0);
        return RC::BAD_PARAMETER;
    }

    m_PartitionInterleave = ComputePI();

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
            Printf(Tee::PriError,
                   "DecodeAddress: Unsupported PTE kind - 0x%02x!\n",
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
    pDecode->xbarRawAddr  = m_Padr;

    if (m_PrintDecodeDetails)
    {
        PrintDecodeDetails(pteKind, pageSize);
    }
    return OK;
}

//------------------------------------------------------------------------------
/*virtual*/ void GMLIT2AddressDecode::PrintDecodeDetails(UINT32 PteKind, UINT32 PageSize)
{
    Printf(Tee::PriNormal, "PA = 0x%x Kind = 0x%x PageSize=%uK\n"
            , unsigned(m_PhysAddr), PteKind, PageSize);
    Printf(Tee::PriNormal, "MMU  Q:R:O:PAKS = 0x%llx:0x%x:0x%x:0x%llx\n",
            m_Quotient, m_Remainder, m_Offset, m_PAKS);
    Printf(Tee::PriNormal, "XBAR Slice:Partition:Padr:LtcId:P.I. ="
           " %u:%u:0x%x:%u:0x%x\n",
            m_Slice, m_Partition,
            m_Padr, m_LtcId, m_PartitionInterleave);
    Printf(Tee::PriNormal, "L2   Tag:Set:Bank:Sector:Offset = 0x%llx:0x%x:0x%x:0x%x:0x%x\n",
            m_L2Tag, m_L2Set, m_L2Bank, m_L2Sector, m_L2Offset);
    Printf(Tee::PriNormal, "DRAM Extbank:R:B:S:C = %u:0x%x:0x%x:%u:0x%x\n",
            m_ExtBank, m_Row, m_Bank, m_SubPart, m_Col);
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::ComputeBlockLinearPAKS()
{
    switch (m_PageSize)
    {
        case 4:
        case 64:
        case 128:
            {
                switch (m_NumLtcs)
                {
                    case 1:
                        m_PAKS = CommonCalcPAKSParts1(m_PhysAddr);
                        break;
                    case 2:
                        m_PAKS = CommonCalcPAKSParts2(m_PhysAddr);
                        break;
                    case 3:
                        m_PAKS = CommonCalcPAKSParts3(m_PhysAddr);
                        break;
                    case 4:
                        m_PAKS = CommonCalcPAKSParts4(m_PhysAddr);
                        break;
                    case 5:
                        m_PAKS = CommonCalcPAKSParts5(m_PhysAddr);
                        break;
                    case 6:
                        m_PAKS = CommonCalcPAKSParts6(m_PhysAddr);
                        break;
                    case 7:
                        m_PAKS = CommonCalcPAKSParts7(m_PhysAddr);
                        break;
                    case 8:
                        m_PAKS = CommonCalcPAKSParts8(m_PhysAddr);
                        break;
                    case 9:
                        m_PAKS = CommonCalcPAKSParts9(m_PhysAddr);
                        break;
                    case 10:
                        m_PAKS = CommonCalcPAKSParts10(m_PhysAddr);
                        break;
                    case 11:
                        m_PAKS = CommonCalcPAKSParts11(m_PhysAddr);
                        break;
                    case 12:
                        m_PAKS = CommonCalcPAKSParts12(m_PhysAddr);
                        break;
                } //switch (m_NumLtcs)
            }
            break;
        default:
            Printf(Tee::PriError, "Unsupported page size %dK\n", m_PageSize);
            MASSERT(0);
    } //switch (m_PageSize)
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::ComputePAKS()
{
    m_PAKS = 0;

    switch (m_PteKind)
    {
        case LW_MMU_PTE_KIND_PITCH_NO_SWIZZLE:
        case LW_MMU_PTE_KIND_PITCH:
            switch (m_PageSize)
            {
                case 4:
                case 64:
                case 128:
                    switch (m_NumLtcs)
                    {
                        case 1:
                            CalcPAKSPitchLinearPS64KParts1();
                            break;
                        case 2:
                            CalcPAKSPitchLinearPS64KParts2();
                            break;
                        case 3:
                            CalcPAKSPitchLinearPS64KParts3();
                            break;
                        case 4:
                            CalcPAKSPitchLinearPS64KParts4();
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
                        case 9:
                            CalcPAKSPitchLinearPS64KParts9();
                            break;
                        case 10:
                            CalcPAKSPitchLinearPS64KParts10();
                            break;
                        case 11:
                            CalcPAKSPitchLinearPS64KParts11();
                            break;
                        case 12:
                            CalcPAKSPitchLinearPS64KParts12();
                            break;
                        default:
                            Printf(Tee::PriError, "Page size 4K "
                                " Unsupported number of Ltcs - %u!\n",
                                m_NumLtcs);
                            MASSERT(!"Unsupported NumLtcs!");
                    } //switch (m_NumLtcs)
                    break;
                default:
                    Printf(Tee::PriError,
                           "DecodeAddress: Unsupported page size - 0x%x!\n",
                            m_PageSize);
                    MASSERT(!"Unsupported page size!");
                    break;
            }//switch (m_PageSize)
            break;
        // All block linear kinds
        case LW_MMU_PTE_KIND_GENERIC_16BX2:
            ComputeBlockLinearPAKS();
            break;
        default:
            MASSERT(!"Unspported pteKind");
            return;
    }
    return;
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::CalcPAKSPitchLinearPS64KParts1()
{
    UINT64 lower = CommonCalcPAKSPitchLinearParts1Lower();
    // swizzle common bits 12 and above
    m_PAKS = CommonCalcPAKSParts1(lower);
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::CalcPAKSPitchLinearPS64KParts2()
{
    UINT64 lower = CommonCalcPAKSPitchLinearParts1Lower();
    // swizzle common bits 12 and above
    m_PAKS = CommonCalcPAKSParts2(lower);
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::CalcPAKSPitchLinearPS64KParts3()
{
    UINT64 lower = CommonCalcPAKSPitchLinearParts1Lower();
    // swizzle common bits 12 and above
    m_PAKS = CommonCalcPAKSParts3(lower);
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::CalcPAKSPitchLinearPS64KParts4()
{
    UINT64 lower = CommonCalcPAKSPitchLinearParts1Lower();
    // swizzle common bits 12 and above
    m_PAKS = CommonCalcPAKSParts4(lower);
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::CalcPAKSPitchLinearPS64KParts5()
{
    UINT64 lower = CommonCalcPAKSPitchLinearParts1Lower();
    // swizzle common bits 12 and above
    m_PAKS = CommonCalcPAKSParts5(lower);
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::CalcPAKSPitchLinearPS64KParts6()
{
    UINT64 lower = CommonCalcPAKSPitchLinearParts1Lower();
    // swizzle common bits 12 and above
    m_PAKS = CommonCalcPAKSParts6(lower);
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::CalcPAKSPitchLinearPS64KParts7()
{
    UINT64 lower = CommonCalcPAKSPitchLinearParts1Lower();
    // swizzle common bits 12 and above
    m_PAKS = CommonCalcPAKSParts7(lower);
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::CalcPAKSPitchLinearPS64KParts8()
{
    UINT64 lower = CommonCalcPAKSPitchLinearParts1Lower();
    // swizzle common bits 12 and above
    m_PAKS = CommonCalcPAKSParts8(lower);
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::CalcPAKSPitchLinearPS64KParts9()
{
    UINT64 lower = CommonCalcPAKSPitchLinearParts1Lower();
    // swizzle common bits 12 and above
    m_PAKS = CommonCalcPAKSParts9(lower);
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::CalcPAKSPitchLinearPS64KParts10()
{
    UINT64 lower = CommonCalcPAKSPitchLinearParts1Lower();
    // swizzle common bits 12 and above
    m_PAKS = CommonCalcPAKSParts10(lower);
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::CalcPAKSPitchLinearPS64KParts11()
{
    UINT64 lower = CommonCalcPAKSPitchLinearParts1Lower();
    // swizzle common bits 12 and above
    m_PAKS = CommonCalcPAKSParts11(lower);
}

//------------------------------------------------------------------------------
void GMLIT2AddressDecode::CalcPAKSPitchLinearPS64KParts12()
{
    UINT64 lower = CommonCalcPAKSPitchLinearParts1Lower();
    // swizzle common bits 12 and above
    m_PAKS = CommonCalcPAKSParts12(lower);
}

//------------------------------------------------------------------------------
//Works for parts1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12
UINT64 GMLIT2AddressDecode::CommonCalcPAKSPitchLinearParts1Lower()
{
    UINT64 temp =
        (bits<PA_MSB, 10>(m_PhysAddr) << 10) |
        (bit<8>(m_PhysAddr) << 9) |
        (bit<9>(m_PhysAddr) << 8) |
        bits<7, 0>(m_PhysAddr);
    return temp;
}

//------------------------------------------------------------------------------
UINT64 GMLIT2AddressDecode::CommonCalcPAKSParts1(UINT64 lower)
{
    // Galois Swizzle
    const INT32 NumSwizzleBits = 30;
    //7 bit overlap in sliding window on address range, no conselwtive two randoms repeat
    UINT32 galois[NumSwizzleBits] =
       {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

    UINT64 upper_address = lower >> 16;
    // generate swizzle
    UINT32 swizzle= 0;
    for (int i=0; i<NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    UINT64 temp =
        (bits<PA_MSB, 15>(lower) << 15) |
        (bits<13, 10>(lower) << 11) |
        // 2K column bit
        (bit<14>(lower) << 10) |
        // slice0
        (((m_NumXbarL2Slices == 6 || m_NumXbarL2Slices == 3) ? bit<9>(lower):
          bit<9>(lower) ^ bit<13>(lower) ^ bit<0>(swizzle)) << 9) |
        bits<8, 0>(lower);
    return temp;
}

//------------------------------------------------------------------------------
UINT64 GMLIT2AddressDecode::CommonCalcPAKSParts2(UINT64 lower)
{
    // Galois Swizzle
    const INT32 NumSwizzleBits = 30;
    // Want to swizzle 3 bit wide field.
    // { part[0], slice[1:0] }
    //  gen_galois 0 1 1 30 2 1 1
    UINT32 galois[NumSwizzleBits] =
       {2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3,
        2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3};

    UINT64 upper_address = lower >> 16;
    // start at bit 17 because 128KB is the minimum block that steps across all drams horizontally
    // generate swizzle
    UINT32 swizzle= 0;
    for (int i=0; i<NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    UINT64 temp =
        (bits<PA_MSB, 15>(lower) << 15) |
        (bits<13, 11>(lower) << 12) |
        // subpartition
        ((bit<10>(lower) ^ bit<1>(swizzle) ^ bit<13>(lower) ^ bit<15>(lower)) << 11) |
        // 2K column bit
        (bit<14>(lower) << 10) |
        // slice0
        (((m_NumXbarL2Slices == 6 || m_NumXbarL2Slices == 3) ? bit<9>(lower):
          bit<9>(lower) ^ bit<14>(lower) ^ bit<0>(swizzle)) << 9) |
        bits<8, 0>(lower);
    return temp;
}

//------------------------------------------------------------------------------
// 4KB m_partitionInterleave for 2T SDDR4 pairing
UINT64 GMLIT2AddressDecode::CommonCalcPAKSParts3(UINT64 lower)
{
    // Galois Swizzle
    const INT32 NumSwizzleBits = 30;
    // Want to swizzle 3 bit wide field.
    // { part[0], slice[1:0] }
    //  gen_galois 0 1 1 30 2 1 1
    UINT32 galois[NumSwizzleBits] =
    {7, 7, 6, 7, 6, 7, 7, 6, 7, 6, 7, 6, 7, 6, 7,
        7, 6, 7, 6, 7, 6, 7, 6, 7, 7, 6, 7, 6, 7, 6};

    UINT64 upper_address = lower >> 18;
    // start at bit 17 because 128KB is the minimum block that steps across all drams horizontally
    // generate swizzle
    UINT32 swizzle= 0;
    for (int i=0; i<NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    // Swizzling above 4KB must be identical for blocklinear, pitch.
    // This means bit 12 and up must be swizzled identically, manilwred and Galois.
    UINT64 temp =
        (bits<PA_MSB, 15>(lower) << 15) |
        ((bits<13, 12>(lower)) << 13) |
        // part
        ((bit<11>(lower) ^ bit<2>(swizzle)) << 12) |
        // supb
        ((bit<10>(lower) ^ bit<1>(swizzle)) << 11) |
        (bit<14>(lower) << 10) |
        // slice0
        (((m_NumXbarL2Slices == 6 || m_NumXbarL2Slices == 3) ? bit<9>(lower):
          bit<9>(lower) ^ bit<14>(lower) ^ bit<0>(swizzle)) << 9) |
        bits<8, 0>(lower);
    return temp;
}

//------------------------------------------------------------------------------
UINT64 GMLIT2AddressDecode::CommonCalcPAKSParts4(UINT64 lower)
{
    // Galois Swizzle
    const INT32 NumSwizzleBits = 30;

    // Want to swizzle 3 bit wide field.
    // { part[0], slice[1:0] }
    //  gen_galois 0 1 0 39 2 2 1,  start at 5th element
    UINT32 galois[NumSwizzleBits] =
    {6, 5, 7, 6, 3, 2, 7, 6, 3, 2, 5, 3, 2, 7, 6,
        3, 2, 7, 6, 5, 7, 6, 3, 2, 7, 6, 5, 2, 3, 6};
    UINT64 upper_address = lower >> 17;   // start at bit 17 because 128KB is
    // the minimum block that steps across all drams horizontally

    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    UINT64 temp =
        (bits<PA_MSB, 15>(lower) << 15) |
        (bits<13, 12>(lower) << 13) |
        // FB_part 0
        ((bit<11>(lower) ^ bit<2>(swizzle) ^ bit<13>(lower)) << 12) |
        // subpartition
        ((bit<10>(lower) ^ bit<1>(swizzle) ^ bit<13>(lower) ^bit<15>(lower)) << 11) |
        // 2K column bit
        (bit<14>(lower) << 10) |
        // slice0
        (((m_NumXbarL2Slices == 6 || m_NumXbarL2Slices == 3) ? bit<9>(lower):
          bit<9>(lower) ^ bit<14>(lower) ^ bit<0>(swizzle)) << 9) |
        bits<8, 0>(lower);
    return temp;
}

//------------------------------------------------------------------------------
UINT64 GMLIT2AddressDecode::CommonCalcPAKSParts5(UINT64 lower)
{
    // Galois Swizzle
    const INT32 NumSwizzleBits = 30;

    //  gen_galois 0 1 0 30 2 3 1
    UINT32 galois[NumSwizzleBits] =
    {13, 6, 9, 14, 11, 13, 7, 2, 9, 6, 5, 11, 9, 10,
        7, 5, 3, 9, 7, 2, 13, 3, 9, 6, 13, 7, 2, 9, 10, 13};

    UINT64 upper_address = lower >> 17;   // start at bit 17 because 128KB is
    // the minimum block that steps across all drams horizontally

    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    UINT64 temp =
        (bits<PA_MSB, 15>(lower) << 15) |
        (bit<13>(lower) << 14) |
        // FB part 1
        ((bit<12>(lower) ^ bit<3>(swizzle)) << 13) |
        // FB_part 0
        ((bit<11>(lower) ^ bit<2>(swizzle)) << 12) |
        // subpartition
        ((bit<10>(lower) ^ bit<1>(swizzle)) << 11) |
        // 2K column bit
        (bit<14>(lower) << 10) |
        // bit 9 is slice
        ((m_NumXbarL2Slices == 6 ? bit<9>(lower):
          bit<9>(lower) ^ bit<0>(swizzle) ^ bit<14>(lower)) << 9) |
        bits<8, 0>(lower);
    return temp;
}

//------------------------------------------------------------------------------
UINT64 GMLIT2AddressDecode::CommonCalcPAKSParts6(UINT64 lower)
{
    // Galois Swizzle
    const INT32 NumSwizzleBits = 30;

    //  gen_galois 0 1 0 30 2 3 1
    UINT32 galois[NumSwizzleBits] =
    {13, 6, 9, 14, 11, 13, 7, 2, 9, 6, 5, 11, 9, 10,
        7, 5, 3, 9, 7, 2, 13, 3, 9, 6, 13, 7, 2, 9, 10, 13};

    UINT64 upper_address = lower >> 17;   // start at bit 17 because 128KB is
    // the minimum block that steps across all drams horizontally

    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    UINT64 temp =
        (bits<PA_MSB, 15>(lower) << 15) |
        (bit<13>(lower) << 14) |
        // FB part 1
        ((bit<12>(lower) ^ bit<3>(swizzle) ^ bit<13>(lower)) << 13) |
        // FB_part 0
        ((bit<11>(lower) ^ bit<2>(swizzle) ^ bit<13>(lower)) << 12) |
        // subpartition
        ((bit<10>(lower) ^ bit<1>(swizzle) ^ bit<13>(lower)) << 11) |
        // 2K column bit
        (bit<14>(lower) << 10) |
        // bit 9 is slice
        ((m_NumXbarL2Slices == 6 ? bit<9>(lower):
          bit<9>(lower) ^ bit<0>(swizzle) ^ bit<14>(lower)) << 9) |
        bits<8, 0>(lower);
    return temp;
}

//------------------------------------------------------------------------------
// 2 slice part
// swizzle PAKS bits 12 and above
UINT64 GMLIT2AddressDecode::CommonCalcPAKSParts7(UINT64 lower)
{
    // Galois Swizzle
    const INT32 NumSwizzleBits = 30;

    //  gen_galois 0 1 0 30 2 3 1
    UINT32 galois[NumSwizzleBits] =
       {9, 6, 5, 14, 7, 13, 6, 9, 3, 14, 5, 10, 3, 14, 5,
        7, 10, 9, 2, 13, 3, 6, 9, 7, 13, 11, 2, 5, 10, 3};
    // start at bit 15 to rotate 32KB
    UINT64 upper_address = lower >> 18;

    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    UINT64 temp =
        (bits<PA_MSB, 15>(lower) << 15) |
        (bit<13>(lower) << 14) |
        // FB part 1
        ((bit<12>(lower) ^ bit<3>(swizzle)) << 13) |
        // FB_part 0
        ((bit<11>(lower) ^ bit<2>(swizzle)) << 12) |
        // subpartition
        ((bit<10>(lower) ^ bit<1>(swizzle)) << 11) |
        // 2K column bit
        (bit<14>(lower) << 10) |
        // bit 9 is slice
        ((m_NumXbarL2Slices == 6 ? bit<9>(lower):
          bit<9>(lower) ^ bit<0>(swizzle) ^ bit<14>(lower)) << 9) |
        bits<8, 0>(lower);
    return temp;
}

//------------------------------------------------------------------------------
UINT64 GMLIT2AddressDecode::CommonCalcPAKSParts8(UINT64 lower)
{
    // Galois Swizzle
    const INT32 NumSwizzleBits = 30;

    //  gen_galois 0 1 0 30 2 3 1
    UINT32 galois[NumSwizzleBits] =
      {5, 15, 14, 7, 5, 15, 6, 1, 11, 9, 14, 1, 2, 9, 15,
       1, 2, 7, 9, 10, 1, 14, 7, 10, 11, 5, 2, 15, 1, 6};

    UINT64 upper_address = lower >> 17;   // start at bit 17 because 128KB is
    // the minimum block that steps across all drams horizontally

    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    // Any swizzling done with bits PA[16] and above must be common to pitch and blocklinear
    // to satisfy kind agnostic copy for sparse textures.
    UINT64 temp =
        (bits<PA_MSB, 15>(lower) << 15) |
        (bit<13>(lower) << 14) |
        // FB part 1
        ((bit<12>(lower) ^ bit<3>(swizzle) ^ bit<13>(lower)) << 13) |
        // FB_part 0
        ((bit<11>(lower) ^ bit<2>(swizzle) ^ bit<15>(lower)) << 12) |
        // subpartition
        ((bit<10>(lower) ^ bit<1>(swizzle) ^ bit<13>(lower) ^ bit<16>(lower)) << 11) |
        // 2K column bit
        (bit<14>(lower) << 10) |
        // bit 9 is slice
        ((m_NumXbarL2Slices == 6 ? bit<9>(lower):
          bit<9>(lower) ^ bit<0>(swizzle) ^ bit<14>(lower)) << 9) |
        bits<8, 0>(lower);
    return temp;
}

//------------------------------------------------------------------------------
UINT64 GMLIT2AddressDecode::CommonCalcPAKSParts9(UINT64 lower)
{
    // Galois Swizzle
    const INT32 NumSwizzleBits = 30;

    //  gen_galois 0 1 0 30 2 3 1
    UINT32 galois[NumSwizzleBits] =
    {17, 26, 13, 15, 18, 3, 22, 19, 13, 2, 23, 5, 27, 17,
        19, 6, 3, 25, 23, 30, 17, 27, 13, 3, 5, 10, 25, 27, 21, 7};

    UINT64 upper_address = lower >> 18;

    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    // Any swizzling done with bits PA[16] and above must be common to pitch and blocklinear
    // to satisfy kind agnostic copy for sparse textures.
    UINT64 temp =
        (bits<PA_MSB, 15>(lower) << 15) |
        // FB part 2
        ((bit<13>(lower) ^ bit<4>(swizzle)) << 14) |
        // FB part 1
        ((bit<12>(lower) ^ bit<3>(swizzle) ^ bit<13>(lower)) << 13) |
        // FB_part 0
        ((bit<11>(lower) ^ bit<2>(swizzle)) << 12) |
        // subpartition
        ((bit<10>(lower) ^ bit<1>(swizzle)) << 11) |
        // 2K column bit
        (bit<14>(lower) << 10) |
        // bit 9 is slice
        ((m_NumXbarL2Slices == 6 ? bit<9>(lower):
          bit<9>(lower) ^ bit<0>(swizzle) ^ bit<14>(lower)) << 9) |
        bits<8, 0>(lower);
    return temp;
}

//------------------------------------------------------------------------------
UINT64 GMLIT2AddressDecode::CommonCalcPAKSParts10(UINT64 lower)
{
    // Galois Swizzle
    const INT32 NumSwizzleBits = 30;

    //  gen_galois 0 1 0 30 2 3 1
    UINT32 galois[NumSwizzleBits] =
    {17, 26, 13, 15, 18, 3, 22, 19, 13, 2, 23, 5, 27, 17,
        19, 6, 3, 25, 23, 30, 17, 27, 13, 3, 5, 10, 25, 27, 21, 7};

    UINT64 upper_address = lower >> 18;

    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    // Any swizzling done with bits PA[16] and above must be common to pitch and blocklinear
    // to satisfy kind agnostic copy for sparse textures.
    UINT64 temp =
        (bits<PA_MSB, 15>(lower) << 15) |
        // FB part 2
        ((bit<13>(lower) ^ bit<4>(swizzle)) << 14) |
        // FB part 1
        ((bit<12>(lower) ^ bit<3>(swizzle)) << 13) |
        // FB_part 0
        ((bit<11>(lower) ^ bit<2>(swizzle)) << 12) |
        // subpartition
        ((bit<10>(lower) ^ bit<1>(swizzle) ^ bit<13>(lower)) << 11) |
        // 2K column bit
        (bit<14>(lower) << 10) |
        // bit 9 is slice
        ((m_NumXbarL2Slices == 6 ? bit<9>(lower):
          bit<9>(lower) ^ bit<0>(swizzle) ^ bit<14>(lower)) << 9) |
        bits<8, 0>(lower);
    return temp;
}

//------------------------------------------------------------------------------
UINT64 GMLIT2AddressDecode::CommonCalcPAKSParts11(UINT64 lower)
{
    // Galois Swizzle
    const INT32 NumSwizzleBits = 30;

    //  gen_galois 0 1 0 30 2 3 1
    UINT32 galois[NumSwizzleBits] =
    {17, 26, 13, 15, 18, 3, 22, 19, 13, 2, 23, 5, 27, 17,
        19, 6, 3, 25, 23, 30, 17, 27, 13, 3, 5, 10, 25, 27, 21, 7};

    UINT64 upper_address = lower >> 18;

    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    // Any swizzling done with bits PA[16] and above must be common to pitch and blocklinear
    // to satisfy kind agnostic copy for sparse textures.
    UINT64 temp =
        (bits<PA_MSB, 15>(lower) << 15) |
        // FB part 2
        ((bit<13>(lower) ^ bit<4>(swizzle)) << 14) |
        // FB part 1
        ((bit<12>(lower) ^ bit<3>(swizzle)) << 13) |
        // FB_part 0
        ((bit<11>(lower) ^ bit<2>(swizzle)) << 12) |
        // subpartition
        ((bit<10>(lower) ^ bit<1>(swizzle)) << 11) |
        // 2K column bit
        (bit<14>(lower) << 10) |
        // bit 9 is slice
        ((m_NumXbarL2Slices == 6 ? bit<9>(lower):
          bit<9>(lower) ^ bit<0>(swizzle) ^ bit<14>(lower)) << 9) |
        bits<8, 0>(lower);
    return temp;
}

//------------------------------------------------------------------------------
UINT64 GMLIT2AddressDecode::CommonCalcPAKSParts12(UINT64 lower)
{
    // Galois Swizzle
    const INT32 NumSwizzleBits = 30;

    //  gen_galois 0 1 0 30 2 3 1
    UINT32 galois[NumSwizzleBits] =
    {17, 26, 13, 15, 18, 3, 22, 19, 13, 2, 23, 5, 27, 17,
        19, 6, 3, 25, 23, 30, 17, 27, 13, 3, 5, 10, 25, 27, 21, 7};

    UINT64 upper_address = lower >> 18;

    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((upper_address>>i)&1)*galois[i]);
    }

    // Any swizzling done with bits PA[16] and above must be common to pitch and blocklinear
    // to satisfy kind agnostic copy for sparse textures.
    UINT64 temp =
        (bits<PA_MSB, 15>(lower) << 15) |
        // FB part 2
        ((bit<13>(lower) ^ bit<4>(swizzle)) << 14) |
        // FB part 1
        ((bit<12>(lower) ^ bit<3>(swizzle)) << 13) |
        // FB_part 0
        ((bit<11>(lower) ^ bit<2>(swizzle) ^ bit<16>(lower)) << 12) |
        // subpartition
        ((bit<10>(lower) ^ bit<1>(swizzle) ^ bit<13>(lower)) << 11) |
        // 2K column bit
        (bit<14>(lower) << 10) |
        // bit 9 is slice
        ((m_NumXbarL2Slices == 6 ? bit<9>(lower):
          bit<9>(lower) ^ bit<0>(swizzle) ^ bit<14>(lower)) << 9) |
        bits<8, 0>(lower);
    return temp;
}

//------------------------------------------------------------------------------
    /*
       assembles offset bits O[11], O[9:2] into m_Col
       2 slice uses 4KB PartitionInterleave, with PAKS packed as
       {PA[n-1:15}, PA[13:11], PA[14], PA[10:0]}
       m_fb_pdr = {Q, O[11], O[9:7]}, 128B aligned
       O[11] = PA[14] = m_FbPadr[3] = 2K column bit
       m_Col includes 2K column bit
     */
/*virtual*/ void GMLIT2AddressDecode::ComputeRBSCParts8Banks8()
{
    UINT32 alignedSector = (m_L2SectorMsk & 0xF) | ((m_L2SectorMsk>>4) & 0xF);
    alignedSector = log2i(alignedSector);

    m_Col =
        (bits<3, 0>(m_FbPadr) << 5) | (bits<1, 0>(alignedSector)<<3) | bits<4, 2>(m_L2Offset);

    UINT32 numColBits = ComputeColumnBits();
    MASSERT(numColBits>=9);
    UINT32 rb=0;

    // O[10] = subpartition
    rb = bits<31, 4>(m_FbPadr);

    // We only support col_bits >= 9
    UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
    m_ExtBank = bit<3>(tmp);
    // Want to step through bank groups.
    // 4 bank groups: BG[1:0] = bank[2:1]
    // 2 bank groups: BG[0] = bank[2]
    m_Bank = (bit<0>(tmp) << 2) | // LSBs in top bits for BG
        (bit<1>(tmp) << 1) |
        (bit<2>(tmp) << 0);
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // It's a hack using common module for all partitions, after all this templating,
    // but there is no clean way to separate out intermediate terms that comprise
    // the swizzle without a lot of replication.
    UINT32 swizzle_manilwred = 0;
    switch (m_NumLtcs)
    {
        case 1:
            // part1  -- walking 1s test only good to 35 bits
            swizzle_manilwred =
                ((bit<0>(m_Row) ^ bit<2>(m_Row)) << 2) |  // can be bit<1>(row)
                ((bit<1>(m_Row)) << 1) |
                ((bit<2>(m_Row)) << 0);
            break;
        case 2:
            // part2 -- good to 512 row[1] = 128
            swizzle_manilwred =
                ((bit<0>(m_Row)) << 2) |
                ((bit<2>(tmp)) << 1) |
                ((bit<2>(m_Row)) << 0);
            break;
        case 3:
            // part3 -- good to 512
            swizzle_manilwred =
                ((bit<0>(m_Row)) << 2) |
                ((bit<1>(m_Row)) << 1) |
                ((bit<2>(m_Row)) << 0);
            break;
        case 4:
            // part4  -- good to 512
            swizzle_manilwred =
                ((bit<2>(tmp) ^ bit<0>(m_Row) ^ bit<1>(m_Row)) << 2) |
                ((0) << 1) |
                ((0) << 0);
            break;
        case 5:
            // part5 -- can't be made very good
            swizzle_manilwred =
                ((bit<1>(tmp) ^ bit<0>(m_Row)) << 2) |
                ((0) << 1) |
                ((bit<0>(m_Row)) << 0);
            break;
        case 6:
            // part6 -- good to 512
            swizzle_manilwred =
                ((0) << 2) |
                ((bit<0>(m_Row)) << 1) |
                ((0) << 0);
            break;
        case 7:
            // part7 -- good to 512
            swizzle_manilwred =
                ((bit<0>(m_Row)) << 2) |
                ((bit<1>(m_Row)) << 1) |
                ((0) << 0);
            break;
        case 8:
            // part8 -- good to 512
            swizzle_manilwred =
                ((bit<1>(tmp)) << 2) |
                ((0) << 1) |
                ((bit<2>(m_Row)) << 0);
            break;
        case 9:
            // part9 -- good to 512
            swizzle_manilwred =
                ((bit<1>(m_Row) ^ bit<2>(tmp)) << 2) |
                (bit<0>(m_Row) << 1) |
                ((bit<1>(tmp)) << 0);
            break;
        case 10:
            // part10 -- good to 512
            swizzle_manilwred =
                ((bit<2>(tmp) ^ bit<0>(m_Row)) << 2) |
                ((bit<0>(m_Row)) << 1) |
                ((0) << 0);
            break;
        case 11:
            // part11 -- good to 512
            swizzle_manilwred =
                ((bit<0>(m_Row)) << 2) |
                ((bit<1>(m_Row)) << 1) |
                ((bit<2>(m_Row)) << 0);
            break;
        case 12:
            // part12 -- good to 512
            swizzle_manilwred =
                ((bit<1>(tmp) ^ bit<0>(m_Row) ^ bit<2>(m_Row)) << 2) |
                ((bit<2>(tmp) ^ bit<2>(m_Row)) << 1) |
                ((0) << 0);
            break;

        default:
            Printf(Tee::PriError, "Unsupported number of Ltcs %d\n",
                    m_NumLtcs);
            MASSERT(0);
    }

    // Swizzle
    // swizzles are tailored to number bank groups.
    const int num_swizzle_bits = NUM_GALOIS_BANK_SWIZZLE_BITS;
    // gen_galois 0 1 0 36 3 2 1

    UINT32 galois_4BG[] =
      {3, 6, 2, 5, 3, 7, 5, 6, 2, 7, 4, 6, 3, 4, 6, 3,
       7, 5, 3, 4, 6, 3, 4, 6, 5, 2, 4, 7, 2, 4, 7, 5};
    // generate swizzle
    UINT64 UpperAddress = m_Row >> 3;
    UINT32 swizzle= 0;

    for (int i=0;i<num_swizzle_bits;++i)
    {
        swizzle = swizzle ^ (((UpperAddress>>i)&1)*galois_4BG[i]);
    }

    // Must ^ only against banks to preserve bank group relationships in mapping.
    m_Bank = m_Bank ^ swizzle ^ swizzle_manilwred;
    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 9) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 9);
    return;
}

//------------------------------------------------------------------------------
/*virtual*/ void GMLIT2AddressDecode::ComputeRBSCParts8Banks16()
{
    UINT32 alignedSector = (m_L2SectorMsk & 0xF) | ((m_L2SectorMsk>>4) & 0xF);
    alignedSector = log2i(alignedSector);
    m_Col = (bits<3, 0>(m_FbPadr) << 5) | (bits<1, 0>(alignedSector)<<3) |  bits<4, 2>(m_L2Offset);

    UINT32 numColBits = ComputeColumnBits();
    UINT32 rb=0;

    // O[10] = subpartition
    rb = bits<31, 4>(m_FbPadr);

    // We only support col_bits >= 9
    UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
    m_ExtBank = bit<4>(tmp);

    // Want to step through bank groups.
    // 4 bank groups: BG[1:0] = bank[2:1]
    // 2 bank groups: BG[0] = bank[2]
    m_Bank =
        (bit<1>(tmp) << 3) | // LSBs in top bits for BG
        (bit<0>(tmp) << 2) |
        (bit<3>(tmp) << 1) |
        (bit<2>(tmp) << 0);
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // It's a hack using common module for all partitions, after all this templating, but there is no clean
    // way to separate out intermediate terms that comprise the swizzle without a lot of replication.

    UINT32 swizzle_manilwred = 0;

    switch (m_NumLtcs)
    {
        case 1:
            // part1  --  walking 1s test only good to 35 bits
            swizzle_manilwred =
                ((bit<3>(tmp)) << 3) |
                ((bit<0>(m_Row) ^ bit<1>(m_Row)) << 2) |
                ((0) << 1) |
                ((0) << 0);
            break;
        case 2:
            // part2 -- good to 512, OK for 4
            swizzle_manilwred =
                ((bit<2>(tmp) ^ bit<1>(m_Row)) << 3) |
                ((bit<3>(tmp)) << 2) |
                ((0) << 1) |
                ((bit<2>(m_Row)) << 0);
            break;
        case 3:
            // part3 -- good to 512
            swizzle_manilwred =
                ((bit<3>(tmp) ^ bit<1>(m_Row)) << 3) |
                ((bit<0>(m_Row)) << 2) |
                ((bit<3>(m_Row)) << 1) |
                ((0) << 0);
            break;
            // Doing all even cases as 4 partition bank mapping.
        case 4:
            // part4  -- good to 512
            swizzle_manilwred =
                ((bit<3>(tmp) ^ bit<0>(m_Row)) << 3) |
                ((bit<2>(tmp) ^ bit<0>(m_Row) ^ bit<2>(m_Row)) << 2) |
                ((bit<2>(m_Row)) << 1) |
                ((bit<1>(m_Row)) << 0);
            break;
        case 5:
            // part5 -- good to 512
            swizzle_manilwred =
                ((bit<3>(tmp)) << 3) |
                ((bit<3>(tmp)) << 2) |
                ((bit<1>(m_Row)) << 1) |
                ((bit<2>(m_Row)) << 0);
            break;

        case 6:
            // part6 -- good to 512
            swizzle_manilwred =
                ((bit<3>(tmp)) << 3) |
                ((bit<1>(m_Row)) << 2) |
                ((bit<2>(tmp)) << 1) |
                ((0) << 0);
            break;
        case 7:
            // part7 -- good to 512
            swizzle_manilwred =
                ((bit<0>(m_Row) ^ bit<2>(m_Row)) << 3) |
                ((bit<3>(tmp) ^ bit<1>(m_Row)) << 2) |
                ((0) << 1) |
                ((0) << 0);
            break;
        case 8:
            // part8 -- good to 512
            swizzle_manilwred =
                ((0) << 3) |
                ((0) << 2) |
                ((0) << 1) |
                ((0) << 0);
            break;
        case 9:
            // part9 -- good to 512
            swizzle_manilwred =
                ((bit<0>(m_Row) ^ bit<2>(tmp)) << 3) |
                ((bit<3>(tmp)) << 2) |
                ((0) << 1) |
                ((0) << 0);
            break;
        case 10:
            // part10 -- good to 512
            swizzle_manilwred =
                ((bit<3>(tmp) ^ bit<1>(m_Row)) << 3) |
                ((bit<0>(m_Row) ^ bit<1>(m_Row)) << 2) |
                ((0) << 1) |
                ((0) << 0);
            break;
        case 11:
            // part11 -- good to 512
            swizzle_manilwred =
                ((bit<0>(m_Row) ^ bit<1>(m_Row)) << 3) |
                ((bit<3>(tmp)) << 2) |
                ((0) << 1) |
                ((0) << 0);
            break;
        case 12:
            // part12 -- good to 512
            swizzle_manilwred =
                ((0) << 3) |
                ((bit<2>(tmp)) << 2) |
                ((0) << 1) |
                ((0) << 0);
            break;
        default:
            Printf(Tee::PriError, "Unsupported number of Ltcs %d\n",
                    m_NumLtcs);
            MASSERT(0);
    }
    // Swizzle
    // Galois Swizzle
    const int num_swizzle_bits = NUM_GALOIS_BANK_SWIZZLE_BITS;
    UINT32 galois[] =
       {13, 6, 12, 14, 3, 6, 12, 8, 15, 14, 10, 13, 2, 4, 8,
        13, 10, 12, 8, 7, 14, 5, 6, 2, 8, 3, 14, 1, 7, 4, 15, 14};

    // what bit to start galois swizzle
    // Swizzle manilwred using m_Row[3:0]
    UINT64 UpperAddress = m_Row >> 1; // start at adr[18] because of vertical bank stacking, part is already swizzled on Adr[17]
    // generate swizzle
    UINT32 swizzle= 0;
    for (int i=0;i<num_swizzle_bits;++i)
    {
        swizzle = swizzle ^ (((UpperAddress>>i)&1)*galois[i]);
    }

    // Must ^ only against banks to preserve bank group relationships in mapping
    m_Bank = bits<3, 0>(m_Bank) ^ swizzle ^ swizzle_manilwred;
    // extract column as row bits
    // m_Col[c-1:9] = m_Row[c-10:0], where c > 9
    m_Col = ((m_Row << 9) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 9);
    return;
}

/*virtual*/ void GMLIT2AddressDecode::ComputeRBSC()
{
    m_FbPadr = m_Padr; //Observed in Fmodel
    ComputeFbPadr();
    switch (m_NumDRAMBanks)
    {
        case 4:
        case 8:
            ComputeRBSCParts8Banks8();
            break;
        case 16:
            ComputeRBSCParts8Banks16();
            break;
        default:
            Printf(Tee::PriError, "%s Invalid number of DramBanks [%u]\n",
                    __FUNCTION__, m_NumDRAMBanks);
            MASSERT(!"Invalid number of DramBanks");
    }
    return;
}
//------------------------------------------------------------------------------
UINT64 GMLIT2AddressDecode::CommonCalcPAKSBlockLinearParts4Lower()
{
    // Can only alter address[11:0], within 4KB
    // Galois Swizzle
    const INT32 NumSwizzleBits = 30;

    // Want to swizzle 4 bit wide field.
    // { part[1:0], slice[1:0] }
    UINT32 galois[NumSwizzleBits] = {7, 6, 1, 3, 5, 7, 2, 1, 3, 5, 6, 5, 6, 1,
        7, 6, 2, 3, 6, 3, 2, 5, 6, 1, 7, 1, 3, 5, 7, 1};

    // Because all DRAMs show up once vertically in an 8KB blocklinear column,
    // we must swizzle bank bits starting at Adr[16] for 8 banks,
    // and Adr[17] for 16 banks.  This is so when we stack blocklinear blocks,
    // we never camp vertically on same DRAM bank.

    UINT64 UpperAddress = m_PhysAddr >> 17;
    // generate swizzle
    UINT32 swizzle= 0;
    for (INT32 i = 0; i < NumSwizzleBits; ++i)
    {
        swizzle = swizzle ^ (((UpperAddress>>i)&1)*galois[i]);
    }

    // Swizzling above 4KB must be identical for blocklinear, pitch.
    // This means bit 12 and up must be swizzled identically, manilwred and Galois.
    return (bits<PADDR_MSB, 12>(m_PhysAddr) << 12) |
        // part0
        ((bit<11>(m_PhysAddr) ^ bit<15>(m_PhysAddr)) << 11) |
        // slice1
        ((bit<10>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ bit<16>(m_PhysAddr)
          ^ bit<1>(swizzle)) << 10) |
        // slice0
        // 6 slices we don't want to swizzle O[9] in PAKS
        ((m_NumXbarL2Slices == 6 ? bit<9>(m_PhysAddr):
          bit<9>(m_PhysAddr) ^ bit<14>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |
        bits<8, 0>(m_PhysAddr);
}

//------------------------------------------------------------------------------
GMLIT2AddressEncode::GMLIT2AddressEncode(GpuSubdevice* pSubdev) :
    GMLIT1AddressEncode(pSubdev),
    m_LtcId(0),
    m_EccLinesPerPage(15), //hw/lwgpu/clib/lwshared/Memory/FermiAddressMapping.cpp
    m_LtcLinesPerPage(16),
    m_FloorsweepingActive(false),
    m_DoubleDensityActive(false)
{

    m_PartitionInterleave = 2048;

    const UINT32 fbMask = pSubdev->GetFsImpl()->FbMask();
    const UINT32 fbps = (UINT32)Utility::CountBits(fbMask);

    UINT32  hwFbNum = (UINT32)Utility::BitScanForward(fbMask, 0);
    for (UINT32 i = 0; i < fbps; i++)
    {
        UINT32 ltcMask = pSubdev->GetFsImpl()->L2Mask(hwFbNum);

       m_LtcMaskForLFbp.push_back(ltcMask);

        if (ltcMask != s_MaxL2MaskPerFbp)
            m_FloorsweepingActive = true;

        UINT32 rowBits = DRF_VAL(_PFB, _FBPA_0_CFG1, _ROWA,
                pSubdev->RegRd32(LW_PFB_FBPA_0_CFG1 + hwFbNum*0x1000));
        switch (rowBits)
        {
            case LW_PFB_FBPA_5_CFG1_ROWA_10:
                rowBits = 10;
                break;
            case LW_PFB_FBPA_5_CFG1_ROWA_11:
                rowBits = 11;
                break;
            case LW_PFB_FBPA_5_CFG1_ROWA_12:
                rowBits = 12;
                break;
            case LW_PFB_FBPA_5_CFG1_ROWA_13:
                rowBits = 13;
                break;
            case LW_PFB_FBPA_5_CFG1_ROWA_14:
                rowBits = 14;
                break;
            case LW_PFB_FBPA_5_CFG1_ROWA_15:
                rowBits = 15;
                break;
            case LW_PFB_FBPA_5_CFG1_ROWA_16:
                rowBits = 16;
                break;
            case LW_PFB_FBPA_5_CFG1_ROWA_17:
                rowBits = 17;
                break;
            default:
                Printf(Tee::PriWarn,
                       "%u Seems incorrect number of row bits as per refmanual."
                       " MODS will fallback to default value 13\n",
                       rowBits);
                rowBits = 13;
                break;
        }
        m_NumRowBitsPerFbp.push_back(rowBits);

        if ( m_NumRowBitsPerFbp.size() > 1 )
            m_DoubleDensityActive |= (m_NumRowBitsPerFbp[i] != m_NumRowBitsPerFbp[i-1]);

        hwFbNum = (UINT32)Utility::BitScanForward(fbMask, hwFbNum + 1);
    }

}

//------------------------------------------------------------------------------
void GMLIT2AddressEncode::MapRBCToXBar()
{
    // inputs: row, bank, extbank, col
    // want to recover fb_padr
    // Assuming at least 9 column bits, 2KB pages
    // Put upper column bits back into row
    UINT32 num_col_bits = ComputeColumnBits();

    MASSERT(num_col_bits >= 9);

    UINT32 row = m_Row << (num_col_bits - 9) | (m_Col >> 9);

    // recover extbank
    UINT32 extBank = 0;
    switch (m_NumDRAMBanks)
    {
        case 8:
            extBank = BankSwizzle8(row >> 3, row) | (m_ExtBank << 3);
            break;
        case 16:
            extBank = BankSwizzle16(row >> 1, row) | (m_ExtBank << 4);
            break;
    }

    // recover rb
    UINT32 rb = row * (m_NumDRAMBanks << (m_ExtBank? 1 : 0)) | extBank;

    // recover O[9:7] from column
    m_Fb_Padr = (rb << 4) | bits<8, 5>(m_Col);

    if (m_ECCOn)
        m_Fb_Padr = static_cast <UINT32> (ReverseEccAddr());

    UINT32 numFBPadrBits = (ComputeColumnBits() + 2) +  log2i(m_NumDRAMBanks)
        + (m_NumExtDRAMBanks? 1 : 0) + m_NumRowBitsPerFbp[m_Partition] - log2i(m_NumXbarL2Slices) - 7;

    bool inUpperMemory = ((m_Row >> (m_NumRowBitsPerFbp[m_Partition] - 1)) == 0x1) && (m_SubPart == 1) && m_FloorsweepingActive;

    m_Fb_Padr |= ((inUpperMemory? 1 : 0) << (numFBPadrBits + 1));

    if ((inUpperMemory) && (m_DoubleDensityActive))
    {
        UINT32 repetition = (UINT32)(m_Fb_Padr >> (numFBPadrBits)) & 0x3;
        if (repetition >= 2)
        {
            m_Fb_Padr = (m_Fb_Padr & ((1 << (numFBPadrBits)) - 1)) |
                          (repetition << (s_FbpaPadrBitWidth-1));
            m_Fb_Padr |= (UINT64)1 << numFBPadrBits;
        }
    }

    switch (m_NumXbarL2Slices)
    {
        case 6:
        case 3:
            m_Padr = m_Fb_Padr;
            m_Slice = m_SubPart; //set slice0
            break;
        case 2:
            m_Padr =  ((m_Fb_Padr >> 3)  << 2) | bits<1, 0>(m_Fb_Padr);
            m_Slice = bit<2>(m_Fb_Padr);
            break;
        default:
            m_Padr = ((m_Fb_Padr>>3) << 2) | (m_Fb_Padr & 0x3);
            m_Slice = bit<7>(m_Col)|(m_SubPart<<1);
            break;
    }

    m_L2SectorMsk = 1 << bits<4, 3>(m_Col);
    m_L2Offset = (bits<4, 3>(m_Col)<<5) | bits<2, 0>(m_Col)<<2;

    if (((m_SubPart == 1) && m_FloorsweepingActive) || (m_DoubleDensityActive && (m_Row >> (m_NumRowBitsPerFbp[m_Partition] - 1))))
    {
        // Set the upper address bit in the xbar address
        m_Padr |= (UINT64)1 << (s_FbpaBitEnableUpperPartition);
    }

}

//------------------------------------------------------------------------------
UINT32 GMLIT2AddressEncode::BankSwizzle8(UINT64 upperAdr, UINT32 row)
{
    UINT08 galois[] = {3, 6, 2, 5, 3, 7, 5, 6, 2, 7, 4, 6, 3, 4, 6, 3, 7, 5, 3, 4, 6, 3, 4, 6, 5, 2, 4, 7, 2, 4, 7, 5};

    UINT32 swizzle = GenerateSwizzle(galois, NUM_GALOIS_BANK_SWIZZLE_BITS, upperAdr);

    // reverse swizzle the bank
    UINT32 bank = m_Bank ^ swizzle;

    UINT32 row_swizzle = 0;
    switch (m_NumPartitions)
    {
        case 1:
            row_swizzle =
                ((bit<0>(row) ^ bit<2>(row)) << 2) |
                ((bit<1>(row)) << 1) |
                ((bit<2>(row)) << 0);
            break;
        case 2:
            row_swizzle =
                ((bit<0>(row)) << 2) |
                ((0) << 1) |
                ((bit<2>(row)) << 0);
            break;
        case 3:
            row_swizzle =
                ((bit<0>(row)) << 2) |
                ((bit<1>(row)) << 1) |
                ((bit<2>(row)) << 0);
            break;
        case 4:
            row_swizzle =
                ((bit<0>(row) ^ bit<1>(row)) << 2) |
                ((0) << 1) |
                ((0) << 0);
            break;
        case 5:
            row_swizzle =
                ((bit<0>(row)) << 2) |
                ((0) << 1) |
                ((bit<0>(row)) << 0);
            break;
        case 6:
            row_swizzle =
                ((0) << 2) |
                ((bit<0>(row)) << 1) |
                ((0) << 0);
            break;
        case 7:
            row_swizzle =
                ((bit<0>(row)) << 2) |
                ((bit<1>(row)) << 1) |
                ((0) << 0);
            break;
        case 8:
            row_swizzle =
                ((0) << 2) |
                ((0) << 1) |
                ((bit<2>(row)) << 0);
            break;
        case 9:
            row_swizzle =
                ((bit<1>(row)) << 2) |
                ((bit<0>(row)) << 1) |
                ((0) << 0);
            break;
        case 10:
            row_swizzle =
                ((bit<0>(row)) << 2) |
                ((bit<0>(row)) << 1) |
                ((0) << 0);
            break;
        case 11:
            row_swizzle =
                ((bit<0>(row)) << 2) |
                ((bit<1>(row)) << 1) |
                ((bit<2>(row)) << 0);
            break;
        case 12:
            row_swizzle =
                ((bit<0>(row) ^ bit<2>(row)) << 2) |
                ((bit<2>(row)) << 1) |
                ((0) << 0);
            break;
        case 13:
            row_swizzle =
                ((bit<0>(row) ^ bit<2>(row)) << 2) |
                ((bit<2>(row)) << 1) |
                ((0) << 0);
            break;
        case 14:
            row_swizzle =
                ((bit<0>(row) ^ bit<2>(row)) << 2) |
                ((bit<2>(row)) << 1) |
                ((0) << 0);
            break;
        case 15:
            row_swizzle =
                ((bit<0>(row) ^ bit<2>(row)) << 2) |
                ((bit<2>(row)) << 1) |
                ((0) << 0);
            break;
        case 16:
            row_swizzle =
                ((bit<0>(row) ^ bit<2>(row)) << 2) |
                ((bit<2>(row)) << 1) |
                ((0) << 0);
            break;
        default:
            Printf(Tee::PriError,
                   "Unsupported number of partitions - %u\n", m_NumPartitions);
            MASSERT(!"Unsupported number of partitions!");
            break;
    }

    UINT32 bank_prime =
      ((bit<2>(bank) ^ bit<2>(row_swizzle)) << 2) |
      ((bit<1>(bank) ^ bit<1>(row_swizzle)) << 1) |
      ((bit<0>(bank) ^ bit<0>(row_swizzle)) << 0);

    UINT32 bnk_swizzle = 0;
    switch (m_NumPartitions)
    {
        case 1:
            bnk_swizzle = 0;
            break;
        case 2:
            bnk_swizzle =
                ((bit<0>(bank_prime)) << 1);
            break;
        case 3:
            bnk_swizzle = 0;
            break;
        case 4:
            bnk_swizzle =
                ((bit<0>(bank_prime)) << 2) |
                ((0) << 1) |
                ((0) << 0);
            break;
        case 5:
            bnk_swizzle =
                ((bit<1>(bank_prime)) << 2) |
                ((0) << 1) |
                ((0) << 0);
            break;
        case 6:
            bnk_swizzle = 0;
            break;
        case 7:
            bnk_swizzle = 0;
            break;
        case 8:
            bnk_swizzle =
                ((bit<1>(bank_prime)) << 2);
            break;
        case 9:
            bnk_swizzle =
                ((bit<0>(bank_prime) ^ bit<1>(bank_prime)) << 2) |
                ((bit<1>(bank_prime)) << 0);
            break;
        case 10:
            bnk_swizzle =
                ((bit<0>(bank_prime)) << 2);
            break;
        case 11:
            bnk_swizzle = 0;
            break;
        case 12:
            bnk_swizzle =
                ((bit<1>(bank_prime) ^ bit<0>(bank_prime)) << 2) |
                ((bit<0>(bank_prime)) << 1) |
                ((0) << 0);
            break;
        case 13:
            bnk_swizzle =
                ((bit<1>(bank_prime) ^ bit<0>(bank_prime)) << 2) |
                ((bit<0>(bank_prime)) << 1) |
                ((0) << 0);
            break;
        case 14:
            bnk_swizzle =
                ((bit<1>(bank_prime) ^ bit<0>(bank_prime)) << 2) |
                ((bit<0>(bank_prime)) << 1) |
                ((0) << 0);
            break;
        case 15:
            bnk_swizzle =
                ((bit<1>(bank_prime) ^ bit<0>(bank_prime)) << 2) |
                ((bit<0>(bank_prime)) << 1) |
                ((0) << 0);
            break;
        case 16:
            bnk_swizzle =
                ((bit<1>(bank_prime) ^ bit<0>(bank_prime)) << 2) |
                ((bit<0>(bank_prime)) << 1) |
                ((0) << 0);
            break;
        default:
            Printf(Tee::PriError,
                   "Unsupported number of partitions - %u\n", m_NumPartitions);
            MASSERT(!"Unsupported number of partitions!");
            break;
    }

    // undo bank grouping arranging
    bnk_swizzle =
        ((bit<0>(bank_prime) ^ bit<0>(bnk_swizzle)) << 2) |
        ((bit<1>(bank_prime) ^ bit<1>(bnk_swizzle)) << 1) |
        ((bit<2>(bank_prime) ^ bit<2>(bnk_swizzle)) << 0);

    return bnk_swizzle;
}

//------------------------------------------------------------------------------
UINT32 GMLIT2AddressEncode::BankSwizzle16(UINT64 upperAdr, UINT32 row)
{
    UINT08 galois[] = {13, 6, 12, 14, 3, 6, 12, 8, 15, 14, 10, 13, 2, 4, 8, 13, 10, 12, 8, 7, 14, 5, 6, 2, 8, 3, 14, 1, 7, 4, 15, 14};

    UINT32 swizzle = GenerateSwizzle(galois, NUM_GALOIS_BANK_SWIZZLE_BITS, upperAdr);

    // reverse swizzle the bank
    UINT32 bank = m_Bank ^ swizzle;

    // undo bank grouping arranging

    UINT32 row_swizzle = 0;
    switch (m_NumPartitions)
    {
        case 1:
            row_swizzle =
            ((bit<0>(row) ^ bit<1>(row)) << 2) |
            ((0) << 1) |
            ((0) << 0);
            break;
        case 2:
            row_swizzle =
            ((bit<1>(row)) << 3) |
            ((0) << 1) |
            ((bit<2>(row)) << 0);
            break;
        case 3:
            row_swizzle =
            ((bit<1>(row)) << 3) |
            ((bit<0>(row)) << 2) |
            ((bit<3>(row)) << 1) |
            ((0) << 0);
            break;
        case 4:
            row_swizzle =
            ((bit<0>(row)) << 3) |
            ((bit<0>(row) ^ bit<2>(row)) << 2) |
            ((bit<2>(row)) << 1) |
            ((bit<1>(row)) << 0);
            break;
        case 5:
            row_swizzle =
            ((bit<1>(row)) << 1) |
            ((bit<2>(row)) << 0);
            break;
        case 6:
            row_swizzle =
            ((bit<1>(row)) << 2) |
            ((0) << 0);
            break;
        case 7:
            row_swizzle =
            ((bit<0>(row) ^ bit<2>(row)) << 3) |
            ((bit<1>(row)) << 2) |
            ((0) << 1) |
            ((0) << 0);
            break;
        case 8:
            row_swizzle =
            ((0) << 3) |
            ((0) << 2) |
            ((0) << 1) |
            ((0) << 0);
            break;
        case 9:
            row_swizzle =
            ((bit<0>(row)) << 3) |
            ((0) << 1) |
            ((0) << 0);
            break;
        case 10:
            row_swizzle =
            ((bit<1>(row)) << 3) |
            ((bit<0>(row) ^ bit<1>(row)) << 2) |
            ((0) << 1) |
            ((0) << 0);
            break;
        case 11:
            row_swizzle =
            ((bit<0>(row) ^ bit<1>(row)) << 3) |
            ((0) << 1) |
            ((0) << 0);
            break;
        case 12:
            row_swizzle =
            ((0) << 3) |
            ((0) << 1) |
            ((0) << 0);
            break;
        case 13:
            row_swizzle =
            ((0) << 3) |
            ((bit<0>(row)) << 1) |
            ((0) << 0);
            break;
        case 14:
            row_swizzle =
            ((0) << 3) |
            ((bit<0>(row)) << 1) |
            ((0) << 0);
            break;
        case 15:
            row_swizzle =
            ((0) << 3) |
            ((bit<0>(row)) << 1) |
            ((0) << 0);
            break;
        case 16:
            row_swizzle =
            ((0) << 3) |
            ((bit<0>(row)) << 1) |
            ((0) << 0);
            break;
        default:
            Printf(Tee::PriError,
                   "Unsupported number of partitions - %u\n", m_NumPartitions);
            MASSERT(!"Unsupported number of partitions!");
            break;
    }

    UINT32 bank_prime =
        ((bit<3>(bank) ^ bit<3>(row_swizzle)) << 3) |
        ((bit<2>(bank) ^ bit<2>(row_swizzle)) << 2) |
        ((bit<1>(bank) ^ bit<1>(row_swizzle)) << 1) |
        ((bit<0>(bank) ^ bit<0>(row_swizzle)) << 0);

    UINT32 bnk_swizzle = 0;
    switch (m_NumPartitions)
    {
        case 1:
            bnk_swizzle =
            ((bit<1>(bank_prime)) << 3) |
            ((0) << 1);
            break;
        case 2:
            bnk_swizzle =
            ((bit<0>(bank_prime)) << 3) |
            ((bit<1>(bank_prime)) << 2) |
            ((0) << 1);
            break;
        case 3:
            bnk_swizzle =
            ((bit<1>(bank_prime)) << 3) |
            ((0) << 0);
            break;
        case 4:
            bnk_swizzle =
            ((bit<1>(bank_prime)) << 3) |
            ((bit<0>(bank_prime)) << 2);
            break;
        case 5:
            bnk_swizzle =
            ((bit<1>(bank_prime)) << 3) |
            ((bit<1>(bank_prime)) << 2);
            break;

        case 6:
            bnk_swizzle =
            ((bit<1>(bank_prime) ^ bit<0>(bank_prime)) << 3) |
            ((bit<0>(bank_prime)) << 1) |
            ((0) << 0);
            break;
        case 7:
            bnk_swizzle =
            ((bit<1>(bank_prime)) << 2) |
            ((0) << 1) |
            ((0) << 0);
            break;
        case 8:
            bnk_swizzle =
            ((0) << 3) |
            ((0) << 2) |
            ((0) << 1) |
            ((0) << 0);
            break;
        case 9:
            bnk_swizzle =
            ((bit<0>(bank_prime)) << 3) |
            ((bit<1>(bank_prime)) << 2) |
            ((0) << 1) |
            ((0) << 0);
            break;
        case 10:
            bnk_swizzle =
            ((bit<1>(bank_prime)) << 3) |
            ((0) << 1) |
            ((0) << 0);
            break;
        case 11:
            bnk_swizzle =
            ((bit<1>(bank_prime)) << 2) |
            ((0) << 1) |
            ((0) << 0);
            break;
        case 12:
            bnk_swizzle =
            ((0) << 3) |
            ((bit<0>(bank_prime)) << 2) |
            ((0) << 0);
            break;
        case 13:
            bnk_swizzle =
            ((0) << 3) |
            ((bit<0>(bank_prime)) << 2) |
            ((0) << 0);
            break;
        case 14:
            bnk_swizzle =
            ((0) << 3) |
            ((bit<0>(bank_prime)) << 2) |
            ((0) << 0);
            break;
        case 15:
            bnk_swizzle =
            ((0) << 3) |
            ((bit<0>(bank_prime)) << 2) |
            ((0) << 0);
            break;
        case 16:
            bnk_swizzle =
            ((0) << 3) |
            ((bit<0>(bank_prime)) << 2) |
            ((0) << 0);
            break;
        default:
            Printf(Tee::PriError,
                   "Unsupported number of partitions - %u\n", m_NumPartitions);
            MASSERT(!"Unsupported number of partitions!");
            break;
    }
    bnk_swizzle =
        ((bit<1>(bank_prime) ^ bit<1>(bnk_swizzle)) << 3) |
        ((bit<0>(bank_prime) ^ bit<0>(bnk_swizzle)) << 2) |
        ((bit<3>(bank_prime) ^ bit<3>(bnk_swizzle)) << 1) |
        ((bit<2>(bank_prime) ^ bit<2>(bnk_swizzle)) << 0);

    return bnk_swizzle;
}

void GMLIT2AddressEncode::MapXBarToPAKS()
{
    // need partition, subpa, fb_padr, col
    // Want to generate Q, R, O, slice, L2 address, PAKS
    // m_slice = (bit<0>(Q) << 1) | bit<9>(O);
    // fb_padr = {Q[n-1:0], O[9:7]}
    // assembles offset bits O[9:2] into m_col
    //        m_col = (bits<2, 0>(m_fb_padr) << 5) | bits<6, 2>(m_l2offset);

    // Generate Q, R, O from fb_padr, col, and subpart
    UINT32 hacksector = bits<4, 3>(m_Col);

    // padr starts on byte address 7, 256B aligned has padr[0]=0
    // client can set both sector_mask[7:4] and padr[0]=1 for upper 128B
    switch (m_NumXbarL2Slices)
    {
        case 6:
            m_Remainder = m_LtcId;
            m_Quotient = ( ( (m_Padr >> (s_Gmlit2Obit9LocInPadr + 1)) << s_Gmlit2Obit9LocInPadr) |    (m_Padr & ((1 << s_Gmlit2Obit9LocInPadr) - 1)) ) >> 2;
            m_Offset = (bit<0>(m_Slice)<<10) |
                (bit<s_Gmlit2Obit9LocInPadr>(m_Padr)<<9) |
                (bits<1, 0>(m_Padr) << 7) |
                (hacksector << 5) |
                (m_L2Offset & 31);
            break;
        case 3:
            m_Remainder = m_LtcId;
            m_Quotient = m_Padr >> 4;
            m_Offset = ((bit<3>(m_Padr))<< 10) |
                (bit<2>(m_Padr) << 9) |
                (bits<1, 0>(m_Padr) << 7) |
                (hacksector << 5) |
                (m_L2Offset & 31);
            break;
        case 2:
            m_Remainder = m_LtcId;
            m_Quotient = m_Padr >> 3;
            m_Offset = ((bit<2>(m_Padr))<< 10) |
                (m_Slice << 9) |
                (bits<1, 0>(m_Padr) << 7) |
                (hacksector << 5) |
                (m_L2Offset & 31);
            break;
        default:
            m_Remainder = m_LtcId;
            m_Quotient = m_Padr >> 2;
            m_Offset = (m_Slice<<9) |
                (bits<1, 0>(m_Padr) << 7) |
                (hacksector << 5) |
                (m_L2Offset & 31);
            break;
    }

    // Generate PAKS from q, r, o
    m_PAKS = (m_Quotient * m_NumPartitions + m_Remainder) * m_PartitionInterleave | m_Offset;

}

//------------------------------------------------------------------------------
UINT64 GMLIT2AddressEncode::CommonCalcPAKS(UINT64 paValue, UINT32 numLTSPerLTC, UINT32 fbps)
{
    const int numSwizzleBits = 30;
    UINT08 galoisPartition[][30] = {
        { 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1},
        { 2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2, 3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3,  2,  3},
        { 7,  7,  6,  7,  6,  7,  7,  6,  7,  6,  7,  6,  7,  6,  7, 7,  6,  7,  6,  7,  6,  7,  6,  7,  7,  6,  7,  6,  7,  6},
        { 6,  5,  7,  6,  3,  2,  7,  6,  3,  2,  5,  3,  2,  7,  6, 3,  2,  7,  6,  5,  7,  6,  3,  2,  7,  6,  5,  2,  3,  6},
        {13,  6,  9, 14, 11, 13,  7,  2,  9,  6,  5, 11,  9, 10,  7, 5,  3,  9,  7,  2, 13,  3,  9,  6, 13,  7,  2,  9, 10, 13},
        { 9,  6,  5, 14,  7, 13,  6,  9,  3, 14,  5, 10,  3, 14,  5, 7, 10,  9,  2, 13,  3,  6,  9,  7, 13, 11,  2,  5, 10,  3},
        { 5, 15, 14,  7,  5, 15,  6,  1, 11,  9, 14,  1,  2,  9, 15, 1,  2,  7,  9, 10,  1, 14,  7, 10, 11,  5,  2, 15,  1,  6},
        {17, 26, 13, 15, 18,  3, 22, 19, 13,  2, 23,  5, 27, 17, 19, 6,  3, 25, 23, 30, 17, 27, 13,  3,  5, 10, 25, 27, 21,  7}};
    UINT08  galoisIdx[] = {0, 1, 2, 3, 3, 4, 5, 6, 7, 7, 7, 7};
    UINT08  upperAddrShift[] = {16, 16, 18, 17, 17, 17, 18, 17, 18, 18, 18, 18};
    UINT32 swizzle;
    UINT64 temp = 0;

    swizzle = GenerateSwizzle(galoisPartition[galoisIdx[fbps - 1]], numSwizzleBits, paValue >> upperAddrShift[fbps - 1]);

    switch (fbps)
    {
        case 1:
            temp =
                (bits<PA_MSB, 15>(paValue) << 15) |
                (bit<         10>(paValue) << 14) |
                (bits<    14, 11>(paValue) << 10);
            break;
        case 2:
            temp =
                ( bits<PA_MSB, 15>(paValue) << 15) |
                ( bit<         10>(paValue) << 14) |
                ( bits<    14, 12>(paValue) << 11) |
                ((bit<11>(paValue) ^ bit<1>(swizzle) ^ bit<14>(paValue) ^ bit<15>(paValue)) << 10);
            break;
        case 3:
            temp =
                ( bits<PA_MSB, 15>(paValue) << 15) |
                ( bit<         10>(paValue) << 14) |
                ( bit<         14>(paValue) << 13) |
                ( bit<         13>(paValue) << 12) |
                ((bit<12>(paValue) ^ bit<2>(swizzle)) << 11) |
                ((bit<11>(paValue) ^ bit<1>(swizzle)) << 10);
            break;
        case 4:
            temp =
                ( bits<PA_MSB, 15>(paValue) << 15) |
                ( bit<         10>(paValue) << 14) |
                ( bit<         14>(paValue) << 13) |
                ( bit<         13>(paValue) << 12) |
                ((bit<12>(paValue) ^ bit<2>(swizzle) ^ bit<14>(paValue)) << 11) |
                ((bit<11>(paValue) ^ bit<1>(swizzle) ^ bit<14>(paValue) ^ bit<15>(paValue)) << 10);
            break;
        case 5:
            temp =
                ( bits<PA_MSB, 15>(paValue) << 15) |
                ( bit<         10>(paValue) << 14) |
                ( bit<         14>(paValue) << 13) |
                ((bit<13>(paValue) ^ bit<3>(swizzle)) << 12) |
                ((bit<12>(paValue) ^ bit<2>(swizzle)) << 11) |
                ((bit<11>(paValue) ^ bit<1>(swizzle)) << 10);
            break;
        case 6:
            temp =
                (bits<PA_MSB, 15>(paValue) << 15) |
                ( bit<        10>(paValue) << 14) |
                ( bit<        14>(paValue) << 13) |
                ((bit<13>(paValue) ^ bit<3>(swizzle) ^ bit<14>(paValue)) << 12) |
                ((bit<12>(paValue) ^ bit<2>(swizzle) ^ bit<14>(paValue)) << 11) |
                ((bit<11>(paValue) ^ bit<1>(swizzle) ^ bit<14>(paValue)) << 10);
            break;
        case 7:
            temp =
                ( bits<PA_MSB, 15>(paValue) << 15) |
                ( bit<         10>(paValue) << 14) |
                ( bit<         14>(paValue) << 13) |
                ((bit<13>(paValue) ^ bit<3>(swizzle)) << 12) |
                ((bit<12>(paValue) ^ bit<2>(swizzle)) << 11) |
                ((bit<11>(paValue) ^ bit<1>(swizzle)) << 10);
            break;
        case 8:
            temp =
                ( bits<PA_MSB, 15>(paValue) << 15) |
                ( bit<         10>(paValue) << 14) |
                ( bit<         14>(paValue) << 13) |
                ((bit<13>(paValue) ^ bit<3>(swizzle) ^ bit<14>(paValue)) << 12) |
                ((bit<12>(paValue) ^ bit<2>(swizzle) ^ bit<15>(paValue)) << 11) |
                ((bit<11>(paValue) ^ bit<1>(swizzle) ^ bit<14>(paValue) ^ bit<16>(paValue)) << 10);
            break;
        case 9:
            temp =
                ( bits<PA_MSB, 15>(paValue) << 15) |
                ( bit<         10>(paValue) << 14) |
                ((bit<14>(paValue) ^ bit<4>(swizzle)) << 13) |
                ((bit<13>(paValue) ^ bit<3>(swizzle) ^ bit<14>(paValue) ^ bit<4>(swizzle)) << 12) |
                ((bit<12>(paValue) ^ bit<2>(swizzle)) << 11) |
                ((bit<11>(paValue) ^ bit<1>(swizzle)) << 10);
            break;
        case 10:
            temp =
                (bits<PA_MSB, 15>(paValue) << 15) |
                ( bit<        10>(paValue) << 14) |
                ((bit<14>(paValue) ^ bit<4>(swizzle)) << 13) |
                ((bit<13>(paValue) ^ bit<3>(swizzle)) << 12) |
                ((bit<12>(paValue) ^ bit<2>(swizzle)) << 11) |
                ((bit<11>(paValue) ^ bit<1>(swizzle) ^ bit<14>(paValue) ^ bit<4>(swizzle)) << 10);
            break;
        case 11:
            temp =
                ((bits<PA_MSB, 15>(paValue) ^ bit<14>(paValue) ^ bit<4>(swizzle)) << 15) |
                ( bit<10>(paValue) << 14) |
                ((bit<14>(paValue) ^ bit<4>(swizzle)) << 13) |
                ((bit<13>(paValue) ^ bit<3>(swizzle)) << 12) |
                ((bit<12>(paValue) ^ bit<2>(swizzle)) << 11) |
                ((bit<11>(paValue) ^ bit<1>(swizzle)) << 10);
            break;
        case 12:
            temp =
                (bits<PA_MSB, 15>(paValue) << 15) |
                ( bit<        10>(paValue) << 14) |
                ((bit<14>(paValue) ^ bit<4>(swizzle)) << 13) |
                ((bit<13>(paValue) ^ bit<3>(swizzle)) << 12) |
                ((bit<12>(paValue) ^ bit<2>(swizzle) ^ bit<16>(paValue)) << 11) |
                ((bit<11>(paValue) ^ bit<1>(swizzle) ^ bit<14>(paValue) ^ bit<4>(swizzle)) << 10);
            break;
    }

    temp |= (((numLTSPerLTC == 6) ? bit<9>(paValue) :
              (bit<9>(paValue) ^ bit<0>(swizzle) ^ bit<10>(paValue))) << 9) |
            bits<8, 0>(paValue);
    return temp;
}

//------------------------------------------------------------------------------
UINT64 GMLIT2AddressEncode :: PaksLowerPitchLinearPart8(UINT64 PA_value)
{
    UINT64 temp = (bits<PA_MSB, 10>(PA_value) << 10) |
                   (bit<8>(PA_value) << 9) |
                   (bit<9>(PA_value) << 8) |
                   bits<7, 0>(PA_value);
    return temp;
}

//------------------------------------------------------------------------------
UINT64 GMLIT2AddressEncode::EncodeAddress
(
    const FbDecode& decode,
    UINT32 pteKind,
    UINT32 pageSize,
    bool eccOn
)
{
    const FrameBuffer *pFb = m_pGpuSubdevice->GetFB();
    const UINT32 rowOffset = pFb->GetRowOffset(decode.burst, decode.beat,
                                               decode.beatOffset);
    m_Row = decode.row;
    m_Col = rowOffset / m_AmapColumnSize;
    m_Bank = decode.bank;
    m_ExtBank = decode.rank;
    m_SubPart = decode.subpartition;
    m_Partition = pFb->VirtualFbioToVirtualFbpa(decode.virtualFbio);
    m_ECCOn = eccOn;

    m_PteKind = pteKind;
    m_PageSize = pageSize;

    // determine ltcid
    UINT32 ltc = 0;
    for (UINT32 i = 0; i < m_Partition; i++)
    {
        ltc += Utility::CountBits(m_LtcMaskForLFbp[i]);
    }
    ltc += m_SubPart;
    m_LtcId = ltc;

    MapRBCToXBar();
    MapXBarToPAKS();

    m_PhysAddr = PaksLowerPitchLinearPart8( CommonCalcPAKS(m_PAKS, m_NumXbarL2Slices, m_NumPartitions) );

    // 0 out the lower 12 bits since we just need the page offset.
    m_PhysAddr = maskoff_lower_bits<12>(m_PhysAddr);

    Printf(Tee::PriLow, "GMLIT2 Address Encode 0x%llx, kind 0x%02x, page %uKB: "
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
UINT64 GMLIT2AddressEncode :: ReverseEccAddr()
{
    UINT64 upper_padr = ((m_Fb_Padr / m_LtcLinesPerPage) *
                         static_cast<UINT64>(m_EccLinesPerPage));
    UINT64 lower_padr = (m_Fb_Padr % m_LtcLinesPerPage);
    return upper_padr + lower_padr;
}

//------------------------------------------------------------------------------
UINT32 GMLIT2AddressEncode :: GenerateSwizzle(UINT08* galois, UINT32 numSwizzleBits, UINT64 address)
{
    UINT32 swizzle = 0;
    for (UINT32 i = 0; i < numSwizzleBits; ++i)
    {
        swizzle ^= (((UINT32)((address >> i) & 1)) * galois[i]);
    }
    return swizzle;
}
