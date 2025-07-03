/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2013,2015-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// The contents of this file have been copied from:
// //hw/kepler1_gk100/clib/lwshared/Memory/amap.h#18
// //hw/kepler1_gk100/clib/lwshared/Memory/gklit1.h#12
// //hw/kepler1_gk100/clib/lwshared/Memory/gklit1_chip.h#2
//
// The address mapping is dolwmented here (but I doubt it is out of date):
// //hw/doc/gpu/kepler/kepler/design/Functional_Descriptions/kepler_address_mapping.doc

#include "gklit1adr.h"
#include "bitop.h"
#include "kepler/gk104/dev_mmu.h"
#include "kepler/gk104/dev_fbpa.h"
#include "kepler/gk104/dev_ltc.h"

using namespace BitOp;

#define PA_MSB 39

GKLIT1AddressDecode::GKLIT1AddressDecode
(
    UINT32 NumPartitions,
    GpuSubdevice* pSubdev
)
: GF100AddressDecode(NumPartitions, pSubdev),
m_Padr(0),
m_PALev1(0),
m_MappingMode(0),
m_ECCPageSize(2048),
m_NumECCLinePerPage(15),
m_NumLTCLinePerPage(16),
m_8BgSwizzle(false)
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

GKLIT1AddressDecode::~GKLIT1AddressDecode()
{
}

void GKLIT1AddressDecode::ComputePAKS()
{
    DefaultPAKS();
}

void GKLIT1AddressDecode::ComputeQRO()
{
    CommonCalcQRO();
}

void GKLIT1AddressDecode::ComputePartition()
{
    DefaultPart();
}

void GKLIT1AddressDecode::ComputeSlice()
{
    DefaultSlice();
}

void GKLIT1AddressDecode::ComputeL2Address()
{
    DefaultL2Address();
}

void GKLIT1AddressDecode::ComputeRBSC()
{
    bool ans = false;
    m_Padr = XbarRawPaddr();
    switch(m_NumDRAMBanks)
    {
    case 16:
        ans = CalcRBCSlices4L2Sets16DRAMBanks16Bool();
        break;
    case 8:
        ans = CalcRBCSlices4L2Sets16DRAMBanks8Bool();
        break;
    case 4:
        ans = CalcRBCSlices4L2Sets16DRAMBanks4Bool();
        break;
    default:
        Printf(Tee::PriHigh, "DecodeAddress: "
            "Unsupported number of DRAM banks - %u!\n",
            m_NumDRAMBanks);
        MASSERT(!"Unsupported number of DRAM banks!");
    }
    if (ans == false)
    {
        GF100AddressDecode::ComputeRBSC();
    }
}

void GKLIT1AddressDecode::PrintDecodeDetails(UINT32 PteKind, UINT32 PageSize)
{
    Printf(Tee::PriLow, "PA = 0x%llx\n", m_PhysAddr);
    Printf(Tee::PriLow, "Kind = %u, Pagesize= %uK\n", PteKind, PageSize);
    Printf(Tee::PriLow, "MMU\t\tQ:R:O:PAKS = 0x%llx:0x%x:0x%x:0x%llx\n",
            m_Quotient, m_Remainder, m_Offset, m_PAKS);
    Printf(Tee::PriLow, "FB\t\tSlice:Part:Padr = %u:%u:0x%x\n",
            m_Slice, m_Partition, m_Padr);
    Printf(Tee::PriLow, "L2\t\tTag:Set:Offset = 0x%x:0x%x:0x%x\n",
            m_L2Tag, m_L2Set, m_L2Offset);
    Printf(Tee::PriLow, "DRAM\t\textbank:row:bank:subp:col = %u:0x%x:0x%x:%u:0x%x\n",
            m_ExtBank, m_Row, m_Bank, m_SubPart, m_Col);
}

void GKLIT1AddressDecode::CommonCalcPAKSCommonPS4KParts3()
{
    // This section must be identical for pitch and block linear 3 partitions
    m_PAKS = (bits<PA_MSB,14>(m_PALev1) << 14) |
        (bit<13>(m_PALev1) << 13) |
        ((bit<12>(m_PALev1)  /* ^ bit<10>(m_PALev1) ^ bit<11>(m_PALev1) ^ bit<13>(m_PALev1)*/ ) << 12) |
        bits<11,0>(m_PALev1);  // inside 4KB page
}

void GKLIT1AddressDecode::CommonCalcPAKSCommonPS4KParts4()
{
    UINT32 camp_fix_4KB_common = static_cast<UINT32>(bit<12>(m_PALev1) ^ bit<18>(m_PALev1) ^ bit<21>(m_PALev1) ^ bit<24>(m_PALev1) ^ bit<27>(m_PALev1) ^ bit<30>(m_PALev1));
    m_PAKS = (bits<PA_MSB,13>(m_PALev1) << 13) |
        // slice[1]
        // want to interleave slices vertically up to the top because same swizzling is used by pitch
        ((bit<0>(camp_fix_4KB_common) ^ bit<10>(m_PALev1)
        ^ bit<15>(m_PALev1)
        ) << 12) |
        bits<11,0>(m_PALev1);  // inside 4KB page
}

void GKLIT1AddressDecode::CommonCalcPAKSBlockLinearPS4KParts4()
{
    UINT32 camp_fix_4KB = static_cast<UINT32>(bits<11,10>(m_PhysAddr) + bits<17,16>(m_PhysAddr) + bits<20,19>(m_PhysAddr) + bits<23,22>(m_PhysAddr) + bits<26,25>(m_PhysAddr) + bits<29,28>(m_PhysAddr));
    m_PALev1 = (bits<PA_MSB,12>(m_PhysAddr) << 12) |
        // part[1:0]
        // cannot swizzle m_PhysAddr[10] with m_PhysAddr[12] derivatives without aliasing
        (((bits<1,0>(camp_fix_4KB)
        + (bit<13>(m_PhysAddr) <<1)
        + bits<15,14>(m_PhysAddr)
        )%4) << 10) |
        // slice[0]
        ((bit<9>(m_PhysAddr) ^ bit<13>(m_PhysAddr)) << 9) |
        // gob
        bits<8,0>(m_PhysAddr);
    CommonCalcPAKSCommonPS4KParts4();
}

