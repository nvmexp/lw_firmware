/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011,2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// The contents of this file have been copied from:
// //hw/kepler1_glit1/clib/lwshared/Memory/gklit1_chip.h
// //hw/kepler1_gklit1/clib/lwshared/Memory/gk107.h
//

#include "gf100adr.h"
#include "gk107adr.h"
#include "bitop.h"

#include "kepler/gk107/dev_mmu.h"
#include "kepler/gk107/dev_fbpa.h"
#include "kepler/gk107/dev_ltc.h"

using namespace BitOp;

#define PA_MSB 39

GK107AddressDecode::GK107AddressDecode
(
    UINT32 NumPartitions,
    GpuSubdevice* pSubdev
)
: GKLIT1AddressDecode(NumPartitions, pSubdev)
{
    m_NumL2Sets = 16; //GK107 specific
    m_NumL2SetsBits = 4;
}

GK107AddressDecode::~GK107AddressDecode()
{
}

void GK107AddressDecode::PrintDecodeDetails(UINT32 PteKind, UINT32 PageSize)
{
    Printf(Tee::PriNormal, "\n++Addr= 0x%x Kind = 0x%x PageSize=%uK\n"
            , unsigned(m_PhysAddr), PteKind, PageSize);
    Printf(Tee::PriNormal, "++Q:R:O:PAKS=0x%llx:0x%x:0x%x:0x%llx\n",
            m_Quotient,m_Remainder,m_Offset,m_PAKS);
    Printf(Tee::PriNormal, "++slice:Partition:P.I.=%u:%u:%x\n",m_Slice, m_Partition
            ,m_PartitionInterleave);
    Printf(Tee::PriNormal, "++L2  Set:Bank:Tag:Sector:Offset 0x%x:0x%x:0x%x:0x%x:0x%x\n ",
        m_L2Set, m_L2Bank, m_L2Tag, m_L2Sector, m_L2Offset);
    Printf(Tee::PriNormal, "++Extbank:R:B:S:C= %u:%x:%x:%x:%x\n",
        m_ExtBank, m_Row, m_Bank, m_SubPart, m_Col);

}

bool GK107AddressDecode::CalcRBCSlices4L2Sets16DRAMBanks16Bool()
{
    if (RBSCCommon()) //GKLITX RBSC
    {
        return false;
    }

    CalcRBCSlices4Common();
    UINT32 numColBits = ComputeColumnBits();
    UINT32 rb = bits<31,3>(m_Padr);
    UINT32 tmp = rb % (m_NumDRAMBanks << m_NumExtDRAMBanks);

    // FT swizzles the bank bits
    // True for all GK10x chips
        m_Bank =  bit<2>(tmp) |
            (bit<3>(tmp) << 1) |
            (bit<1>(tmp) << 2) |
            (bit<0>(tmp) << 3);
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
