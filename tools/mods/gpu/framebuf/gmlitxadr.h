/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013, 2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#pragma once
#ifndef INCLUDED_GMLITXADR_H
#define INCLUDED_GMLITXADR_H

#include "gpufb.h"

/**
 * @class( GMlitxFbAddrMap )
 *
 * Class for decoding/encoding GMlitx physical addresses.
 */
class GMlitxFbAddrMap
{
public:
    explicit GMlitxFbAddrMap(GpuSubdevice* pSubdev);
    virtual ~GMlitxFbAddrMap() = default;
    void PrintFbConfig(INT32 pri);
protected:
    // Input data for decode process
    UINT64 m_PhysAddr;
    UINT32 m_PteKind;
    UINT32 m_PageSize;
    UINT32 m_AmapColumnSize;

    // Hardcoded chip configuration
    UINT32 m_NumL2Sets;
    UINT32 m_NumL2SetsBits;
    UINT32 m_FBDataBusWidth;
    UINT32 m_NumL2Sectors;
    UINT32 m_NumL2Banks;

    // Dynamic chip configuration
    GpuSubdevice *m_pGpuSubdevice;
    UINT32 m_NumPartitions;
    UINT32 m_FbioChannelWidth;
    UINT32 m_NumXbarL2Slices;
    UINT32 m_NumDRAMBanks;
    UINT32 m_NumExtDRAMBanks;
    UINT32 m_DRAMPageSize;
    UINT32 m_BurstSize;
    UINT32 m_BeatSize;
    UINT32 m_PartitionInterleave;
    bool m_DRAMBankSwizzle;
    bool m_ECCOn;
    bool m_IsGF10x;
    bool m_IsGF108;
    bool m_IsGF119;

    UINT32 m_L2addrin;
    UINT32 m_L2SectorMsk;
    UINT32 m_Fb_Padr;
    UINT32 m_FbPadr;
    UINT32 m_NumDramsperDramc;
    UINT32 m_Padr;

    // Callwlated data for decode process
    // Input data for Encode process
    UINT64 m_PAKS;
    UINT64 m_Quotient;
    UINT32 m_Offset;
    UINT32 m_Remainder;
    UINT32 m_Partition;
    UINT32 m_Slice;
    UINT32 m_L2Set;
    UINT32 m_L2Bank;
    UINT64 m_L2Tag;
    UINT32 m_L2Sector;
    UINT32 m_L2Offset;
    UINT32 m_SubPart;
    UINT32 m_NumDramsPerDramc;
    UINT32 m_Bank;
    UINT32 m_ExtBank;
    UINT32 m_Row;
    UINT32 m_Col;
    bool m_L2BankSwizzleSetting;
    bool m_8BgSwizzle;

    //Decode Tool
    UINT64 PhysicalToRaw(UINT64 PhysFbOffset, UINT32 PteKind);
    virtual void ComputeQRO();
    UINT32 ComputeXbarRawPaddr();
    virtual bool ComputeFbSubPartition(){return false;};

    //Encode Tool
    virtual void CalcL2Sector(UINT32 Offset);
    virtual void ComputeL2Address(); // Encode
    virtual void CalcL2AddressSet16();
    virtual void CalcL2AddressSet32();
    virtual void CalcL2AddressSet64();
    virtual void CalcL2AddressSet128();
    virtual void CalcL2AddressSet256();
    virtual void CalcL2AddressSet512();
    virtual void CalcL2AddressSet1024();
    UINT32 ComputeColumnBits() const;

    virtual UINT64 ReversePAKSPitchPS64KParts2();
    virtual UINT64 ReversePAKSPitchPS64KParts1();
    virtual UINT64 ReversePAKSBlocklinearPS64KParts1();
    virtual UINT64 ReversePAKSBlocklinearPS64KParts2();

    virtual UINT32 ComputePartitionInterleave(UINT32 NumPartitions);
    // swizzling for bank addr reduced due to timing issues
    enum { NUM_GALOIS_BANK_SWIZZLE_BITS = 17
          ,NUM_GALOIS_BANK_SWIZZLE_BITS_2 = 30
          ,PADDR_MSB = 39};
};
#endif //INCLUDED_GMLITXADR_H