void GKLIT1AddressDecode::CommonCalcPAKSBlockLinearPS4KParts3()
{
    UINT32 camp_fix = static_cast<UINT32>(bit<18>(m_PhysAddr) ^ bit<19>(m_PhysAddr) ^ bit<20>(m_PhysAddr) ^ bit<21>(m_PhysAddr) ^ bit<22>(m_PhysAddr) ^
        bit<23>(m_PhysAddr) ^ bit<24>(m_PhysAddr) ^ bit<25>(m_PhysAddr) ^ bit<26>(m_PhysAddr) ^ bit<27>(m_PhysAddr) ^ bit<28>(m_PhysAddr) ^
        bit<29>(m_PhysAddr) ^ bit<30>(m_PhysAddr) ^ bit<31>(m_PhysAddr));
    m_PALev1 = (bits<PA_MSB,12>(m_PhysAddr) << 12) |

        // 4KB
        // swizzle p[0]
        (bit<11>(m_PhysAddr) << 11) |
        // swizzle slices
        ((bit<10>(m_PhysAddr) ^ bit<13>(m_PhysAddr)  ^ bit<0>(camp_fix) ) << 10) |
        ((bit<9>(m_PhysAddr) ^ bit<14>(m_PhysAddr)) << 9) |
        bits<8,0>(m_PhysAddr);
    CommonCalcPAKSCommonPS4KParts3();
}

void GKLIT1AddressDecode::CommonCalcPAKSBlockLinearPS4KParts2()
{
    UINT32 camp_fix = static_cast<UINT32>(bits<17,16>(m_PhysAddr) + bits<19,18>(m_PhysAddr) + bits<21,20>(m_PhysAddr) + bits<23,22>(m_PhysAddr) + bits<25,24>(m_PhysAddr) +
        bits<27,26>(m_PhysAddr) + bits<29,28>(m_PhysAddr) + bits<31,30>(m_PhysAddr));
    UINT32 swizzle = static_cast<UINT32>(bit<10>(m_PhysAddr) ^ bit<13>(m_PhysAddr));

    m_PAKS = (bits<PA_MSB,12>(m_PhysAddr) << 12) |

        ((bit<11>(m_PhysAddr) ^ bit<0>(swizzle) ^ bit<14>(m_PhysAddr) ^ bit<1>(camp_fix)
        ) << 11) |

        ((bit<0>(swizzle) ^ bit<0>(camp_fix)) << 10) |

        ((bit<9>(m_PhysAddr) ^ bit<13>(m_PhysAddr)) << 9) |
        bits<8,0>(m_PhysAddr);
}

