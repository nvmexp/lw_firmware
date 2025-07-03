/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2013,2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_GKLIT2ADR_H
#define INCLUDED_GKLIT2ADR_H

#include "gklit1adr.h"

/**
 * @class( GKLIT2AddressDecode )
 *
 * Class for decoding GKLIT2 physical addresses.
 */

class GKLIT2AddressDecode : public GKLIT1AddressDecode
{
public:
    GKLIT2AddressDecode(
        UINT32 NumPartitions,
        GpuSubdevice* pSubdev
        );
    virtual ~GKLIT2AddressDecode();

protected:
    virtual void ComputePAKS();
    virtual void ComputeSlice();
    virtual void ComputePartition();
    virtual void ComputeL2Address();

protected:
    // PAKS
    void CommonCalcPAKSBlockLinear();
    void CommonCalcPAKSPitchLinear();
    void CommonCalcPAKSPitchNoSwizzle();

    void CalcPAKSPitchLinearPS4KParts6();
    void CalcPAKSPitchLinearPS4KParts5();
    void CalcPAKSBlockLinearPS4KParts6();
    void CalcPAKSBlockLinearPS4KParts5();
    void CalcPAKSCommonPS4KParts5();
    void CalcPAKSPitchLinearPS64KParts6();
    void CalcPAKSPitchLinearPS64KParts5();
    // Same as GF100AddressDecode::CalcPAKSBlockLinearPS64KParts6
    void CalcPAKSBlockLinearPS64KParts6GF100();
    void CalcPAKSBlockLinearPS64KParts6GK110();
    // Same as GF100AddressDecode::CalcPAKSBlockLinearPS64KParts5
    void CalcPAKSBlockLinearPS64KParts5GF100();
    void CalcPAKSBlockLinearPS64KParts5GK110();

    // Slice
    void CalcSliceSlices4Parts6();

    // Partition
    void CalcPartParts6();

    // L2 address
    void CalcL2SetSlices4L2Set16();
    void CalcL2SetSlices4L2Set32();
    // Same as GF100AddressDecode::CalcL2SetSlices4L2Sets16Parts876421
    void CalcL2SetSlices4L2Sets16Parts6GF100();
    // Same as GF100AddressDecode::CalcL2SetSlices4L2Sets16Parts53
    void CalcL2SetSlices4L2Sets16Parts5GF100();
    // Same as GF100AddressDecode::CalcL2SetSlices4L2Sets32Parts876421
    void CalcL2SetSlices4L2Sets32Parts6GF100();
    // Same as GF100AddressDecode::CalcL2SetSlices4L2Sets32Parts53
    void CalcL2SetSlices4L2Sets32Parts5GF100();
};

class GKLIT2AddressEncode
{
public:

    GKLIT2AddressEncode(
        UINT32 NumPartitions,
        GpuSubdevice* pSubdev
    );

    UINT64 EncodeAddress(
        const FbDecode& decode,
        UINT32 PteKind,
        UINT32 PageSize,
        bool EccOn);

private:

    // Input data
    UINT32 m_Row;
    UINT32 m_Column;
    UINT32 m_IntBank;
    UINT32 m_ExtBank;
    UINT32 m_SubPartition;
    UINT32 m_Partition;
    UINT32 m_PteKind;
    UINT32 m_PageSize;
    GpuSubdevice *m_pGpuSubdevice;
    UINT32 m_NumPartitions;
    bool m_EccOn;

    // Hardcoded chip configuration
    UINT32 m_NumL2Sets;
    UINT32 m_NumL2SetsBits;

    // Callwlated Data
    UINT32 m_L2Offset;
    UINT32 m_Slice;
    UINT32 m_L2Set;
    UINT32 m_L2Tag;
    UINT64 m_Padr;
    UINT64 m_PAKS;
    UINT64 m_PAKS_9_8;
    UINT64 m_PAKS_7;
    UINT64 m_Quotient;
    UINT32 m_Offset;
    UINT32 m_Remainder;
    UINT32 m_PdsReverseMap;
    UINT64 m_PhysAddr;

    // Dynamic chip configuration
    UINT32 m_NumDRAMBanks;
    UINT32 m_NumExtDRAMBanks;
    UINT32 m_DRAMPageSize;
    bool m_DRAMBankSwizzle;
    UINT32 m_NumXbarL2Slices;
    UINT32 m_AmapColumnSize;

    UINT32 m_FBAColumnBitsAboveBit8;
    UINT32 m_FBADRAMBankBits;

    void MapRBCToXBar();
    void MapXBarToPA();

    void ReverseEccAddr(UINT32 *pRow, UINT32 *pCol, UINT32 *pBank, UINT64* pAdr);

    void CalcPhysAddrPitchLinearPS4KParts1();
    void CalcPhysAddrPitchLinearPS4KParts2();
    void CalcPhysAddrPitchLinearPS4KParts3();
    void CalcPhysAddrPitchLinearPS4KParts4();
    void CalcPhysAddrPitchLinearPS4KParts5();
    void CalcPhysAddrPitchLinearPS4KParts6();
};

#endif // INCLUDED_GKLIT2ADR_H
