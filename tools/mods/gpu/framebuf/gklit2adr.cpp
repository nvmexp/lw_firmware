/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2017,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// The contents of this file have been copied from:
// //hw/kepler1_gk100/clib/lwshared/Memory/amap.h#13
// //hw/kepler1_gk100/clib/lwshared/Memory/gklit1.h#9
// //hw/kepler1_gk100/clib/lwshared/Memory/gklit2_chip.h#5
//
// The address mapping is dolwmented here (but I doubt it is out of date):
// //hw/doc/gpu/kepler/kepler/design/Functional_Descriptions/kepler_address_mapping.doc

#include "gklit2adr.h"
#include "bitop.h"
#include "kepler/gk110/dev_mmu.h"
#include "kepler/gk110/dev_fbpa.h"
#include "kepler/gk110/dev_ltc.h"

using namespace BitOp;

#define PA_MSB 39

GKLIT2AddressDecode::GKLIT2AddressDecode
(
    UINT32 NumPartitions,
    GpuSubdevice* pSubdev
)
: GKLIT1AddressDecode(NumPartitions, pSubdev)
{
    m_NumXbarL2Slices =
        DRF_VAL(_PLTCG, _LTCS_LTSS_CBC_PARAM, _SLICES_PER_FBP,
        pSubdev->RegRd32(LW_PLTCG_LTCS_LTSS_CBC_PARAM));
    m_DRAMBankSwizzle = 0 !=
        DRF_VAL(_PFB, _FBPA_BANK_SWIZZLE, _ENABLE,
        pSubdev->RegRd32(LW_PFB_FBPA_BANK_SWIZZLE));
    m_ECCOn = 0 !=
        DRF_VAL(_PFB, _FBPA_ECC_CONTROL, _MASTER_EN,
        pSubdev->RegRd32(LW_PFB_FBPA_ECC_CONTROL));
}

GKLIT2AddressDecode::~GKLIT2AddressDecode()
{
}

void GKLIT2AddressDecode::ComputePAKS()
{
    m_PAKS = 0;
    switch (m_PteKind)
    {
    case LW_MMU_PTE_KIND_GENERIC_16BX2:
    // case FermiPhysicalAddress::BLOCKLINEAR_Z64:
        CommonCalcPAKSBlockLinear();
        break;

    case LW_MMU_PTE_KIND_PITCH:
        CommonCalcPAKSPitchLinear();
        break;

    case LW_MMU_PTE_KIND_PITCH_NO_SWIZZLE:
        CommonCalcPAKSPitchNoSwizzle();
        break;

    default:
        Printf(Tee::PriError, "DecodeAddress: "
            "Unsupported PTE kind - 0x%02x!\n",
            m_NumPartitions);
        MASSERT(!"Unsupported PTE kind!");
        break;
    }
}

void GKLIT2AddressDecode::ComputeSlice()
{
    // Looks like slice can only be 4
    // See gklit2_chip.h
    switch (m_NumPartitions)
    {
    case 6:
        CalcSliceSlices4Parts6();
        break;

    case 5:
        CommonCalcSliceParts3();
        break;

    default:
        DefaultSlice();
        break;
    }
}

void GKLIT2AddressDecode::ComputePartition()
{
    switch (m_NumPartitions)
    {
    case 6:
        CalcPartParts6();
        break;

    case 5:
        m_Partition = m_Remainder;
        break;

    default:
        DefaultPart();
        break;
    }
}

void GKLIT2AddressDecode::ComputeL2Address()
{
    switch (m_NumL2Sets)
    {
    case 16:
        CalcL2SetSlices4L2Set16();
        break;

    case 32:
        CalcL2SetSlices4L2Set32();
        break;

    default:
        Printf(Tee::PriError, "DecodeAddress: "
            "Unsupported L2 sets number - 0x%x!\n",
            m_NumL2Sets);
        MASSERT(!"Unsupported L2 sets number!");
        break;
    }
}

void GKLIT2AddressDecode::CommonCalcPAKSBlockLinear()
{
    switch (m_PageSize)
    {
    case 4:
        switch (m_NumPartitions)
        {
        case 6:
            CalcPAKSBlockLinearPS4KParts6();
            break;

        case 5:
            CalcPAKSBlockLinearPS4KParts5();
            break;

        default:
            DefaultPAKS();
            break;

        }
        break;
    case 64:
    case 128:
        switch (m_NumPartitions)
        {
        case 6:
            switch ((m_MappingMode >> 12) & 0x3)
            {
            case 1:
                CalcPAKSBlockLinearPS64KParts6GK110();
                break;

            default:
                CalcPAKSBlockLinearPS64KParts6GF100();
                break;

            }
            break;
        case 5:
            switch ((m_MappingMode >> 12) & 0x3)
            {
            case 1:
                CalcPAKSBlockLinearPS64KParts5GK110();
                break;

            default:
                CalcPAKSBlockLinearPS64KParts5GF100();
                break;

            }
            break;
        default:
            DefaultPAKS();
            break;
        }
        break;
    default:
        Printf(Tee::PriError, "DecodeAddress: "
            "Unsupported page size - 0x%x!\n",
            m_PageSize);
        MASSERT(!"Unsupported page size!");
        break;
    }
}

void GKLIT2AddressDecode::CommonCalcPAKSPitchLinear()
{
    switch (m_PageSize)
    {
    case 4:
        switch (m_NumPartitions)
        {
        case 6:
            CalcPAKSPitchLinearPS4KParts6();
            break;

        case 5:
            CalcPAKSPitchLinearPS4KParts5();
            break;

        default:
            DefaultPAKS();
            break;
        }
        break;

    case 64:
    case 128:
        switch (m_NumPartitions)
        {
        case 6:
            CalcPAKSPitchLinearPS64KParts6();
            break;

        case 5:
            CalcPAKSPitchLinearPS64KParts5();
            break;

        default:
            DefaultPAKS();
            break;

        }
        break;
    default:
        Printf(Tee::PriError, "DecodeAddress: "
            "Unsupported page size - 0x%x!\n",
            m_PageSize);
        MASSERT(!"Unsupported page size!");
        break;
    }
}

void GKLIT2AddressDecode::CommonCalcPAKSPitchNoSwizzle()
{
    switch (m_PageSize)
    {
    case 4:
        switch (m_NumPartitions)
        {
        case 6:
            m_PAKS = m_PhysAddr;
            break;

        case 5:
            CalcPAKSCommonPS4KParts5();
            break;

        default:
            DefaultPAKS();
            break;

        }
        break;

    case 64:
    case 128:
        switch (m_NumPartitions)
        {
        case 6:
        case 5:
            m_PAKS = m_PhysAddr;
            break;

        default:
            DefaultPAKS();
            break;

        }
        break;
    default:
        Printf(Tee::PriError, "DecodeAddress: "
            "Unsupported page size - 0x%x!\n",
            m_PageSize);
        MASSERT(!"Unsupported page size!");
        break;
    }
}