void GKLIT1AddressDecode::CommonCalcPAKSBlockLinearPS4KParts1()
{
    UINT32 swizzle = static_cast<UINT32>(bits<14,13>(m_PhysAddr) + bits<16,15>(m_PhysAddr));
    m_PAKS = (bits<PA_MSB,11>(m_PhysAddr) << 11) |
        // don't want diagonals
        // using ^ here so that carry doesn't propagate into bit10 from changing bit9, allows bit flipping in ROP
        ((bit<10>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<9>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        bits<8,0>(m_PhysAddr);
}

void GKLIT1AddressDecode::CommonCalcPAKSBlockLinearPS4KParts11KSliceStride()
{
    // changing the slice stride to 1K for gk107
    UINT32 swizzle = static_cast<UINT32>(bits<14,13>(m_PhysAddr) + bits<16,15>(m_PhysAddr));
    m_PAKS = (bits<PA_MSB,12>(m_PhysAddr) << 12) |
        ((bit<9>(m_PhysAddr)) << 11) |
        ((bit<11>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<10>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        bits<8,0>(m_PhysAddr);
}

void GKLIT1AddressDecode::CommonCalcPAKSBlockLinearPS64KParts4()
{
    UINT32 swizzle_64 = static_cast<UINT32>(bits<11,10>(m_PhysAddr) + (bit<13>(m_PhysAddr) << 1) + bit<14>(m_PhysAddr));
    UINT32 swizzle_ps = static_cast<UINT32>(bits<22,19>(m_PhysAddr) + bits<26,23>(m_PhysAddr) + bits<30,27>(m_PhysAddr) + bits<34,31>(m_PhysAddr) + bits<36,35>(m_PhysAddr));

    m_PAKS = (bits<PA_MSB,13>(m_PhysAddr) << 13) |
        // slice 1
        (bit<12>(m_PhysAddr) ^ bit<10>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ bit<14>(m_PhysAddr) ^ bit<15>(m_PhysAddr) ^ bit<2>(swizzle_ps)) << 12 |
        // partitions
        bits<1,0>((bits<1,0>(swizzle_64) + bits<1,0>(swizzle_ps))) << 10 |
        // slice 0
        (bit<9>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ bit<16>(m_PhysAddr) ^ bit<3>(swizzle_ps)) << 9 |
        bits<8,0>(m_PhysAddr);
}

void GKLIT1AddressDecode::CommonCalcPAKSBlockLinearPS64KParts3()
{
    UINT32 swizzle_ps = static_cast<UINT32>((bit<19>(m_PhysAddr)<<1) ^ bits<21,20>(m_PhysAddr) ^ bits<23,22>(m_PhysAddr) ^
        bits<25,24>(m_PhysAddr) ^ bits<27,26>(m_PhysAddr) ^ bits<29,28>(m_PhysAddr) ^ bits<31,30>(m_PhysAddr)
        ^ bits<33,32>(m_PhysAddr) ^ bits<35,34>(m_PhysAddr) ^ bit<36>(m_PhysAddr));
    m_PAKS = (bits<PA_MSB,11>(m_PhysAddr) << 11) |
        // swizzle slices
        // bit15 prevents diagonal slices
        ((bit<10>(m_PhysAddr) ^ bit<14>(m_PhysAddr) ^ bit<15>(m_PhysAddr) ^ bit<0>(swizzle_ps)) << 10) |
        ((bit<9>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ bit<1>(swizzle_ps)) << 9) |
        bits<8,0>(m_PhysAddr);
}

void GKLIT1AddressDecode::CommonCalcPAKSBlockLinearPS64KParts2()
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

void GKLIT1AddressDecode::CommonCalcPAKSBlockLinearPS64KParts1()
{
    UINT32 swizzle = static_cast<UINT32>(bits<19,18>(m_PhysAddr) + bits<21,20>(m_PhysAddr) + bits<23,22>(m_PhysAddr) +
        bits<25,24>(m_PhysAddr) + bits<27,26>(m_PhysAddr) + bits<29,28>(m_PhysAddr) + bits<31,30>(m_PhysAddr)
        + bits<33,32>(m_PhysAddr) + bits<35,34>(m_PhysAddr) + bit<36>(m_PhysAddr));

    m_PAKS = (bits<PA_MSB,11>(m_PhysAddr) << 11) |
        // don't want diagonals
        // using ^ here so that carry doesn't propagate into bit10 from changing bit9, allows bit flipping in ROP
        ((bit<10>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<9>(m_PhysAddr) ^ bit<14>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        bits<8,0>(m_PhysAddr);
}

void GKLIT1AddressDecode::CommonCalcPAKSBlockLinearPS64KParts11KSliceStride()
{
    // changing the slice stride to 1K for gk107
    UINT32 swizzle = static_cast<UINT32>(bits<19,18>(m_PhysAddr) + bits<21,20>(m_PhysAddr) + bits<23,22>(m_PhysAddr) +
        bits<25,24>(m_PhysAddr) + bits<27,26>(m_PhysAddr) + bits<29,28>(m_PhysAddr) + bits<31,30>(m_PhysAddr)
        + bits<33,32>(m_PhysAddr) + bits<35,34>(m_PhysAddr) + bit<36>(m_PhysAddr));

    m_PAKS = (bits<PA_MSB,12>(m_PhysAddr) << 12) |
        (bit<9>(m_PhysAddr) << 11) |
        ((bit<11>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<10>(m_PhysAddr) ^ bit<14>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        bits<8,0>(m_PhysAddr);
}

void GKLIT1AddressDecode::CommonCalcPAKSPitchLinearPS4KParts4()
{
    m_PALev1 = (bits<PA_MSB,12>(m_PhysAddr) << 12) |
        // part[1:0]
        // cannot swizzle m_PhysAddr[8] with m_PhysAddr[12] derivatives without aliasing
        (((bits<9,8>(m_PhysAddr)
        + bits<11,10>(m_PhysAddr)
        + (bit<13>(m_PhysAddr) <<1)
        + bits<15,14>(m_PhysAddr) + bits<17,16>(m_PhysAddr) + bits<19,18>(m_PhysAddr)
        + bits<21,20>(m_PhysAddr) + bits<23,22>(m_PhysAddr) + bits<25,24>(m_PhysAddr)
        + bits<27,26>(m_PhysAddr) + bits<29,28>(m_PhysAddr) + bits<31,30>(m_PhysAddr)

        )%4) << 10) |

        // slice[0]
        ((bit<7>(m_PhysAddr) ^ bit<13>(m_PhysAddr)) << 9) |

        // gob
        (bits<11,10>(m_PhysAddr) << 7) |
        bits<6,0>(m_PhysAddr);

    CommonCalcPAKSCommonPS4KParts4();
}

void GKLIT1AddressDecode::CommonCalcPAKSPitchLinearPS4KParts3()
{
    // both pitch and blocklinear 4KB mappings must swap bits 15, 11
    UINT64 swizzle = bits<12,11>(m_PhysAddr) ^ bits<14,13>(m_PhysAddr) ^  bits<16,15>(m_PhysAddr) ^
        bits<18,17>(m_PhysAddr) ^ bits<20,19>(m_PhysAddr) ^  bits<22,21>(m_PhysAddr) ^
        bits<24,23>(m_PhysAddr) ^ bits<26,25>(m_PhysAddr) ^  bits<28,27>(m_PhysAddr) ^
        bits<30,29>(m_PhysAddr);

    m_PALev1 = (bits<PA_MSB,12>(m_PhysAddr) << 12) |

        // 4KB
        // part[0]
        (bit<9>(m_PhysAddr) << 11) |

        // slice[1:0]
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle) ^ bit<0>(swizzle)) << 10) |
        ((bit<7>(m_PhysAddr) ) << 9) |

        (bit<11>(m_PhysAddr) << 8) |
        (bit<10>(m_PhysAddr) << 7) |
        bits<6,0>(m_PhysAddr);

    CommonCalcPAKSCommonPS4KParts3();
}

void GKLIT1AddressDecode::CommonCalcPAKSPitchLinearPS4KParts2()
{
    // looks a lot like 64KB, 128KB 2 partitions pitch
    UINT64 swizzle = bits<11,10>(m_PhysAddr) + bits<13,12>(m_PhysAddr) + bits<15,14>(m_PhysAddr) +
        bits<17,16>(m_PhysAddr) + bits<19,18>(m_PhysAddr) + bits<21,20>(m_PhysAddr) + bits<23,22>(m_PhysAddr) +
        bits<25,24>(m_PhysAddr) + bits<27,26>(m_PhysAddr) + bits<29,28>(m_PhysAddr) + bits<31,30>(m_PhysAddr)
        ;

    m_PAKS = (bits<PA_MSB,12>(m_PhysAddr) << 12) |
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle)) << 11) |

        (((bit<9>(m_PhysAddr) + bit<10>(m_PhysAddr) + bit<11>(m_PhysAddr) + bit<12>(m_PhysAddr) + bit<13>(m_PhysAddr)
        + bit<14>(m_PhysAddr) + bit<15>(m_PhysAddr) + bit<16>(m_PhysAddr) + bit<17>(m_PhysAddr) + bit<18>(m_PhysAddr)
        + bit<19>(m_PhysAddr) + bit<20>(m_PhysAddr) + bit<21>(m_PhysAddr) + bit<22>(m_PhysAddr) + bit<23>(m_PhysAddr)
        + bit<24>(m_PhysAddr) + bit<25>(m_PhysAddr) + bit<26>(m_PhysAddr) + bit<27>(m_PhysAddr) + bit<28>(m_PhysAddr)
        + bit<29>(m_PhysAddr) + bit<30>(m_PhysAddr) + bit<31>(m_PhysAddr)) %2) << 10) |

        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        (bits<11,10>(m_PhysAddr) << 7) |
        bits<6,0>(m_PhysAddr);
}

void GKLIT1AddressDecode::CommonCalcPAKSPitchLinearPS4KParts1()
{
    UINT32 swizzle = static_cast<UINT32>(bits<10,9>(m_PhysAddr) + bits<12,11>(m_PhysAddr) + bits<14,13>(m_PhysAddr) +
        bits<16,15>(m_PhysAddr) + bits<18,17>(m_PhysAddr) + bits<20,19>(m_PhysAddr) + bits<22,21>(m_PhysAddr) +
        bits<24,23>(m_PhysAddr) + bits<26,25>(m_PhysAddr) + bits<28,27>(m_PhysAddr) + bits<30,29>(m_PhysAddr) + bit<31>(m_PhysAddr)
        );

    m_PAKS = (bits<PA_MSB,12>(m_PhysAddr) << 12) |

        (bit<9>(m_PhysAddr) << 11) |

        // slice[1:0]
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        (bits<11,10>(m_PhysAddr) << 7) |
        bits<6,0>(m_PhysAddr);
}

void GKLIT1AddressDecode::CommonCalcPAKSPitchLinearPS64KParts4()
{
    UINT32 swizzle = static_cast<UINT32>(bits<12,11>(m_PhysAddr) + bits<14,13>(m_PhysAddr) + bits<16,15>(m_PhysAddr) + bits<18,17>(m_PhysAddr) + bits<20,19>(m_PhysAddr) +
        bits<22,21>(m_PhysAddr) + bits<24,23>(m_PhysAddr) + bits<26,25>(m_PhysAddr) + bits<28,27>(m_PhysAddr) + bits<30,29>(m_PhysAddr) +
        bit<31>(m_PhysAddr));

    m_PAKS = (bits<PA_MSB,16>(m_PhysAddr) << 16) |
        (bits<13,11>(m_PhysAddr) << 13) |
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle)) << 12) |

        (((bits<10,9>(m_PhysAddr)
        + bits<1,0>(swizzle)  ) %4) << 10) |

        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |
        (bits<15,14>(m_PhysAddr) << 7) |
        bits<6,0>(m_PhysAddr);
}

