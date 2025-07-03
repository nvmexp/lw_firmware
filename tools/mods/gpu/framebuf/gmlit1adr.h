/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_GMLIT1ADR_H
#define INCLUDED_GMLIT1ADR_H

#include "gmlitxadr.h"

/**
 * @class( GMLIT1AddressDecode )
 *
 * Class for decoding GMLIT1 physical addresses.
 * Note: The register address's have changed on some of the
 * fields so you can't derive from any of the Kepler or Fermi
 * classes.
 */

class GMLIT1AddressDecode : public  GMlitxFbAddrMap
{
public:
    GMLIT1AddressDecode(GpuSubdevice* pSubdev);
    virtual ~GMLIT1AddressDecode();
    RC DecodeAddress(
        FbDecode* Decode,
        UINT64 PhysicalFbOffset,
        UINT32 PteKind,
        UINT32 PageSize
    );
    void EnableDecodeDetails(bool enable) { m_PrintDecodeDetails = enable; }
protected:
    virtual void PrintDecodeDetails(UINT32 PteKind, UINT32 PageSize);
    virtual UINT32 XbarRawPaddr() const;
    virtual void CalcPAKSPitchLinearPS64KParts2();
    virtual void CalcPAKSPitchLinearPS64KParts1();
    virtual void ComputePAKS();
    virtual void ComputePartition();
    virtual bool ComputeFbSubPartition();
    virtual void ComputeSlice();
    virtual void CommonCalcSliceParts8421();
    virtual void ComputeRBSC();
    virtual void CommonCalcPAKSBlockLinearPS64KParts2();
    virtual void CommonCalcPAKSBlockLinearPS64KParts1();
    virtual void CommonCalcPAKSBlockLinearPS4KParts2();
    virtual void CommonCalcPAKSBlockLinearPS4KParts1();

    bool CalcRBCS4Parts2Banks4Bool();
    bool CalcRBCS4Parts2Banks8Bool();
    bool CalcRBCS4Parts2Banks16Bool();
    virtual void Set8BankGroupSwizzle(bool f) { m_8BgSwizzle = f; };

    bool m_PrintDecodeDetails;
};

//AMAP ENCODE
class GMLIT1AddressEncode : public GMlitxFbAddrMap
{
public:
    GMLIT1AddressEncode(GpuSubdevice* pSubdev);
    UINT64 EncodeAddress(
        const FbDecode& decode,
        UINT32 PteKind,
        UINT32 PageSize,
        bool EccOn);

protected:
    UINT32 m_NumPartitions;
    UINT64 m_PAKS_9_8;
    UINT64 m_PAKS_7;

    UINT32 m_FBAColumnBitsAboveBit8;
    UINT32 m_FBADRAMBankBits;

    void CalcColumnBankBits();

    void MapRBCToXBar();
    virtual void MapXBarToPAKS();
    void XBARtoPAKSpart1();
    void MapPAKSToAddress();

    void MapRBCToXBARPart1Banks4() {};
    void MapRBCToXBARPart1Banks8();
    void MapRBCToXBARPart1Banks16();
    void MapRBCToXBARPart2Banks4() {};
    void MapRBCToXBARPart2Banks8();
    void MapRBCToXBARPart2Banks16();
    void CalcSlicesFromQRO(UINT64 Q, UINT64 R, UINT64 O);
};

#endif // INCLUDED_GMLIT1ADR_H
