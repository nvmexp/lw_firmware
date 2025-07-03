/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011,2013,2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_GKLIT1ADR_H
#define INCLUDED_GKLIT1ADR_H

#include "gf100adr.h"

/**
 * @class( GKLIT1AddressDecode )
 *
 * Class for decoding GKLIT1 physical addresses.
 */

class GKLIT1AddressDecode : public GF100AddressDecode
{
public:
    GKLIT1AddressDecode(
        UINT32 NumPartitions,
        GpuSubdevice* pSubdev
        );
    virtual ~GKLIT1AddressDecode();

protected:
    virtual void ComputePAKS();
    virtual void ComputeQRO();
    virtual void ComputePartition();
    virtual void ComputeSlice();
    virtual void ComputeL2Address();
    virtual void ComputeRBSC();
    // RBC
    virtual bool CalcRBCSlices4L2Sets16DRAMBanks4Bool();
    virtual bool CalcRBCSlices4L2Sets16DRAMBanks8Bool();
    virtual bool CalcRBCSlices4L2Sets16DRAMBanks16Bool();
    virtual void CalcRBCSlices4Common();
    virtual bool RBSCCommon();

    virtual void PrintDecodeDetails(UINT32 PteKind, UINT32 PageSize);

protected:
    // PAKS: memory common 4k swizzle
    void CommonCalcPAKSCommonPS4KParts3();
    void CommonCalcPAKSCommonPS4KParts4();

    // PAKS: blocklinear 4K
    void CommonCalcPAKSBlockLinearPS4KParts4();
    void CommonCalcPAKSBlockLinearPS4KParts3();
    void CommonCalcPAKSBlockLinearPS4KParts2();
    void CommonCalcPAKSBlockLinearPS4KParts1();
    void CommonCalcPAKSBlockLinearPS4KParts11KSliceStride();

    // PAKS: blocklinear 64K
    virtual void CommonCalcPAKSBlockLinearPS64KParts4();
    virtual void CommonCalcPAKSBlockLinearPS64KParts3();
    virtual void CommonCalcPAKSBlockLinearPS64KParts2();
    virtual void CommonCalcPAKSBlockLinearPS64KParts1();
    virtual void CommonCalcPAKSBlockLinearPS64KParts11KSliceStride();

    // PAKS: pitchlinear 4K
    void CommonCalcPAKSPitchLinearPS4KParts4();
    void CommonCalcPAKSPitchLinearPS4KParts3();
    void CommonCalcPAKSPitchLinearPS4KParts2();
    void CommonCalcPAKSPitchLinearPS4KParts1();

    // PAKS: pitchlinear 64K
    void CommonCalcPAKSPitchLinearPS64KParts4();
    void CommonCalcPAKSPitchLinearPS64KParts3();
    void CommonCalcPAKSPitchLinearPS64KParts2();
    void CommonCalcPAKSPitchLinearPS64KParts1();

    void ComputePAKS1();

    void DefaultPAKS();
    void DefaultPAKSParts4();
    void DefaultPAKSParts3();
    void DefaultPAKSParts2();
    void DefaultPAKSParts1();

    // Partition inter leave
    void DefaultPartitionInterleave();

    // QRO
    void CommonCalcQRO();

    // Slice
    void ComputeSlice1();
    void ComputeSlice2();
    void ComputeSlice3();
    void ComputeSlice4();

    void CommonCalcSliceParts842();
    void CommonCalcSliceParts1();
    void CommonCalcSliceParts3();
    void DefaultSlice();

    // Partition
    void DefaultPart();

    // L2 address
    void CommonCalcL2AddressParts3();
    void CommonCalcL2AddressParts421();
    void DefaultL2Address();

    UINT32 ComputeECCAddr();

protected:
    UINT32 m_Padr;
    UINT64 m_PALev1;
    UINT32 m_MappingMode;
    UINT32 m_ECCPageSize;
    UINT32 m_NumECCLinePerPage;
    UINT32 m_NumLTCLinePerPage;
    bool m_8BgSwizzle;
};

#endif // INCLUDED_GKLIT1ADR_H