void GKLIT1AddressDecode::CommonCalcPAKSPitchLinearPS64KParts3()
{
    UINT32 swizzle = static_cast<UINT32>(bits<12,11>(m_PhysAddr) + bits<14,13>(m_PhysAddr) +  bits<16,15>(m_PhysAddr) +
        bits<18,17>(m_PhysAddr) + bits<20,19>(m_PhysAddr) +  bits<22,21>(m_PhysAddr) +
        bits<24,23>(m_PhysAddr) + bits<26,25>(m_PhysAddr) +  bits<28,27>(m_PhysAddr) +
        bits<30,29>(m_PhysAddr));

    m_PAKS = (bits<PA_MSB,16>(m_PhysAddr) << 16) |
        (bits<13,11>(m_PhysAddr) << 13) |
        // partitions
        (bits<10,9>(m_PhysAddr) << 11) |

        // slice[1:0]
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        (bits<15,14>(m_PhysAddr) << 7) |
        bits<6,0>(m_PhysAddr);
}

void GKLIT1AddressDecode::CommonCalcPAKSPitchLinearPS64KParts2()
{
    UINT32 swizzle = static_cast<UINT32>(bits<11,10>(m_PhysAddr) + bits<13,12>(m_PhysAddr) + bits<15,14>(m_PhysAddr) +
        bits<17,16>(m_PhysAddr) + bits<19,18>(m_PhysAddr) + bits<21,20>(m_PhysAddr) + bits<23,22>(m_PhysAddr) +
        bits<25,24>(m_PhysAddr) + bits<27,26>(m_PhysAddr) + bits<29,28>(m_PhysAddr) + bits<31,30>(m_PhysAddr)
        );

    m_PAKS = (bits<PA_MSB,16>(m_PhysAddr) << 16) |
        (bits<13,10>(m_PhysAddr) << 12) |
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle)) << 11) |

        (((bit<9>(m_PhysAddr) + bit<10>(m_PhysAddr) + bit<11>(m_PhysAddr) + bit<12>(m_PhysAddr) + bit<13>(m_PhysAddr)
        + bit<14>(m_PhysAddr) + bit<15>(m_PhysAddr) + bit<16>(m_PhysAddr) + bit<17>(m_PhysAddr) + bit<18>(m_PhysAddr)
        + bit<19>(m_PhysAddr) + bit<20>(m_PhysAddr) + bit<21>(m_PhysAddr) + bit<22>(m_PhysAddr) + bit<23>(m_PhysAddr)
        + bit<24>(m_PhysAddr) + bit<25>(m_PhysAddr) + bit<26>(m_PhysAddr) + bit<27>(m_PhysAddr) + bit<28>(m_PhysAddr)
        + bit<29>(m_PhysAddr) + bit<30>(m_PhysAddr) + bit<31>(m_PhysAddr)) %2) << 10) |

        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        (bits<15,14>(m_PhysAddr) << 7) |
        bits<6,0>(m_PhysAddr);
}

