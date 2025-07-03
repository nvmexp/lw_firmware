/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Refer to these file have been copied from:
// //hw/class/kepler/perfsim/code/perfsim/units/AddrMap/gk208.h
// //hw/class/kepler/perfsim/code/perfsim/units/AddrMap/gklit4.h
// //hw/class/kepler/perfsim/code/perfsim/units/AddrMap/gklit1.h
//

#include "gk208adr.h"
#include "kepler/gk208/dev_mmu.h"
#include "kepler/gk208/dev_fbpa.h"
#include "kepler/gk208/dev_ltc.h"

#include "bitop.h"

using namespace BitOp;

static const bool SliceLowerBit = false;
#define PA_MSB 39

GK208AddressDecode::GK208AddressDecode
(
    UINT32 NumPartitions,
    GpuSubdevice* pSubdev
)
: GKLIT2AddressDecode(NumPartitions, pSubdev)
{
    m_NumSubPartitions = pSubdev->GetFB()->GetSubpartitions();
    m_NumXbarL2Slices =
        DRF_VAL(_PLTCG, _LTCS_LTSS_CBC_PARAM, _SLICES_PER_FBP,
        pSubdev->RegRd32(LW_PLTCG_LTCS_LTSS_CBC_PARAM));
    m_DRAMBankSwizzle = 0 !=
        DRF_VAL(_PFB, _FBPA_BANK_SWIZZLE, _ENABLE,
        pSubdev->RegRd32(LW_PFB_FBPA_BANK_SWIZZLE));
    m_ECCOn = 0 !=
        DRF_VAL(_PFB, _FBPA_ECC_CONTROL, _MASTER_EN,
        pSubdev->RegRd32(LW_PFB_FBPA_ECC_CONTROL));
    //GK208 property
    m_NumL2Sets = 128;
    m_NumL2SetsBits = 7;
}

GK208AddressDecode::~GK208AddressDecode()
{
}

void GK208AddressDecode::CommonCalcPAKSBlockLinearPS64KParts2()
{
    UINT32 swizzle_ps = static_cast<UINT32>((bit<20>(m_PhysAddr) <<1) + (bit<19>(m_PhysAddr) <<2) + bits<23,21>(m_PhysAddr) + bits<26,24>(m_PhysAddr)
        + bits<29,27>(m_PhysAddr) + bits<32,30>(m_PhysAddr) + bits<35,33>(m_PhysAddr) + bit<36>(m_PhysAddr));

    m_PAKS = (bits<PA_MSB,12>(m_PhysAddr) << 12) |
        // slice1
        ((bit<11>(m_PhysAddr) ^ bit<10>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ bit<14>(m_PhysAddr) ^  bit<1>(swizzle_ps)) << 11) |
        // part
        ((bit<10>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ bit<0>(swizzle_ps)) << 10) |
        // slice0
        ((bit<9>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ bit<15>(m_PhysAddr) ^ bit<2>(swizzle_ps)) << 9) |
        bits<8,0>(m_PhysAddr);
}

