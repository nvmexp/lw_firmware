/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2013, 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_GF100ADR_H
#define INCLUDED_GF100ADR_H

#include "gpufb.h"

/**
 * @class( GF100AddressDecode )
 *
 * Class for decoding GF10x physical addresses.
 */

class GF100AddressDecode
{
public:
    GF100AddressDecode(
        UINT32 NumPartitions,
        GpuSubdevice* pSubdev
    );
    virtual void PrintFbConfig();
    void EnableDecodeDetails(bool enable) { m_PrintDecodeDetails = enable; }
    RC DecodeAddress(
        FbDecode* Decode,
        UINT64 PhysicalFbOffset,
        UINT32 PteKind,
        UINT32 PageSize
    );
    virtual ~GF100AddressDecode();

protected:
    static UINT64 PhysicalToRaw(UINT64 PhysicalFbOffset, UINT32 PteKind);

    virtual void ComputePAKS();
    virtual UINT32 ComputePartitionInterleave(UINT32 NumPartitions, GpuSubdevice* pSubdev);
    virtual void ComputeQRO();
    virtual void ComputePartition();
    virtual void ComputeSlice();
    virtual void ComputeL2Address();
    virtual void ComputeRBSC();

    virtual void CalcPAKSPitchLinearPS64KParts2();
    virtual void CalcPAKSPitchLinearPS64KParts1();
    virtual void PrintDecodeDetails(UINT32 PteKind, UINT32 PageSize);

    void CalcPAKSPitchLinearPS4KParts8();
    void CalcPAKSPitchLinearPS4KParts7();
    void CalcPAKSPitchLinearPS4KParts6();
    void CalcPAKSPitchLinearPS4KParts5();
    void CalcPAKSPitchLinearPS4KParts4();
    void CalcPAKSPitchLinearPS4KParts3();
    void CalcPAKSPitchLinearPS4KParts2();
    void CalcPAKSPitchLinearPS4KParts1();
    void CalcPAKSBlockLinearPS4KParts8();
    void CalcPAKSBlockLinearPS4KParts7();
    void CalcPAKSBlockLinearPS4KParts6();
    void CalcPAKSBlockLinearPS4KParts5();
    void CalcPAKSBlockLinearPS4KParts4();
    void CalcPAKSBlockLinearPS4KParts3();
    void CalcPAKSBlockLinearPS4KParts2();
    void CalcPAKSBlockLinearPS4KParts1();
    void CalcPAKSCommonPS4KParts8(UINT64 Addr);
    void CalcPAKSCommonPS4KParts5(UINT64 Addr);
    void CalcPAKSCommonPS4KParts4(UINT64 Addr);
    void CalcPAKSCommonPS4KParts3(UINT64 Addr);
    void CalcPAKSPitchLinearPS64KParts8();
    void CalcPAKSPitchLinearPS64KParts7();
    void CalcPAKSPitchLinearPS64KParts6();
    void CalcPAKSPitchLinearPS64KParts5();
    void CalcPAKSPitchLinearPS64KParts4();
    void CalcPAKSPitchLinearPS64KParts3();
    void CalcPAKSBlockLinearPS64KParts8();
    void CalcPAKSBlockLinearPS64KParts7();
    void CalcPAKSBlockLinearPS64KParts6();
    void CalcPAKSBlockLinearPS64KParts5();
    void CalcPAKSBlockLinearPS64KParts4();
    void CalcPAKSBlockLinearPS64KParts3();
    void CalcPAKSBlockLinearPS64KParts2();
    void CalcPAKSBlockLinearPS64KParts1();

    void CalcL2SetSlices4L2Sets16Parts876421();
    void CalcL2SetSlices4L2Sets16Parts53();
    void CalcL2SetSlices4L2Sets32Parts876421();
    void CalcL2SetSlices4L2Sets32Parts53();
    void CalcL2SetSlices2L2Sets16Parts876421();
    void CalcL2SetSlices2L2Sets16Parts53();
    void CalcL2SetSlices2L2Sets32Parts876421();
    void CalcL2SetSlices2L2Sets32Parts53();