void GKLIT1AddressDecode::CommonCalcPAKSPitchLinearPS64KParts1()
{
    UINT32 swizzle = static_cast<UINT32>(bits<10,9>(m_PhysAddr) + bits<12,11>(m_PhysAddr) + bits<14,13>(m_PhysAddr) +
        bits<16,15>(m_PhysAddr) + bits<18,17>(m_PhysAddr) + bits<20,19>(m_PhysAddr) + bits<22,21>(m_PhysAddr) +
        bits<24,23>(m_PhysAddr) + bits<26,25>(m_PhysAddr) + bits<28,27>(m_PhysAddr) + bits<30,29>(m_PhysAddr) + bit<31>(m_PhysAddr)
        );

    m_PAKS = (bits<PA_MSB,16>(m_PhysAddr) << 16) |
        (bits<13,9>(m_PhysAddr) << 11) |

        // slice[1:0]
        ((bit<8>(m_PhysAddr) ^ bit<1>(swizzle)) << 10) |
        ((bit<7>(m_PhysAddr) ^ bit<0>(swizzle)) << 9) |

        (bits<15,14>(m_PhysAddr) << 7) |
        bits<6,0>(m_PhysAddr);
}

void GKLIT1AddressDecode::ComputePAKS1()
{
    UINT32 swizzle_ps = static_cast<UINT32>((bit<20>(m_PhysAddr) <<1) + (bit<19>(m_PhysAddr) <<2) + bits<23,21>(m_PhysAddr) + bits<26,24>(m_PhysAddr)
        + bits<29,27>(m_PhysAddr) + bits<32,30>(m_PhysAddr) + bits<35,33>(m_PhysAddr) + bit<36>(m_PhysAddr));
    m_PAKS = (bits<PA_MSB,12>(m_PhysAddr) << 12) |
        ((bit<11>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ bit<1>(swizzle_ps)) << 11) |
        ((bit<10>(m_PhysAddr) ^ bit<14>(m_PhysAddr) ^ bit<0>(swizzle_ps)) << 10) |
        // slice0
        ((bit<9>(m_PhysAddr) ^ bit<12>(m_PhysAddr) ^ bit<13>(m_PhysAddr) ^ bit<2>(swizzle_ps)) << 9) |
        bits<8,0>(m_PhysAddr);
}

void GKLIT1AddressDecode::ComputeSlice1()
{
    m_Slice = static_cast<UINT32>(((bit<0>(m_Quotient) ^ bit<1>(m_Quotient) ^ bit<2>(m_Quotient) ^ bit<3>(m_Quotient) ^ bit<4>(m_Quotient) ^  bit<5>(m_Quotient) ^ bit<6>(m_Quotient) ^ bit<7>(m_Quotient) ^ bit<8>(m_Quotient)) << 1)
        | bit<9>(m_Offset));
}

void GKLIT1AddressDecode::ComputeSlice2()
{
    m_Slice = static_cast<UINT32>(((bit<0>(m_Quotient) ^ bit<3>(m_Quotient) ^ bit<4>(m_Quotient) ^  bit<5>(m_Quotient) ^ bit<6>(m_Quotient) ^ bit<7>(m_Quotient) ^ bit<8>(m_Quotient)) << 1)
        | bit<9>(m_Offset));
}

void GKLIT1AddressDecode::ComputeSlice3()
{
    m_Slice = static_cast<UINT32>(((bit<0>(m_Quotient) ^ bit<4>(m_Quotient) ^  bit<5>(m_Quotient) ^ bit<6>(m_Quotient) ^ bit<7>(m_Quotient) ^ bit<8>(m_Quotient)) << 1)
        | bit<9>(m_Offset));
}

void GKLIT1AddressDecode::ComputeSlice4()
{
    m_Slice = static_cast<UINT32>(((bit<0>(m_Quotient) ^ bit<2>(m_Quotient) ^ bit<3>(m_Quotient) ^ bit<4>(m_Quotient) ^  bit<5>(m_Quotient) ^ bit<6>(m_Quotient) ^ bit<7>(m_Quotient) ^ bit<8>(m_Quotient)) << 1)
        | bit<9>(m_Offset));
}

void GKLIT1AddressDecode::DefaultPAKS()
{
    switch (m_NumPartitions)
    {
    case 0xa:
        ComputePAKS1();
        break;
    case 0x4:
        DefaultPAKSParts4();
        break;
    case 0x3:
        DefaultPAKSParts3();
        break;
    case 0x2:
        DefaultPAKSParts2();
        break;
    case 0x1:
        DefaultPAKSParts1();
        break;
    default:
        Printf(Tee::PriHigh, "DecodeAddress: "
            "Unsupported partition number - 0x%x!\n",
            m_NumPartitions);
        MASSERT(!"Unsupported partition number!");
    }
}

void GKLIT1AddressDecode::DefaultPAKSParts4()
{
    switch (m_PteKind)
    {
    case LW_MMU_PTE_KIND_GENERIC_16BX2:
    // case FermiPhysicalAddress::BLOCKLINEAR_Z64:
        switch (m_PageSize)
        {
        case 4:
            CommonCalcPAKSBlockLinearPS4KParts4();
            break;
        case 64:
        case 128:
            CommonCalcPAKSBlockLinearPS64KParts4();
            break;
        default:
            Printf(Tee::PriHigh, "DecodeAddress: "
                "Unsupported page size - 0x%x!\n",
                m_PageSize);
            MASSERT(!"Unsupported page size!");
            break;
        }
        break;
    case LW_MMU_PTE_KIND_PITCH:
        switch (m_PageSize)
        {
        case 4:
            CommonCalcPAKSPitchLinearPS4KParts4();
            break;
        case 64:
        case 128:
            CommonCalcPAKSPitchLinearPS64KParts4();
            break;
        default:
            Printf(Tee::PriHigh, "DecodeAddress: "
                "Unsupported page size - 0x%x!\n",
                m_PageSize);
            MASSERT(!"Unsupported page size!");
            break;
        }
        break;
    case LW_MMU_PTE_KIND_PITCH_NO_SWIZZLE:
        switch (m_PageSize)
        {
        case 4:
            CommonCalcPAKSCommonPS4KParts4();
            break;
        case 64:
        case 128:
            m_PAKS = m_PhysAddr;
            break;
        default:
            Printf(Tee::PriHigh, "DecodeAddress: "
                "Unsupported page size - 0x%x!\n",
                m_PageSize);
            MASSERT(!"Unsupported page size!");
            break;
        }
        break;
    default:
        Printf(Tee::PriHigh, "DecodeAddress: "
            "Unsupported PTE kind - 0x%02x!\n",
            m_NumPartitions);
        MASSERT(!"Unsupported PTE kind!");
    }
}