void GK208AddressDecode::CommonCalcPAKSBlockLinearPS64KParts1()
{
    UINT32  swizzle = static_cast<UINT32>(bits<14,13>(m_PhysAddr) + bits<16,15>(m_PhysAddr));
    m_PAKS = (bits<PA_MSB,11>(m_PhysAddr) << 11) |
        // don't want diagonals
        // using ^ here so that carry doesn't propagate into bit10 from changing bit9, allows bit flipping in ROP
        ((bit<10>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<9>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |
        bits<8,0>(m_PhysAddr);
}

void GK208AddressDecode::ComputeL2Address()
{
    switch (m_NumL2Sets)
    {
    case 16:
        CalcL2SetSlices4L2Set16();
        break;
    case 32:
        CalcL2SetSlices4L2Set32();
        break;
    case 128:
        CalcL2SetSlices4L2Set128();
        break;
    default:
        Printf(Tee::PriHigh, "DecodeAddress: "
            "Unsupported L2 sets number - 0x%x!\n",
            m_NumL2Sets);
        MASSERT(!"Unsupported L2 sets number!");
        break;
    }
}

void GK208AddressDecode::CalcL2SetSlices4L2Set32()
{
    UINT32 common;
     common = static_cast<UINT32>(bits<3,1>(m_Quotient) + bits<8,6>(m_Quotient)
         + bits<13,11>(m_Quotient) + bits<17,16>(m_Quotient)
         + ((bit<4>(m_Quotient) ^ bit<5>(m_Quotient) ^ bit<9>(m_Quotient)
                     ^ bit<10>(m_Quotient) ^ bit<14>(m_Quotient)
                     ^ bit<15>(m_Quotient)) << 2));
    if (SliceLowerBit == true)
    {
        m_L2Set =  static_cast<UINT32>((bit<7>(m_Offset) ^ bit<3>(m_Quotient) ^  bit<8>(m_Quotient) ^ bit<13>(m_Quotient)) |
            ((bit<8>(m_Offset) ^ bit<4>(m_Quotient) ^  bit<9>(m_Quotient) ^ bit<14>(m_Quotient)) << 1) |
            ((bit<0>(m_Quotient) ^ bit<5>(m_Quotient) ^ bit<10>(m_Quotient) ^ bit<15>(m_Quotient)) << 2) |
            (bits<1,0>(common) << 3));
    }
    else
    {
        m_L2Set = static_cast<UINT32>((bit<7>(m_Offset) ^ bit<3>(m_Quotient) ^  bit<8>(m_Quotient) ^ bit<13>(m_Quotient)) |
            ((bit<8>(m_Offset) ^ bit<4>(m_Quotient) ^  bit<9>(m_Quotient) ^ bit<14>(m_Quotient)) << 1) |
            ((bit<9>(m_Offset) ^ bit<5>(m_Quotient) ^ bit<10>(m_Quotient) ^ bit<15>(m_Quotient)) << 2) |
            (bits<1,0>(common) << 3));
    }
    m_L2Tag = static_cast<UINT32>(bit<2>(common) | (bits<31,4>(m_Quotient) << 1));
    CalcL2Sector();
}

void GK208AddressDecode::CalcL2SetSlices4L2Set128()
{
      UINT32 common;
      // 128 set mapping based on 32 set mapping
      common = static_cast<UINT32>(bits<3,1>(m_Quotient) + bits<8,6>(m_Quotient) + bits<13,11>(m_Quotient) + bits<17,16>(m_Quotient)
          + ((bit<4>(m_Quotient) ^ bit<5>(m_Quotient) ^ bit<9>(m_Quotient) ^ bit<10>(m_Quotient) ^ bit<14>(m_Quotient) ^ bit<15>(m_Quotient)) << 2));
      m_L2Set =  static_cast<UINT32>((bit<7>(m_Offset) ^ bit<3>(m_Quotient) ^  bit<8>(m_Quotient) ^ bit<13>(m_Quotient)) |
          ((bit<8>(m_Offset) ^ bit<4>(m_Quotient) ^  bit<9>(m_Quotient) ^ bit<14>(m_Quotient)) << 1) |
          ((bit<9>(m_Offset) ^ bit<5>(m_Quotient) ^ bit<10>(m_Quotient) ^ bit<15>(m_Quotient)) << 2) |
          (bits<2,0>(common) << 3) |
          (bit<4>(m_Quotient) << 6));          // I extend to 128 sets by attaching two more bits from m_Quotient
      m_L2Tag = static_cast<UINT32>((bits<31,5>(m_Quotient)));     // The two bits that are given to the set are removed from the tag.
      CalcL2Sector();
}

void GK208AddressDecode::CommonCalcL2AddressParts421()
{
    // swizzle bank and upper 2 bits L2 index
    UINT32 swizzle_20 = static_cast<UINT32>(bits<6,4>(m_Quotient) + bits<9,7>(m_Quotient) + bits<12,10>(m_Quotient) + bits<15,13>(m_Quotient) + bits<18,16>(m_Quotient) + bits<21,19>(m_Quotient) + bits<24,22>(m_Quotient) + bits<27,25>(m_Quotient));
    // rotates set of 4 L2 lines in a partition tile
    UINT32 swizzle_43 = static_cast<UINT32>(bit<3>(m_Quotient) + bit<9>(m_Quotient) + bit<15>(m_Quotient) + bit<21>(m_Quotient) + bit<27>(m_Quotient) + ((bit<6>(m_Quotient) + bit<12>(m_Quotient) + bit<18>(m_Quotient) + bit<24>(m_Quotient)) << 1));
    if(((m_MappingMode & 0x80) == 0x80) && (m_NumPartitions == 1))
    { // changing the slice stride to 1K for gk107
        UINT32 bank = static_cast<UINT32>(bits<3,2>(m_Quotient) + bits<2,1>(swizzle_20));
        m_L2Set = static_cast<UINT32>((bit<7>(m_Offset) ^ bit<1>(swizzle_43)) |
            ((bit<8>(m_Offset) ^ bit<0>(swizzle_43)) << 1) |
            ((bit<1>(m_Quotient) ^ bit<0>(swizzle_20)) << 2) |
            (bit<0>(bank) << 3));
        m_L2Tag = static_cast<UINT32>(bit<1>(bank) | ((m_Quotient >> 4) << 1));
        CalcL2Sector();
    }
    else
    {
        UINT32 bank = static_cast<UINT32>(bits<3,1>(m_Quotient) + bits<2,0>(swizzle_20));
        m_L2Set = (bit<7>(m_Offset) ^ bit<1>(swizzle_43)) |
            ((bit<8>(m_Offset) ^ bit<0>(swizzle_43)) << 1) |
            (bits<1,0>(bank) << 2);
        m_L2Tag = static_cast<UINT32>(bit<2>(bank) | ((m_Quotient >> 4) << 1));
        CalcL2Sector();
    }
}

bool GK208AddressDecode::RBSCCommon()
{
    //m_Padr = (m_L2Tag << m_NumL2SetsBits) | m_L2Set;
    m_Padr = (m_L2Tag << 7) | m_L2Set;
    if (m_NumXbarL2Slices != 2)
    {
        Printf(Tee::PriLow, "Unsupported number of xbar/L2 slices [%u] \n",
                m_NumXbarL2Slices);
        return true;
    }
    if(m_DRAMBankSwizzle)
    {
        //swizzling 1st part
        m_Padr = (m_Padr & ~0x38) | ((bits<2,0>(bits<5,3>(m_Padr) - 3*bits<8,6>(m_Padr))) << 3);
    }
    if (m_ECCOn)
    {
        // m_Padr = (m_Padr / 7) * 8 | (m_Padr % 7);
        m_Padr = ComputeECCAddr();
    }
    // Bug 1540967: GK208s has a sku with one subpartition disabled, but for a few addresses
    // the numbers still work out wrong. This forces m_Subpart to be a valid value.
    m_SubPart = m_NumSubPartitions > 1 ? bit<0>(m_Slice) : 0;
    UINT32 rb=bits<31,3>(m_Padr); // same as (m_l2tag << 2) | bits<4,3>(m_l2set);
      //UINT32 tmp = rb % (m_number_dram_banks_fbp[m_partition] << m_exists_extbank_fbp[m_partition])
    UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
    m_Bank = tmp % m_NumDRAMBanks;
    m_ExtBank = tmp / m_NumDRAMBanks;
    m_Row = (rb / (m_NumDRAMBanks << m_NumExtDRAMBanks)); // includes "column-as-row" bits

      m_Col = ((m_Row << 8) | (bits<2,0>(m_Padr) << 5) | bits<6,2>(m_L2Offset)) & ((1 << ComputeColumnBits()) - 1);
      m_Row = m_Row >> (ComputeColumnBits() - 8);

      if(m_NumDRAMBanks==8 && m_MappingMode == 0x6)           //overwrite
        {

          //UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);
          m_ExtBank = bit<3>(m_Padr) & (m_NumExtDRAMBanks)  ; //tmp / m_NumDRAMBanks;

          m_Bank = (bit<3>(m_Padr) ^ bit<4>(m_Padr)) << 0 |
                          bit<5>(m_Padr) << 1 |
                          bit<6>(m_Padr) << 2 ;

          m_Row = (rb / 16);

          m_Col = ((m_Row << 8) | (bits<2,0>(m_Padr) << 5) | bits<6,2>(m_L2Offset)) & ((1 << ComputeColumnBits()) - 1);
          m_Row = m_Row >> (ComputeColumnBits() - 8);

          if(!m_NumExtDRAMBanks){
            m_Row = m_Row << 1 | bit<3>(m_Padr);
          }
        }

    return false;
}

bool GK208AddressDecode::CalcRBCSlices4L2Sets16DRAMBanks4Bool()
{
    if (RBSCCommon())
    {
        return false;
    }
    return true;
}

bool GK208AddressDecode::CalcRBCSlices4L2Sets16DRAMBanks8Bool()
{
    if (RBSCCommon())
    {
        return false;
    }
    // swizzle banks for bank grouping
    m_Bank =  bit<2>(m_Bank) |
        (bit<1>(m_Bank) << 1) |
        (bit<0>(m_Bank) << 2);
    return true;
}

/*virtual*/ bool GK208AddressDecode::CalcRBCSlices4L2Sets16DRAMBanks16Bool()
{
    if (RBSCCommon())
    {
        return false;
    }

    // FT swizzles the bank bits
    m_Bank = (bit<3>(m_Bank) << 3) |
        (bit<0>(m_Bank) << 2) |
        (bit<1>(m_Bank) << 1) |
        (bit<2>(m_Bank) << 0);
    // 16 bank swizzle
    m_Bank = (bit<2>(m_Bank) << 3) |
        (bit<1>(m_Bank) << 2) |
        (bit<3>(m_Bank) << 1) |
        (bit<0>(m_Bank) << 0);

    // 8x2 bank grouping
    if(m_8BgSwizzle)
    {
        m_Bank = (bit<3>(m_Bank) << 3) |
            (bit<2>(m_Bank) << 2) |
            (bit<0>(m_Bank) << 1) |
            (bit<1>(m_Bank) << 0);
    }

    return true;
}