    // Every CalcL2Set* function should make its own call
    // to CalcL2Sector()
    void CalcL2Sector();

    void CalcRBCSlices4L2Sets16DRAMBanks4();
    void CalcRBCSlices4L2Sets16DRAMBanks8();
    void CalcRBCSlices4L2Sets16DRAMBanks16();
    void CalcRBCSlices4L2Sets32DRAMBanks4();
    void CalcRBCSlices4L2Sets32DRAMBanks8();
    void CalcRBCSlices4L2Sets32DRAMBanks16();
    void CalcRBCSlices2L2Sets16DRAMBanks4();
    void CalcRBCSlices2L2Sets16DRAMBanks8();
    void CalcRBCSlices2L2Sets16DRAMBanks16();
    void CalcRBCSlices2L2Sets32DRAMBanks4();
    void CalcRBCSlices2L2Sets32DRAMBanks8();
    void CalcRBCSlices2L2Sets32DRAMBanks16();
    void CalcRBCSlices2L2Sets16DRAMBanks4GF108();
    void CalcRBCSlices2L2Sets16DRAMBanks8GF108();
    void CalcRBCSlices2L2Sets16DRAMBanks16GF108();
    void CalcRBCSlices2L2Sets32DRAMBanks4GF108();
    void CalcRBCSlices2L2Sets32DRAMBanks8GF108();
    void CalcRBCSlices2L2Sets32DRAMBanks16GF108();

    void CalcRBCSlices4Common();
    void CalcRBCSlices2Common();

    UINT32 ComputeColumnBits() const;
    UINT32 XbarRawPaddr() const;

    // Input data
    UINT64 m_PhysAddr;
    UINT32 m_PteKind;
    UINT32 m_PageSize;
    UINT32 m_AmapColumnSize;
    bool m_PrintDecodeDetails;

    // Dynamic chip configuration
    GpuSubdevice *m_pGpuSubdevice;
    UINT32 m_NumPartitions;
    UINT32 m_FbioChannelWidth;
    UINT32 m_NumXbarL2Slices;
    UINT32 m_NumDRAMBanks;
    UINT32 m_NumExtDRAMBanks;
    UINT32 m_DRAMPageSize;
    bool m_DRAMBankSwizzle;
    bool m_ECCOn;
    UINT32 m_BurstSize;
    UINT32 m_BeatSize;
    UINT32 m_PartitionInterleave;

    // Hardcoded chip configuration
    UINT32 m_NumL2Sets;
    UINT32 m_NumL2SetsBits;
    UINT32 m_FBDataBusWidth;
    UINT32 m_NumL2Sectors;
    UINT32 m_NumL2Banks;

    // Callwlated data
    UINT64 m_PAKS;
    UINT64 m_Quotient;
    UINT32 m_Offset;
    UINT32 m_Remainder;
    UINT32 m_Partition;
    UINT32 m_Slice;
    UINT32 m_L2Set;
    UINT32 m_L2Bank;
    UINT32 m_L2Tag;
    UINT32 m_L2Sector;
    UINT32 m_L2Offset;
    UINT32 m_SubPart;
    UINT32 m_Bank;
    UINT32 m_ExtBank;
    UINT32 m_Row;
    UINT32 m_Col;
};

class GF100AddressEncode
{
public:

    GF100AddressEncode(
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
    UINT32 m_Padr;
    UINT64 m_PAKS;
    UINT64 m_PAKS_9_8;
    UINT64 m_PAKS_7;
    UINT64 m_Quotient;
    UINT32 m_Offset;
    UINT32 m_Remainder;
    UINT32 m_Sum;
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

    void CalcPhysAddrPitchLinearPS4KParts1();
    void CalcPhysAddrPitchLinearPS4KParts2();
    void CalcPhysAddrPitchLinearPS4KParts3();
    void CalcPhysAddrPitchLinearPS4KParts4();
    void CalcPhysAddrPitchLinearPS4KParts5();
    void CalcPhysAddrPitchLinearPS4KParts6();
};

#endif // INCLUDED_GF100ADR_H