void GKLIT1AddressDecode::DefaultPAKSParts3()
{
    switch (m_PteKind)
    {
    case LW_MMU_PTE_KIND_GENERIC_16BX2:
    //case FermiPhysicalAddress::BLOCKLINEAR_Z64:
        switch (m_PageSize)
        {
        case 4:
            CommonCalcPAKSBlockLinearPS4KParts3();
            break;
        case 64:
        case 128:
            CommonCalcPAKSBlockLinearPS64KParts3();
            break;
        default:
            Printf(Tee::PriHigh, "DecodeAddress: "
                "Unsupported page size - 0x%x!\n",
                m_PageSize);
            MASSERT(!"Unsupported page size!");
            break;
        }
        break;
    case LW_MMU_PTE_KIND_PITCH:
        switch (m_PageSize)
        {
        case 4:
            CommonCalcPAKSPitchLinearPS4KParts3();
            break;
        case 64:
        case 128:
            CommonCalcPAKSPitchLinearPS64KParts3();
            break;
        default:
            Printf(Tee::PriHigh, "DecodeAddress: "
                "Unsupported page size - 0x%x!\n",
                m_PageSize);
            MASSERT(!"Unsupported page size!");
            break;
        }
        break;
    case LW_MMU_PTE_KIND_PITCH_NO_SWIZZLE:
        switch (m_PageSize)
        {
        case 4:
            CommonCalcPAKSCommonPS4KParts3();
            break;
        case 64:
        case 128:
            m_PAKS = m_PhysAddr;
            break;
        default:
            Printf(Tee::PriHigh, "DecodeAddress: "
                "Unsupported page size - 0x%x!\n",
                m_PageSize);
            MASSERT(!"Unsupported page size!");
            break;
        }
        break;
    default:
        Printf(Tee::PriHigh, "DecodeAddress: "
            "Unsupported PTE kind - 0x%02x!\n",
            m_NumPartitions);
        MASSERT(!"Unsupported PTE kind!");
    }
}

void GKLIT1AddressDecode::DefaultPAKSParts2()
{
    switch (m_PteKind)
    {
    case LW_MMU_PTE_KIND_GENERIC_16BX2:
    //case FermiPhysicalAddress::BLOCKLINEAR_Z64:
        switch (m_PageSize)
        {
        case 4:
            CommonCalcPAKSBlockLinearPS4KParts2();
            break;
        case 64:
        case 128:
            CommonCalcPAKSBlockLinearPS64KParts2();
            break;
        default:
            Printf(Tee::PriHigh, "DecodeAddress: "
                "Unsupported page size - 0x%x!\n",
                m_PageSize);
            MASSERT(!"Unsupported page size!");
            break;
        }
        break;
    case LW_MMU_PTE_KIND_PITCH:
        switch (m_PageSize)
        {
        case 4:
            CommonCalcPAKSPitchLinearPS4KParts2();
            break;
        case 64:
        case 128:
            CommonCalcPAKSPitchLinearPS64KParts2();
            break;
        default:
            Printf(Tee::PriHigh, "DecodeAddress: "
                "Unsupported page size - 0x%x!\n",
                m_PageSize);
            MASSERT(!"Unsupported page size!");
            break;
        }
        break;
    case LW_MMU_PTE_KIND_PITCH_NO_SWIZZLE:
        switch (m_PageSize)
        {
        case 4:
        case 64:
        case 128:
            m_PAKS = m_PhysAddr;
            break;
        default:
            Printf(Tee::PriHigh, "DecodeAddress: "
                "Unsupported page size - 0x%x!\n",
                m_PageSize);
            MASSERT(!"Unsupported page size!");
            break;
        }
        break;
    default:
        Printf(Tee::PriHigh, "DecodeAddress: "
            "Unsupported PTE kind - 0x%02x!\n",
            m_NumPartitions);
        MASSERT(!"Unsupported PTE kind!");
    }
}