void GKLIT2AddressDecode::CalcPAKSPitchLinearPS4KParts6()
{
    m_PAKS = (bits<PA_MSB, 12>(m_PhysAddr) << 12) |
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

void GKLIT2AddressDecode::CalcPAKSPitchLinearPS4KParts5()
{
    UINT64 swizzle = bits<13, 12>(m_PhysAddr) + bits<15, 14>(m_PhysAddr) +  bits<17, 16>(m_PhysAddr) +
        bits<19, 18>(m_PhysAddr) + bits<21, 20>(m_PhysAddr) +  bits<23, 22>(m_PhysAddr) +
        bits<25, 24>(m_PhysAddr) + bits<27, 26>(m_PhysAddr) +  bits<29, 28>(m_PhysAddr) +
        bits<31, 30>(m_PhysAddr);

    m_PALev1 = (bits<PA_MSB, 12>(m_PhysAddr) << 12) |

        // 4KB portion
        // partition[0]
        (bit<9>(m_PhysAddr) << 11) |

        // slice[1:0]
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        (bit<11>(m_PhysAddr) << 8) |
        (bit<10>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);

    CalcPAKSCommonPS4KParts5();
}

void GKLIT2AddressDecode::CalcPAKSBlockLinearPS4KParts6()
{
    m_PAKS = (bits<PA_MSB, 14>(m_PhysAddr) << 14) |
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

void GKLIT2AddressDecode::CalcPAKSBlockLinearPS4KParts5()
{
    // The constraint is to only swizzle 4KB of addressing.
    // As long as I don't swizzle outside 64KB, and I use the same m_PALev1->PAKS
    // for both pitch and blocklinear, 4KB pages work.

    m_PALev1 = (bits<PA_MSB, 12>(m_PhysAddr) << 12) |

        // 4KB
        (bit<11>(m_PhysAddr) << 11) |
        // swizzle slices
        ((bit<10>(m_PhysAddr) ^ bit<13>(m_PhysAddr)) << 10) |
        ((bit<9>(m_PhysAddr) ^ bit<14>(m_PhysAddr)) << 9) |
        bits<8, 0>(m_PhysAddr);

    CalcPAKSCommonPS4KParts5();
}

void GKLIT2AddressDecode::CalcPAKSCommonPS4KParts5()
{
    // This section must be identical for pitch and block linear 5 partitions

    m_PAKS = (bits<PA_MSB, 14>(m_PALev1) << 14) |
        (bit<13>(m_PALev1)  << 13) |
        ((bit<12>(m_PALev1) ^ bit<10>(m_PALev1)) << 12) |
        bits<11, 0>(m_PALev1);  // inside 4KB page

}

void GKLIT2AddressDecode::CalcPAKSPitchLinearPS64KParts6()
{
    m_PAKS = (bits<PA_MSB, 16>(m_PhysAddr) << 16) |
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

void GKLIT2AddressDecode::CalcPAKSPitchLinearPS64KParts5()
{
    UINT64 swizzle = bits<13, 12>(m_PhysAddr) + bits<15, 14>(m_PhysAddr) +  bits<17, 16>(m_PhysAddr) +
        bits<19, 18>(m_PhysAddr) + bits<21, 20>(m_PhysAddr) +  bits<23, 22>(m_PhysAddr) +
        bits<25, 24>(m_PhysAddr) + bits<27, 26>(m_PhysAddr) +  bits<29, 28>(m_PhysAddr) +
        bits<31, 30>(m_PhysAddr);

    m_PAKS = (bits<PA_MSB, 16>(m_PhysAddr) << 16) |
        (bits<13, 12>(m_PhysAddr) << 14) |
        // partitions
        (bits<11, 9>(m_PhysAddr) << 11) |

        // slice[1:0]
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        (bits<15, 14>(m_PhysAddr) << 7) |
        bits<6, 0>(m_PhysAddr);
}

void GKLIT2AddressDecode::CalcPAKSBlockLinearPS64KParts6GF100()
{
    m_PAKS = (bits<PA_MSB, 14>(m_PhysAddr) << 14) |
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

void GKLIT2AddressDecode::CalcPAKSBlockLinearPS64KParts6GK110()
{
    UINT64 swizzle_ps=0;
    swizzle_ps = bit<16>(m_PhysAddr) ^ bit<17>(m_PhysAddr) ^ bit<18>(m_PhysAddr) ^ bit<19>(m_PhysAddr) ^ bit<20>(m_PhysAddr) ^
        bit<21>(m_PhysAddr) ^ bit<22>(m_PhysAddr) ^ bit<23>(m_PhysAddr) ^ bit<24>(m_PhysAddr) ^ bit<25>(m_PhysAddr) ^ bit<26>(m_PhysAddr) ^
        bit<27>(m_PhysAddr) ^ bit<28>(m_PhysAddr) ^ bit<29>(m_PhysAddr) ^ bit<30>(m_PhysAddr) ^ bit<31>(m_PhysAddr);
    m_PAKS = (bits<PA_MSB, 10>(m_PhysAddr) << 10) |

        // part[0], power of 2 bit
        //      ((bit<10>(m_PhysAddr) ) << 10) | // post divide R swizzle

        // slice[0]
        ((bit<9>(m_PhysAddr)  ^ bit<13>(m_PhysAddr) ^ swizzle_ps) << 9) |
        bits<8, 0>(m_PhysAddr);
}

void GKLIT2AddressDecode::CalcPAKSBlockLinearPS64KParts5GF100()
{
    m_PAKS = (bits<PA_MSB, 16>(m_PhysAddr) << 16) |
        (bits<14, 13>(m_PhysAddr) << 14) |
        ((~bit<12>(m_PhysAddr)&1 ) << 13) |
        ((bit<11>(m_PhysAddr)&1 ) << 12) |
        ((bit<10>(m_PhysAddr)&1 ) << 11) |
        // swizzle slices
        ((bit<15>(m_PhysAddr) ^ bit<10>(m_PhysAddr)  ^ bit<14>(m_PhysAddr) ) << 10) |
        ((bit<9>(m_PhysAddr) ^ bit<13>(m_PhysAddr)) << 9) |
        bits<8, 0>(m_PhysAddr);
}

void GKLIT2AddressDecode::CalcPAKSBlockLinearPS64KParts5GK110()
{
    UINT64 swizzle_ps=0;
    swizzle_ps = bits<17, 16>(m_PhysAddr) + bits<19, 18>(m_PhysAddr) + bits<21, 20>(m_PhysAddr) + bits<23, 22>(m_PhysAddr) +
        bits<25, 24>(m_PhysAddr) + bits<27, 26>(m_PhysAddr) + bits<29, 28>(m_PhysAddr) + bits<31, 30>(m_PhysAddr);

    m_PAKS = (bits<PA_MSB, 16>(m_PhysAddr) << 16) |
        (bits<14, 13>(m_PhysAddr) << 14) |
        ((~bit<12>(m_PhysAddr)&1 ) << 13) |
        ((bit<11>(m_PhysAddr)&1 ) << 12) |
        ((bit<10>(m_PhysAddr)&1 ) << 11) |
        // swizzle slices
        ((bit<15>(m_PhysAddr) ^ bit<10>(m_PhysAddr)  ^ bit<14>(m_PhysAddr) ^ bit<0>(swizzle_ps)) << 10) |
        ((bit<9>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ bit<1>(swizzle_ps)) << 9) |
        bits<8, 0>(m_PhysAddr);
}

void GKLIT2AddressDecode::CalcSliceSlices4Parts6()
{
    //        m_Slice =  ((bit<0>(m_Quotient) ^ bit<0>(m_Remainder)  ^ bit<2>(m_Quotient)          // part = r
    m_Slice =  static_cast<UINT32>(((bit<0>(m_Quotient)  ^ bit<0>(m_Remainder) ^ bit<3>(m_Quotient)    // post divide R swizzle
        ^ bit<4>(m_Quotient) ^ bit<5>(m_Quotient) ^ bit<6>(m_Quotient) ^ bit<7>(m_Quotient) ^ bit<8>(m_Quotient) ^ bit<9>(m_Quotient) ^ bit<10>(m_Quotient)
        ^ bit<11>(m_Quotient) ^ bit<12>(m_Quotient) ^ bit<13>(m_Quotient) ^ bit<14>(m_Quotient) ^ bit<15>(m_Quotient) ^ bit<16>(m_Quotient) ^ bit<17>(m_Quotient)
        ^ bit<18>(m_Quotient) ^ bit<19>(m_Quotient) ^ bit<20>(m_Quotient) ^ bit<21>(m_Quotient) ^ bit<22>(m_Quotient) ^ bit<23>(m_Quotient) ^ bit<24>(m_Quotient)
        ^ bit<25>(m_Quotient) ^ bit<26>(m_Quotient) ^ bit<27>(m_Quotient) ^ bit<28>(m_Quotient) ^ bit<29>(m_Quotient) ^ bit<30>(m_Quotient) ^ bit<31>(m_Quotient)
        ) << 1  ) | bit<9>(m_Offset));
}

void GKLIT2AddressDecode::CalcPartParts6()
{
    m_Partition = static_cast<UINT32>((bit<2>(m_Quotient) ^ bit<0>(m_Remainder)) |
        (bits<2, 1>(m_Remainder) << 1));
}

void GKLIT2AddressDecode::CalcL2SetSlices4L2Set16()
{
    switch (m_NumPartitions)
    {
    case 6:
        switch ((m_MappingMode >> 14) & 0x3)
        {
        // The following logic is according to
        // //hw/class/kepler/perfsim/code/perfsim/units/AddrMap/gklit2_chip.h#6,
        // which is synced to and more clear than
        // //hw/kepler1_gk100/clib/lwshared/Memory/gklit2_chip.h#5.
        case 1:
            CommonCalcL2AddressParts421();
            break;

        default:
            CalcL2SetSlices4L2Sets16Parts6GF100();
            break;
        }
        break;

    case 5:
        switch ((m_MappingMode >> 14) & 0x3)
        {
        // The following logic is according to
        // //hw/class/kepler/perfsim/code/perfsim/units/AddrMap/gklit2_chip.h#6,
        // which is synced to and more clear than
        // //hw/kepler1_gk100/clib/lwshared/Memory/gklit2_chip.h#5.
        case 1:
            CommonCalcL2AddressParts3();
            break;

        default:
            CalcL2SetSlices4L2Sets16Parts5GF100();
            break;
        }
        break;

    default:
        DefaultL2Address();
        break;
    }
}

void GKLIT2AddressDecode::CalcL2SetSlices4L2Set32()
{
    switch (m_NumPartitions)
    {
    case 6:
        switch ((m_MappingMode >> 14) & 0x3)
        {
        // The following logic is according to
        // //hw/class/kepler/perfsim/code/perfsim/units/AddrMap/gklit2_chip.h#6,
        // which is synced to and more clear than
        // //hw/kepler1_gk100/clib/lwshared/Memory/gklit2_chip.h#5.
        case 1:
            CommonCalcL2AddressParts421();
            break;

        case 2:
            CalcL2SetSlices4L2Sets32Parts6GF100();
            break;

        default:
            CalcL2SetSlices4L2Sets16Parts6GF100();
            break;
        }
        break;

    case 5:
        switch ((m_MappingMode >> 14) & 0x3)
        {
        // The following logic is according to
        // //hw/class/kepler/perfsim/code/perfsim/units/AddrMap/gklit2_chip.h#6,
        // which is synced to and more clear than
        // //hw/kepler1_gk100/clib/lwshared/Memory/gklit2_chip.h#5.
        case 1:
            CommonCalcL2AddressParts3();
            break;

        case 2:
            CalcL2SetSlices4L2Sets32Parts5GF100();
            break;

        default:
            CalcL2SetSlices4L2Sets16Parts5GF100();
            break;
        }
        break;

    default:
        DefaultL2Address();
        break;
    }
    if (((m_MappingMode >> 14) & 0x3) != 2)
    {
        m_L2Set = (bit<0>(m_L2Tag) << 4) | m_L2Set;
        m_L2Tag = m_L2Tag >> 1;
    }
}

void GKLIT2AddressDecode::CalcL2SetSlices4L2Sets16Parts6GF100()
{
    m_L2Set = static_cast<UINT32>((bit<7>(m_Offset) ^ bit<3>(m_Quotient) ^ bit<7>(m_Quotient) ^ bit<11>(m_Quotient) ^ bit<15>(m_Quotient)) |
        ((bit<8>(m_Offset) ^ bit<4>(m_Quotient) ^ bit<8>(m_Quotient) ^ bit<12>(m_Quotient) ^ bit<16>(m_Quotient)) << 1) |
        (bits<1, 0>(bits<3, 1>(m_Quotient) + bits<7, 5>(m_Quotient) + bits<11, 9>(m_Quotient) + bits<15, 13>(m_Quotient) + bit<17>(m_Quotient)) << 2));
    m_L2Tag = static_cast<UINT32>(bit<2>(bits<3, 1>(m_Quotient) + bits<7, 5>(m_Quotient) + bits<11, 9>(m_Quotient) + bits<15, 13>(m_Quotient) + bit<17>(m_Quotient)));
    // additional swizzling                             + ((bit<4>(m_Quotient) + bit<8>(m_Quotient) + bit<12>(m_Quotient) + bit<16>(m_Quotient)) << 2));
    m_L2Tag |= ((bits<31, 4>(m_Quotient)) << 1); // ors with bit zero prepared in case above
    CalcL2Sector();
}

void GKLIT2AddressDecode::CalcL2SetSlices4L2Sets16Parts5GF100()
{
    m_L2Set = static_cast<UINT32>((bit<7>(m_Offset) ^ bit<2>(m_Quotient) ^ bit<6>(m_Quotient) ^ bit<10>(m_Quotient) ^ bit<14>(m_Quotient)) |
        ((bit<8>(m_Offset) ^ bit<3>(m_Quotient) ^ bit<7>(m_Quotient) ^ bit<11>(m_Quotient) ^ bit<15>(m_Quotient)) << 1) |
        (bits<1, 0>(bits<2, 0>(m_Quotient) + bits<6, 4>(m_Quotient) + bits<10, 8>(m_Quotient) + bits<14, 12>(m_Quotient) + bit<16>(m_Quotient)) << 2));
    m_L2Tag = static_cast<UINT32>(bit<2>(bits<2, 0>(m_Quotient) + bits<6, 4>(m_Quotient) + bits<10, 8>(m_Quotient) + bits<14, 12>(m_Quotient) + bit<16>(m_Quotient)));
    // additional swizzling                             + ((bit<3>(m_Quotient) + bit<7>(m_Quotient) + bit<11>(m_Quotient) + bit<15>(m_Quotient)) << 2));
    m_L2Tag |= ((bits<31, 3>(m_Quotient)) << 1); // ors with bit zero prepared in case above
    CalcL2Sector();
}

void GKLIT2AddressDecode::CalcL2SetSlices4L2Sets32Parts6GF100()
{
    m_L2Set = static_cast<UINT32>((bit<7>(m_Offset) ^ bit<4>(m_Quotient) ^ bit<9>(m_Quotient) ^ bit<14>(m_Quotient)) |
        ((bit<8>(m_Offset) ^ bit<5>(m_Quotient) ^ bit<10>(m_Quotient) ^ bit<15>(m_Quotient)) << 1) |
        (bits<2, 0>(bits<3, 1>(m_Quotient) + bits<8, 6>(m_Quotient) + bits<13, 11>(m_Quotient) + bits<17, 16>(m_Quotient)) << 2));
    m_L2Tag = static_cast<UINT32>(bits<31, 4>(m_Quotient));

    CalcL2Sector();
}

void GKLIT2AddressDecode::CalcL2SetSlices4L2Sets32Parts5GF100()
{
    m_L2Set = static_cast<UINT32>((bit<7>(m_Offset) ^ bit<3>(m_Quotient) ^ bit<8>(m_Quotient) ^ bit<13>(m_Quotient)) |
        ((bit<8>(m_Offset) ^ bit<4>(m_Quotient) ^ bit<9>(m_Quotient) ^ bit<14>(m_Quotient)) << 1) |
        (bits<2, 0>(bits<2, 0>(m_Quotient) + bits<7, 5>(m_Quotient) + bits<12, 10>(m_Quotient) + bits<16, 15>(m_Quotient)) << 2));
    m_L2Tag = static_cast<UINT32>(bits<31, 3>(m_Quotient));

    CalcL2Sector();
}

GKLIT2AddressEncode::GKLIT2AddressEncode
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
  m_PdsReverseMap(0),
  m_PhysAddr(0),
  m_NumXbarL2Slices(0),
  m_FBAColumnBitsAboveBit8(0)
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

    m_FBADRAMBankBits = Utility::Log2i(m_NumDRAMBanks);

    const INT32 Pri = Tee::PriLow;
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

UINT64 GKLIT2AddressEncode::EncodeAddress
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

    Printf(Tee::PriLow, "GKLIT2 Address Encode 0x%llx, kind 0x%02x, page %uKB: "
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
    Printf(Tee::PriLow, "Detailed Encode for 0x%llx: "
           "paks 0x%llx, q 0x%llx, r 0x%x, o 0x%x, s %u, "
           "l2set %u, l2t %u, l2o %u, paks98 0x%llx, paks7 0x%llx\n",
           m_PhysAddr, m_PAKS, m_Quotient, m_Remainder, m_Offset, m_Slice,
           m_L2Set, m_L2Tag, m_L2Offset, m_PAKS_9_8, m_PAKS_7);

    return m_PhysAddr + (rowOffset % m_AmapColumnSize);
}

void GKLIT2AddressEncode::MapRBCToXBar()
{
    UINT64 RBLWpper = 0;
    UINT32 BankShift = m_FBADRAMBankBits;
    UINT32 OldBank = m_IntBank;

    switch (m_NumDRAMBanks)
    {
      case 4:
        break;

      case 8:
        OldBank = (bit<0>(m_IntBank) << 2) | (bit<1>(m_IntBank) <<1) | bit<2>(m_IntBank);
        break;

      case 16:
        OldBank = (bit<3>(m_IntBank) << 3) | (bit<0>(m_IntBank) << 2) | (bit<1>(m_IntBank) << 1) | bit<2>(m_IntBank);
        break;
    }

    //common RBC_upper except 512B stride in 32 L2stes
    RBLWpper = (m_Row << m_FBAColumnBitsAboveBit8) | (m_Column >> 8);
    RBLWpper = (RBLWpper << BankShift) | OldBank;

    m_L2Offset = (bits<4, 0>(m_Column))<<2;

    if (m_EccOn)
    {
        if (m_DRAMPageSize > 1024)
        {
            ReverseEccAddr(&m_Row, &m_Column, &OldBank, &m_Padr );
        }
        else
        {
            m_Padr = (RBLWpper << 3) | bits<7, 5>(m_Column);
            m_Padr = (m_Padr / 16) * 15 + (m_Padr % 16);
        }
    }
    else
    {
        m_Padr    = (RBLWpper << 3) | bits<7, 5>(m_Column);
    }

    m_Slice = static_cast<UINT32>(m_SubPartition<<1 | bit<2>(m_Padr));
    m_Padr = (((m_Padr>>3)<<2) | bits<1, 0>(m_Padr));

}

void GKLIT2AddressEncode::ReverseEccAddr(UINT32 *pRow, UINT32 *pCol, UINT32 *pBank, UINT64* pAdr)
{

    UINT64 adr;
    UINT32 row, col, bank;
    UINT64 tmp = 0;
    UINT32 colBits_2_0 = 0;
    UINT32 colBit_8 = 0;
    UINT32 newBank = 0;
    UINT32 newRow = 0;
    UINT64 compPadr = 0;
    UINT64 fbPadr = 0;
    UINT32 totalBankNumBits = 0;

    row  = *pRow;
    col  = *pCol;
    bank = *pBank;

    adr = row;
    adr = (adr << m_FBAColumnBitsAboveBit8) | ( col >> 8);
    adr = (adr << m_FBADRAMBankBits) | bank;
    adr = (adr << 3) | ((col >> 5) & 0x7);

    if (m_DRAMPageSize > 1024)
    {
        totalBankNumBits = m_FBADRAMBankBits;
        adr = ((adr >> (4+totalBankNumBits)) << (4+totalBankNumBits))
                | ( ((adr >> 3) & (( 1 << totalBankNumBits) - 1)) << 4)
                | (((adr >> (3 + totalBankNumBits)) & 0x1) <<3) | bits<2, 0>(adr);          //GETBITS(2, 0, adr);
    }

    compPadr = (adr / 16) * 15 + (adr % 16);
    if (m_DRAMPageSize > 1024)
    {
        tmp = compPadr;
        colBits_2_0 = tmp & 0x7;
        colBit_8 = (tmp>>3) & 0x1;
        tmp >>= 4;
        newBank = tmp & ((1 << m_FBADRAMBankBits) - 1);
        tmp >>= m_FBADRAMBankBits;
        newRow = static_cast<UINT32>(tmp);
        fbPadr = (newRow << (4+m_FBADRAMBankBits)) | (colBit_8 << (3+m_FBADRAMBankBits)) | (newBank << 3) | colBits_2_0;
    }
    else
    {
        fbPadr = compPadr;
    }

    *pRow = row;
    *pCol = col;
    *pBank = bank;
    *pAdr  = fbPadr;
}

void GKLIT2AddressEncode::MapXBarToPA()
{
    m_L2Tag = (UINT32)(m_Padr >> m_NumL2SetsBits);
    m_L2Set = (UINT32)(m_Padr & ((1 << m_NumL2SetsBits) - 1));

    UINT32 TempSum;

    switch ( m_NumXbarL2Slices )
    {
        case 4:

        if ( m_NumL2Sets == 32 )
        {
          m_L2Tag = (m_L2Tag << 1) | bit<4>(m_L2Set);
          m_L2Set = bits<3, 0>(m_L2Set);
        }
        switch (m_NumPartitions)
        {
            case 4: case 2: case 1:
            {

                m_Quotient = bits<27, 1>(m_L2Tag) << 4;
                UINT32 swizzle_20 = static_cast<UINT32>(bits<6, 4>(m_Quotient) + bits<9, 7>(m_Quotient) + bits<12, 10>(m_Quotient) + bits<15, 13>(m_Quotient) + bits<18, 16>(m_Quotient) + bits<21, 19>(m_Quotient) + bits<24, 22>(m_Quotient) + bits<27, 25>(m_Quotient));
                m_Quotient |= bits<2, 0>( ((bit<0>(m_L2Tag) << 2) | bits<3, 2>(m_L2Set)) - bits<2, 0>(swizzle_20)) << 1;
                m_Quotient |= bit<1>(m_Slice);

                UINT32 swizzle_43 = static_cast<UINT32>(bit<3>(m_Quotient) + bit<9>(m_Quotient) + bit<15>(m_Quotient) + bit<21>(m_Quotient) + bit<27>(m_Quotient) + ((bit<6>(m_Quotient) + bit<12>(m_Quotient) + bit<18>(m_Quotient) + bit<24>(m_Quotient)) << 1));
                m_PAKS_9_8 = (bit<0>(m_Slice)<<1) | bit<0>(bit<1>(m_L2Set)^bit<0>(swizzle_43));
                m_PAKS_7 = bit<0>(bit<0>(m_L2Set)^bit<1>(swizzle_43));

            }
            break;

            case 6:
            {
                m_Quotient = bits<27, 1>(m_L2Tag) << 4;

                TempSum = static_cast<UINT32>(bits<2, 0>(bits<7, 5>(m_Quotient) + bits<11, 9>(m_Quotient) + bits<15, 13>(m_Quotient) + bit<17>(m_Quotient)));

                m_Quotient |= ( bits<2, 0>(((bit<0>(m_L2Tag) << 2) | bits<3, 2>(m_L2Set)) - TempSum) << 1 );
                m_Quotient |= bit<1>(m_Slice)  ^ bit<0>(bit<0>(m_Partition) ^ bit<2>(m_Quotient)) ^ bit<3>(m_Quotient)    // post divide R swizzle
                  ^ bit<4>(m_Quotient) ^ bit<5>(m_Quotient) ^ bit<6>(m_Quotient) ^ bit<7>(m_Quotient) ^ bit<8>(m_Quotient) ^ bit<9>(m_Quotient) ^ bit<10>(m_Quotient)
                  ^ bit<11>(m_Quotient) ^ bit<12>(m_Quotient) ^ bit<13>(m_Quotient) ^ bit<14>(m_Quotient) ^ bit<15>(m_Quotient) ^ bit<16>(m_Quotient) ^ bit<17>(m_Quotient)
                  ^ bit<18>(m_Quotient) ^ bit<19>(m_Quotient) ^ bit<20>(m_Quotient) ^ bit<21>(m_Quotient) ^ bit<22>(m_Quotient) ^ bit<23>(m_Quotient) ^ bit<24>(m_Quotient)
                  ^ bit<25>(m_Quotient) ^ bit<26>(m_Quotient) ^ bit<27>(m_Quotient) ^ bit<28>(m_Quotient) ^ bit<29>(m_Quotient) ^ bit<30>(m_Quotient) ^ bit<31>(m_Quotient);

                m_PAKS_9_8 = (bit<0>(m_Slice)<<1) | bit<0>(bit<1>(m_L2Set)^bit<4>(m_Quotient)^bit<8>(m_Quotient)^bit<12>(m_Quotient)^bit<16>(m_Quotient));
                m_PAKS_7 = bit<0>(bit<0>(m_L2Set)^bit<3>(m_Quotient)^bit<7>(m_Quotient)^bit<11>(m_Quotient)^bit<15>(m_Quotient));
            }
            break;

            case 5:
            {
                // Q[-:3] = L2tag[-:1]
                m_Quotient = bits<27, 1>(m_L2Tag) << 3;
                m_Quotient |= bits<2, 0>( ((bit<0>(m_L2Tag) << 2) | bits<3, 2>(m_L2Set)) - bits<2, 0>(+ bits<6, 4>(m_Quotient) + bits<10, 8>(m_Quotient) + bits<14, 12>(m_Quotient) + bit<16>(m_Quotient)) );
                m_PAKS_9_8 = (bit<0>(m_Slice)<<1) | bit<0>(bit<1>(m_L2Set) ^ bit<3>(m_Quotient) ^ bit<7>(m_Quotient) ^ bit<11>(m_Quotient) ^ bit<15>(m_Quotient));
                m_PAKS_7 = bit<0>(bit<0>(m_L2Set) ^ bit<2>(m_Quotient) ^ bit<6>(m_Quotient) ^ bit<10>(m_Quotient) ^ bit<14>(m_Quotient));
            }
            break;

            case 3:
            {
                // Q[-:3] = L2tag[-:1]
                m_Quotient = bits<27, 1>(m_L2Tag) << 3;
                UINT32 swizzle_20 = static_cast<UINT32>(bit<3>(m_Quotient) + (bit<5>(m_Quotient) << 1) + bits<10, 8>(m_Quotient) + bits<13, 11>(m_Quotient) + bits<16, 14>(m_Quotient) + bits<19, 17>(m_Quotient) + bits<21, 20>(m_Quotient));
                m_Quotient |= bits<2, 0>( ((bit<0>(m_L2Tag) << 2) | bits<3, 2>(m_L2Set)) - bits<2, 0>(swizzle_20) );
                UINT32 swizzle_43 = static_cast<UINT32>(bits<3, 2>(m_Quotient) + (bit<6>(m_Quotient)<<1) + bit<7>(m_Quotient) + bit<13>(m_Quotient) + bit<19>(m_Quotient) + ((bit<10>(m_Quotient) + bit<16>(m_Quotient) + bit<21>(m_Quotient)) << 1));
                m_PAKS_9_8 = (bit<0>(m_Slice)<<1) | bit<0>(bit<1>(m_L2Set)^bit<0>(swizzle_43));
                m_PAKS_7 = bit<0>(bit<0>(m_L2Set)^bit<1>(swizzle_43));
            }
            break;

        }
        break;

        case 2:
        switch (m_NumPartitions)
        {
            UINT32 R_0;
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

                m_Quotient |= bit<0> ( bit<0>(m_Slice) ^ bit<0>(R_0) ^  bit<3>(m_Quotient) ^  bit<4>(m_Quotient) ^  bit<5>(m_Quotient)
                                ^  bit<6>(m_Quotient) ^  bit<7>(m_Quotient) ^  bit<8>(m_Quotient) ^  bit<9>(m_Quotient) ^  bit<10>(m_Quotient)
                                ^  bit<11>(m_Quotient) ^  bit<12>(m_Quotient)
                                ^  bit<13>(m_Quotient) ^  bit<14>(m_Quotient) ^  bit<15>(m_Quotient) ^  bit<16>(m_Quotient) ^  bit<17>(m_Quotient)
                                ^  bit<18>(m_Quotient) ^  bit<19>(m_Quotient) ^  bit<20>(m_Quotient) ^  bit<21>(m_Quotient) ^  bit<22>(m_Quotient)
                                ^  bit<23>(m_Quotient) ^  bit<24>(m_Quotient) ^  bit<25>(m_Quotient) ^  bit<26>(m_Quotient) ^  bit<27>(m_Quotient)
                                ^  bit<28>(m_Quotient) ^  bit<29>(m_Quotient) ^  bit<30>(m_Quotient) ^  bit<31>(m_Quotient) );
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

            m_PAKS_9_8 = bits<1, 0>(((bit<0>(bit<2>(m_L2Set)^bit<2>(m_Quotient)^bit<6>(m_Quotient)^bit<10>(m_Quotient)^bit<14>(m_Quotient)))
                                    << 1) | (bit<0>(bit<1>(m_L2Set)^bit<3>(m_Quotient)^bit<7>(m_Quotient)^bit<11>(m_Quotient)^bit<15>(m_Quotient))));
            m_PAKS_7 = bit<0>(bit<0>(m_L2Set)^bit<1>(m_Quotient)^bit<5>(m_Quotient)^bit<9>(m_Quotient)^bit<13>(m_Quotient));
            m_PdsReverseMap = static_cast<UINT32>((m_Quotient << 3) | ( m_PAKS_9_8 << 1) | m_PAKS_7);
            break;

          default:
            MASSERT(!"Unexpected number of partitions");
            break;

        }
    }

    m_L2Set = (bit<0>(m_L2Tag) << 4) | m_L2Set;
    m_L2Tag = (m_L2Tag >> 1);

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
            m_Offset = static_cast<UINT32>((m_PAKS_9_8 << 8) | (m_PAKS_7 << 7) | bits<6, 0>(m_Offset));
            break;

        case 3: case 5:
            m_Offset = static_cast<UINT32>
                (((bit<1>(m_Slice)) << 10) | (m_PAKS_9_8 << 8) | (m_PAKS_7 << 7)|bits<6, 0>(m_Offset));
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
                    switch ( m_NumPartitions )
                    {
                        case 1:
                        {
                            UINT32 swizzle = (UINT32)(( (bit<7>(m_PAKS)<<1 | bit<15>(m_PAKS)) + (bit<16>(m_PAKS)<<1 | bit<8>(m_PAKS))
                                             + bits<12, 11>(m_PAKS) + bits<14, 13>(m_PAKS) + bits<18, 17>(m_PAKS) + bits<20, 19>(m_PAKS)
                                             + bits<22, 21>(m_PAKS) + bits<24, 23>(m_PAKS) + bits<26, 25>(m_PAKS) + bits<28, 27>(m_PAKS)
                                             + bits<30, 29>(m_PAKS) + bit<31>(m_PAKS)));
                            m_PhysAddr = ((bits<PA_MSB, 16>(m_PAKS) << 16)
                                  | (bits<8, 7>(m_PAKS) << 14)
                                  | (bits<15, 11>(m_PAKS) << 9)
                                  | ((bit<10>(m_PAKS) ^ bit<1>(swizzle)) << 8)
                                  | ((bit<9>(m_PAKS) ^ bit<0>(swizzle)) << 7)
                                  | bits<6, 0>(m_PAKS));
                            break;
                        }

                        case 2:
                        {
                            UINT32 swizzle = (UINT32)(( bits<8, 7>(m_PAKS)
                                             + bits<13, 12>(m_PAKS) + bits<15, 14>(m_PAKS) + bits<17, 16>(m_PAKS) + bits<19, 18>(m_PAKS)
                                             + bits<21, 20>(m_PAKS) + bits<23, 22>(m_PAKS) + bits<25, 24>(m_PAKS) + bits<27, 26>(m_PAKS) + bits<29, 28>(m_PAKS)
                                             + bits<31, 30>(m_PAKS)));
                            UINT32 bit9 = (UINT32)(( bit<7>(m_PAKS) ^ bit<8>(m_PAKS)
                                          ^ bit<10>(m_PAKS) ^ bit<12>(m_PAKS) ^ bit<13>(m_PAKS) ^ bit<14>(m_PAKS)
                                          ^ bit<15>(m_PAKS) ^ bit<16>(m_PAKS) ^ bit<17>(m_PAKS) ^ bit<18>(m_PAKS) ^ bit<19>(m_PAKS)
                                          ^ bit<20>(m_PAKS) ^ bit<21>(m_PAKS) ^ bit<22>(m_PAKS) ^ bit<23>(m_PAKS) ^ bit<24>(m_PAKS)
                                          ^ bit<25>(m_PAKS) ^ bit<26>(m_PAKS) ^ bit<27>(m_PAKS) ^ bit<28>(m_PAKS) ^ bit<29>(m_PAKS)
                                          ^ bit<30>(m_PAKS) ^ bit<31>(m_PAKS)
                                          ));
                            m_PhysAddr = ((bits<PA_MSB, 16>(m_PAKS) << 16)
                                  | (bits<8, 7>(m_PAKS) << 14)
                                  | (bits<15, 12>(m_PAKS) << 10)
                                  | (bit9 << 9)
                                  | ((bit<11>(m_PAKS) ^ bit<1>(swizzle)) << 8)
                                  | ((bit<9>(m_PAKS) ^ bit<0>(swizzle)) << 7)
                                  | bits<6, 0>(m_PAKS));
                            break;
                        }

                        case 5:
                        {
                            UINT32 swizzle = (UINT32)(( bits<8, 7>(m_PAKS)
                                             + bits<15, 14>(m_PAKS) + bits<17, 16>(m_PAKS) + bits<19, 18>(m_PAKS)
                                             + bits<21, 20>(m_PAKS) + bits<23, 22>(m_PAKS) + bits<25, 24>(m_PAKS) + bits<27, 26>(m_PAKS) + bits<29, 28>(m_PAKS)
                                             + bits<31, 30>(m_PAKS)));
                            m_PhysAddr = ((bits<PA_MSB, 16>(m_PAKS) << 16)
                                  | (bits<8, 7>(m_PAKS) << 14)
                                  | (bits<15, 11>(m_PAKS) << 9)
                                  | ((bit<10>(m_PAKS) ^ bit<1>(swizzle)) << 8)
                                  | ((bit<9>(m_PAKS) ^ bit<0>(swizzle)) << 7)
                                  | bits<6, 0>(m_PAKS));
                            break;
                        }

                        case 3:
                        {
                            UINT32 swizzle = (UINT32)(( (bit<7>(m_PAKS)<<1 | bit<15>(m_PAKS)) + (bit<16>(m_PAKS)<<1 | bit<8>(m_PAKS))
                                             + bits<14, 13>(m_PAKS) + bits<18, 17>(m_PAKS) + bits<20, 19>(m_PAKS)
                                             + bits<22, 21>(m_PAKS) + bits<24, 23>(m_PAKS) + bits<26, 25>(m_PAKS) + bits<28, 27>(m_PAKS)
                                             + bits<30, 29>(m_PAKS) ));
                            m_PhysAddr = ((bits<PA_MSB, 16>(m_PAKS) << 16)
                                  | (bits<8, 7>(m_PAKS) << 14)
                                  | (bits<15, 13>(m_PAKS) << 11)
                                  | (bits<12, 11>(m_PAKS) << 9)
                                  | ((bit<10>(m_PAKS) ^ bit<1>(swizzle)) << 8)
                                  | ((bit<9>(m_PAKS) ^ bit<0>(swizzle)) << 7)
                                  | bits<6, 0>(m_PAKS));
                            break;
                        }

                        case 4:
                        {
                            UINT32 swizzle = (UINT32)(( (bit<7>(m_PAKS)<<1 | bit<15>(m_PAKS)) + (bit<16>(m_PAKS)<<1 | bit<8>(m_PAKS))
                                             + bits<14, 13>(m_PAKS) + bits<18, 17>(m_PAKS) + bits<20, 19>(m_PAKS)
                                             + bits<22, 21>(m_PAKS) + bits<24, 23>(m_PAKS) + bits<26, 25>(m_PAKS) + bits<28, 27>(m_PAKS)
                                             + bits<30, 29>(m_PAKS) + bit<31>(m_PAKS)));
                            m_PhysAddr = ((bits<PA_MSB, 16>(m_PAKS) << 16)
                                  | (bits<8, 7>(m_PAKS) << 14)
                                  | (bits<15, 13>(m_PAKS) << 11)
                                  | (bits<1, 0>(bits<11, 10>(m_PAKS) - swizzle) << 9)
                                  | ((bit<12>(m_PAKS) ^ bit<1>(swizzle)) << 8)
                                  | ((bit<9>(m_PAKS) ^ bit<0>(swizzle)) << 7)
                                  | bits<6, 0>(m_PAKS));
                            break;
                        }

                        case 6:
                        {
                            UINT32 bit8 = (UINT32)(bit<0>( bit<8>(m_PAKS)
                                          ^ bit<10>(m_PAKS) ^ bit<11>(m_PAKS) ^ bit<12>(m_PAKS) ^ bit<15>(m_PAKS) ^ bit<17>(m_PAKS) ^ bit<19>(m_PAKS)
                                          ^ bit<21>(m_PAKS) ^ bit<23>(m_PAKS) ^ bit<25>(m_PAKS) ^ bit<27>(m_PAKS) ^ bit<29>(m_PAKS)
                                          ^ bit<31>(m_PAKS)
                                          ));
                            UINT32 bit7 = (UINT32)(bit<0>( bit<7>(m_PAKS) ^ bit<9>(m_PAKS)
                                          ^ bit<14>(m_PAKS) ^ bit<16>(m_PAKS) ^ bit<18>(m_PAKS)
                                          ^ bit<20>(m_PAKS) ^ bit<22>(m_PAKS) ^ bit<24>(m_PAKS) ^ bit<26>(m_PAKS) ^ bit<28>(m_PAKS)
                                          ^ bit<30>(m_PAKS)
                                          ));
                            m_PhysAddr = ((bits<PA_MSB, 16>(m_PAKS) << 16)
                                  | (bits<8, 7>(m_PAKS) << 14)
                                  | (bits<15, 11>(m_PAKS) << 9)
                                  | (bit8 << 8)
                                  | (bit7 << 7)
                                  | bits<6, 0>(m_PAKS));
                            break;
                        }

                        default:
                            break;
                    }

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
                            m_PhysAddr = (bits<PA_MSB, 13>(m_PAKS) << 13);
                            m_PhysAddr |= ((bit<10>(m_PAKS) ^ bit<12>(m_PAKS) ^ bit<15>(m_PAKS) ^ bit<18>(m_PAKS)
                                  ^ bit<21>(m_PAKS) ^ bit<24>(m_PAKS) ^ bit<27>(m_PAKS) ^ bit<30>(m_PAKS)) << 12);
                            m_PhysAddr |= (bits<11, 0>(m_PAKS));
                            break;

                        case 3:
                            m_PhysAddr = m_PAKS;
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

void GKLIT2AddressEncode::CalcPhysAddrPitchLinearPS4KParts1()
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

void GKLIT2AddressEncode::CalcPhysAddrPitchLinearPS4KParts2()
{
    UINT64 swizzle = (UINT32)
                          (( bits<8, 7>(m_PAKS) + bits<13, 12>(m_PAKS) + bits<15, 14>(m_PAKS) + bits<17, 16>(m_PAKS)
                             + bits<19, 18>(m_PAKS) + bits<21, 20>(m_PAKS) + bits<23, 22>(m_PAKS) + bits<25, 24>(m_PAKS)
                             + bits<27, 26>(m_PAKS) + bits<29, 28>(m_PAKS) + bits<31, 30>(m_PAKS)));

    UINT32 bit9 = (UINT32)
                          (( bit<7>(m_PAKS) ^ bit<8>(m_PAKS) ^ bit<10>(m_PAKS) ^ bit<12>(m_PAKS) ^ bit<13>(m_PAKS)
                             ^ bit<14>(m_PAKS) ^ bit<15>(m_PAKS) ^ bit<16>(m_PAKS) ^ bit<17>(m_PAKS)
                             ^ bit<18>(m_PAKS) ^ bit<19>(m_PAKS) ^ bit<20>(m_PAKS) ^ bit<21>(m_PAKS)
                             ^ bit<22>(m_PAKS) ^ bit<23>(m_PAKS) ^ bit<24>(m_PAKS) ^ bit<25>(m_PAKS)
                             ^ bit<26>(m_PAKS) ^ bit<27>(m_PAKS) ^ bit<28>(m_PAKS) ^ bit<29>(m_PAKS)
                             ^ bit<30>(m_PAKS) ^ bit<31>(m_PAKS)));

    m_PhysAddr = ((bits<PA_MSB, 12>(m_PAKS) << 12)
                              | (bit<8>(m_PAKS) << 11)
                              | (bit<7>(m_PAKS) << 10)
                              | (bit9 << 9)
                              | ((bit<11>(m_PAKS) ^ bit<1>(swizzle)) << 8)
                              | ((bit<9>(m_PAKS) ^ bit<0>(swizzle)) << 7)
                              | bits<6, 0>(m_PAKS));
}

void GKLIT2AddressEncode::CalcPhysAddrPitchLinearPS4KParts3()
{
    UINT64 palev1 = m_PAKS;
    UINT64 swizzle = (UINT32)(((bit<12>(palev1) << 1) | bit<8>(palev1)) ^ bits<14, 13>(palev1)
      ^ bits<16, 15>(palev1) ^ bits<18, 17>(palev1) ^ bits<20, 19>(palev1)
      ^ bits<22, 21>(palev1) ^ bits<24, 23>(palev1) ^ bits<26, 25>(palev1)
      ^ bits<28, 27>(palev1) ^ bits<30, 29>(palev1));

    m_PhysAddr = (bits<PA_MSB, 12>(palev1) << 12);

    m_PhysAddr |= (bit<8>(palev1) << 11);
    m_PhysAddr |= (bit<7>(palev1) << 10);
    m_PhysAddr |= (bit<11>(palev1) << 9);

    m_PhysAddr |= ((bit<10>(palev1) ^ bit<1>(swizzle) ^ bit<0>(swizzle)) <<8);

    m_PhysAddr |= (bit<9>(palev1) << 7);
    m_PhysAddr |= (bits<6, 0>(palev1));
}

void GKLIT2AddressEncode::CalcPhysAddrPitchLinearPS4KParts4()
{
    UINT64 palev1;
    palev1 = (bits<PA_MSB, 13>(m_PAKS) << 13);
    palev1 |= ((bit<10>(m_PAKS) ^ bit<12>(m_PAKS) ^ bit<15>(m_PAKS) ^ bit<18>(m_PAKS)
              ^ bit<21>(m_PAKS) ^ bit<24>(m_PAKS) ^ bit<27>(m_PAKS) ^ bit<30>(m_PAKS)) << 12);
    palev1 |= (bits<11, 0>(m_PAKS));

    m_PhysAddr = (bits<PA_MSB, 12>(palev1) << 12);
    m_PhysAddr |= (bits<8, 7>(palev1) <<10);

    m_PhysAddr |= ((bit<9>(palev1) ^ bit<13>(palev1))<<7);
    m_PhysAddr |= (bits<6, 0>(palev1));

    UINT32 mysum = (UINT32)( bits<8, 7>(palev1) + (bit<13>(palev1) << 1)
      + bits<15, 14>(palev1) + bits<17, 16>(palev1) + bits<19, 18>(palev1)
      + bits<21, 20>(palev1) + bits<23, 22>(palev1) + bits<25, 24>(palev1)
      + bits<27, 26>(palev1) + bits<29, 28>(palev1) + bits<31, 30>(palev1));

    m_PhysAddr |= ((bits<1, 0>( bits<11, 10>(palev1)  - (mysum % 4))) << 8);
}

void GKLIT2AddressEncode::CalcPhysAddrPitchLinearPS4KParts5()
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

void GKLIT2AddressEncode::CalcPhysAddrPitchLinearPS4KParts6()
{

    UINT32 bit8 = (UINT32)
                    (bit<0>( bit<7>(m_PAKS) ^ bit<10>(m_PAKS) ^ bit<11>(m_PAKS) ^ bit<13>(m_PAKS)
                   ^ bit<15>(m_PAKS) ^ bit<17>(m_PAKS) ^ bit<19>(m_PAKS) ^ bit<21>(m_PAKS)
                   ^ bit<23>(m_PAKS) ^ bit<25>(m_PAKS) ^ bit<27>(m_PAKS) ^ bit<29>(m_PAKS)
                   ^ bit<31>(m_PAKS)));
    UINT32 bit7 = (UINT32)
                  (bit<0>( bit<9>(m_PAKS) ^ bit<12>(m_PAKS) ^ bit<14>(m_PAKS) ^ bit<16>(m_PAKS)
                   ^ bit<18>(m_PAKS) ^ bit<20>(m_PAKS) ^ bit<22>(m_PAKS) ^ bit<24>(m_PAKS)
                   ^ bit<26>(m_PAKS) ^ bit<28>(m_PAKS) ^ bit<30>(m_PAKS)));
    m_PhysAddr = ((bits<PA_MSB, 12>(m_PAKS) << 12)
          | (bits<8, 7>(m_PAKS) << 10)
          | (bit<11>(m_PAKS) << 9)
          | (bit8 << 8)
          | (bit7 << 7)
          | bits<6, 0>(m_PAKS));
}