void GKLIT1AddressDecode::DefaultPAKSParts1()
{
    switch (m_PteKind)
    {
    case LW_MMU_PTE_KIND_GENERIC_16BX2:
        //case FermiPhysicalAddress::BLOCKLINEAR_Z64:
        switch (m_PageSize)
        {
        case 4:
            if((m_MappingMode & 0x80) == 0x80 )  // changing the slice stride to 1K for gk107
                CommonCalcPAKSBlockLinearPS4KParts11KSliceStride();
            else
                CommonCalcPAKSBlockLinearPS4KParts1();
            break;
        case 64:
        case 128:
            if((m_MappingMode & 0x80) == 0x80 )  // changing the slice stride to 1K for gk107
                CommonCalcPAKSBlockLinearPS64KParts11KSliceStride();
            else
                CommonCalcPAKSBlockLinearPS64KParts1();
            break;
        default:
            Printf(Tee::PriHigh, "DecodeAddress: "
                "Unsupported page size - 0x%x!\n",
                m_PageSize);
            MASSERT(!"Unsupported page size!");
            break;
        }
        break;
      case LW_MMU_PTE_KIND_PITCH:
        switch (m_PageSize)
        {
        case 4:
            CommonCalcPAKSPitchLinearPS4KParts1();
            break;
        case 64:
        case 128:
            CommonCalcPAKSPitchLinearPS64KParts1();
            break;
        default:
            Printf(Tee::PriHigh, "DecodeAddress: "
                "Unsupported page size - 0x%x!\n",
                m_PageSize);
            MASSERT(!"Unsupported page size!");
            break;
        }
        break;
    case LW_MMU_PTE_KIND_PITCH_NO_SWIZZLE:
        switch (m_PageSize)
        {
        case 4:
        case 64:
        case 128:
            m_PAKS = m_PhysAddr;
            break;
        default:
            Printf(Tee::PriHigh, "DecodeAddress: "
                "Unsupported page size - 0x%x!\n",
                m_PageSize);
            MASSERT(!"Unsupported page size!");
            break;
        }
        break;
    default:
        Printf(Tee::PriHigh, "DecodeAddress: "
            "Unsupported PTE kind - 0x%02x!\n",
            m_NumPartitions);
        MASSERT(!"Unsupported PTE kind!");
    }
}

void GKLIT1AddressDecode::CommonCalcQRO()
{
    m_Offset = m_PAKS % m_PartitionInterleave;
    UINT64 tmp = m_PAKS / m_PartitionInterleave;
    m_Quotient = tmp / m_NumPartitions;
    m_Remainder = tmp % m_NumPartitions;
}

void GKLIT1AddressDecode::CommonCalcSliceParts842()
{
    m_Slice = static_cast<UINT32>((bit<0>(m_Quotient) << 1) | bit<9>(m_Offset));
}

void GKLIT1AddressDecode::CommonCalcSliceParts1()
{
   m_Slice = static_cast<UINT32>(bit<0>(m_Quotient)); // GC modified
}

void GKLIT1AddressDecode::CommonCalcSliceParts3()
{
    m_Slice = bits<10,9>(m_Offset);
}

void GKLIT1AddressDecode::DefaultSlice()
{
    switch (m_NumPartitions)
    {
    case 4:
    case 2:
        CommonCalcSliceParts842();
        break;
    case 1:
        CommonCalcSliceParts1();
        break;
    case 3:
        CommonCalcSliceParts3();
        break;
    default:
        Printf(Tee::PriHigh, "DecodeAddress: "
            "Unsupported partition number - 0x%x!\n",
            m_NumPartitions);
        MASSERT(!"Unsupported partition number!");
    }
}

void GKLIT1AddressDecode::DefaultPart()
{
    switch (m_NumPartitions)
    {
    case 4:
    case 3:
    case 2:
        m_Partition = m_Remainder;
        break;
    case 1:
        m_Partition = 0;
        break;
    default:
        Printf(Tee::PriHigh, "DecodeAddress: "
            "Unsupported partition number - 0x%x!\n",
            m_NumPartitions);
        MASSERT(!"Unsupported partition number!");
    }
}

void GKLIT1AddressDecode::CommonCalcL2AddressParts3()
{
    // swizzle bank and upper 2 bits L2 index
    UINT32 swizzle_20 = static_cast<UINT32>(bit<3>(m_Quotient) + (bit<5>(m_Quotient) << 1) + bits<10,8>(m_Quotient) + bits<13,11>(m_Quotient) + bits<16,14>(m_Quotient) + bits<19,17>(m_Quotient) + bits<21,20>(m_Quotient));
    // rotates set of 4 L2 lines in a partition tile
    //  UINT32 swizzle_43 = bit<2>(m_Quotient) + bit<8>(m_Quotient) + bit<14>(m_Quotient) + bit<20>(m_Quotient) + ((bit<5>(m_Quotient) + bit<11>(m_Quotient) + bit<17>(m_Quotient)) << 1);
    UINT32 swizzle_43 = static_cast<UINT32>(bits<3,2>(m_Quotient) + (bit<6>(m_Quotient) << 1) + bit<7>(m_Quotient) + bit<13>(m_Quotient) + bit<19>(m_Quotient) + ((bit<10>(m_Quotient) + bit<16>(m_Quotient) + bit<21>(m_Quotient)) << 1));
    UINT32 bank = static_cast<UINT32>(bits<2,0>(m_Quotient) + bits<2,0>(swizzle_20));
    m_L2Set = static_cast<UINT32>((bit<7>(m_Offset) ^ bit<1>(swizzle_43)) |
        ((bit<8>(m_Offset) ^ bit<0>(swizzle_43)) << 1) |
        (bits<1,0>(bank) << 2));
    m_L2Tag = static_cast<UINT32>(bit<2>(bank) | ((m_Quotient >> 3) << 1));
    CalcL2Sector();
}

void GKLIT1AddressDecode::CommonCalcL2AddressParts421()
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

void GKLIT1AddressDecode::DefaultL2Address()
{
    switch (m_NumPartitions)
    {
    case 4:
    case 2:
    case 1:
        CommonCalcL2AddressParts421();
        break;
    case 3:
        CommonCalcL2AddressParts3();
        break;
    default:
        Printf(Tee::PriHigh, "DecodeAddress: "
            "Unsupported partition number - 0x%x!\n",
            m_NumPartitions);
        MASSERT(!"Unsupported partition number!");
    }
}

bool GKLIT1AddressDecode::CalcRBCSlices4L2Sets16DRAMBanks4Bool()
{
    if (RBSCCommon())
    {
        return false;
    }

    CalcRBCSlices4Common();
    UINT32 numColBits = ComputeColumnBits();
    UINT32 rb = bits<31,3>(m_Padr);
    UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);

    m_Bank = tmp % m_NumDRAMBanks;
    m_ExtBank = tmp / m_NumDRAMBanks;
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);

    return true;
}

bool GKLIT1AddressDecode::CalcRBCSlices4L2Sets16DRAMBanks8Bool()
{
    if (RBSCCommon())
    {
        return false;
    }

    CalcRBCSlices4Common();
    UINT32 numColBits = ComputeColumnBits();
    UINT32 rb = bits<31,3>(m_Padr);
    UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);

    m_Bank = bit<2>(tmp) |
        (bit<1>(tmp) << 1) |
        (bit<0>(tmp) << 2);
    //    m_bank = tmp % m_number_dram_banks;   // Wayne: overriding the bank swizzle temporarily
    m_ExtBank = bit<3>(tmp);
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);

    return true;
}

/*virtual*/ bool GKLIT1AddressDecode::CalcRBCSlices4L2Sets16DRAMBanks16Bool()
{
    if (RBSCCommon())
    {
        return false;
    }

    CalcRBCSlices4Common();
    UINT32 numColBits = ComputeColumnBits();
    UINT32 rb = bits<31,3>(m_Padr);
    UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);

    // FT swizzles the bank bits
    m_Bank = (bit<3>(tmp) << 3) |
        (bit<0>(tmp) << 2) |
        (bit<1>(tmp) << 1) |
        (bit<2>(tmp) << 0);

    // 8x2 bank grouping
    if(m_8BgSwizzle)
    {
        m_Bank = (bit<3>(m_Bank) << 3) |
            (bit<2>(m_Bank) << 2) |
            (bit<0>(m_Bank) << 1) |
            (bit<1>(m_Bank) << 0);
    }

    m_ExtBank = bit<4>(tmp);
    m_Row = rb / (m_NumDRAMBanks << m_NumExtDRAMBanks); // includes "column-as-row" bits

    // m_Col[c-1:8] = m_Row[c-9:0], where c > 8
    m_Col = ((m_Row << 8) | m_Col) & ((1 << numColBits) - 1);
    m_Row = m_Row >> (numColBits - 8);

    return true;
}

void GKLIT1AddressDecode::CalcRBCSlices4Common()
{
    m_Col = (bits<2,0>(m_Padr) << 5) | bits<6,2>(m_L2Offset);
}

bool GKLIT1AddressDecode::RBSCCommon()
{
    if (m_NumXbarL2Slices != 4)
    {
        return true;
    }
    if((m_MappingMode & 0x40) == 0x40  && (m_NumPartitions == 1))
    { // 128b gk107
        m_SubPart = bit<0>(m_Slice);
    }
    else
    {  // 64b case: inject slice bit into the padr field for case where 2 slices feed 1 subpa
        m_Padr =  ((m_Padr & ~3) << 1) | (bit<0>(m_Slice) << 2) | bits<1,0>(m_Padr);
        m_SubPart = bit<1>(m_Slice);
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
    return false;
}

UINT32 GKLIT1AddressDecode::ComputeECCAddr()
{
    UINT32 bank_bits, col_bit_8, row_bits, comp_padr = 0, mult_padr, ecc_padr = 0;
    UINT32 total_banks = m_NumDRAMBanks << m_NumExtDRAMBanks;

    UINT32 col_bits_2_0 = bits<2,0>(m_Padr);

    if(m_ECCPageSize > 1024)
    {
        switch(total_banks)
        {
        case 4:
            bank_bits = bits<4,3>(m_Padr);
            col_bit_8 = bit<5>(m_Padr);
            row_bits  = m_Padr >> 6;
            comp_padr = (row_bits << 6) | (bank_bits << 4) | (col_bit_8 << 3) | col_bits_2_0;
            break;
        case 8:
            bank_bits = bits<5,3>(m_Padr);
            col_bit_8 = bit<6>(m_Padr);
            row_bits  = m_Padr >> 7;
            comp_padr = (row_bits << 7) | (bank_bits << 4) | (col_bit_8 << 3) | col_bits_2_0;
            break;
        case 16:
            bank_bits = bits<6,3>(m_Padr);
            col_bit_8 = bit<7>(m_Padr);
            row_bits  = m_Padr >> 8;
            comp_padr = (row_bits << 8) | (bank_bits << 4) | (col_bit_8 << 3) | col_bits_2_0;
            break;
        case 32:
            bank_bits = bits<7,3>(m_Padr);
            col_bit_8 = bit<8>(m_Padr);
            row_bits  = m_Padr >> 9;
            comp_padr = (row_bits << 9) | (bank_bits << 4) | (col_bit_8 << 3) | col_bits_2_0;
            break;
        }
    }
    else
    {
        comp_padr = m_Padr;
    }
    mult_padr = (comp_padr / m_NumECCLinePerPage) * m_NumLTCLinePerPage | (comp_padr % m_NumECCLinePerPage);
    if(m_ECCPageSize > 1024)
    {
        col_bits_2_0 = bits<2,0>(mult_padr);
        col_bit_8 = bit<3>(mult_padr);
        switch(total_banks)
        {
        case 4:
            bank_bits = bits<5,4>(mult_padr);
            row_bits  = mult_padr >> 6;
            ecc_padr = (row_bits << 6) | (col_bit_8 << 5) | (bank_bits << 3) | col_bits_2_0;
            break;
        case 8:
            bank_bits = bits<6,4>(mult_padr);
            row_bits  = mult_padr >> 7;
            ecc_padr = (row_bits << 7) | (col_bit_8 << 6) | (bank_bits << 3) | col_bits_2_0;
            break;
        case 16:
            bank_bits = bits<7,4>(mult_padr);
            row_bits  = mult_padr >> 8;
            ecc_padr = (row_bits << 8) | (col_bit_8 << 7) | (bank_bits << 3) | col_bits_2_0;
            break;
        case 32:
            bank_bits = bits<8,4>(mult_padr);
            row_bits  = mult_padr >> 9;
            ecc_padr = (row_bits << 9) | (col_bit_8 << 8) | (bank_bits << 3) | col_bits_2_0;
            break;
        }
    }
    else
    {
        ecc_padr = mult_padr;
    }
    return ecc_padr;
}
