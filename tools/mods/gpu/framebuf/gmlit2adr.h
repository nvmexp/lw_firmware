/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_GMLIT2ADR_H
#define INCLUDED_GMLIT2ADR_H
#include "gmlit1adr.h"

#define PA_MSB 39

/**
 * @class( GMLIT2AddressDecode )
 *
 * Class for decoding GMLIT2 physical addresses. Note: The
 * register address's have changed on some of the fields so you
 * can't derive from any of the Kepler or Fermi classes.
 */

class GMLIT2AddressDecode : public  GMLIT1AddressDecode
{
public:
    GMLIT2AddressDecode(GpuSubdevice* pSubdev);
    virtual ~GMLIT2AddressDecode();
    virtual RC DecodeAddress(
            FbDecode* Decode,
            UINT64 PhysicalFbOffset,
            UINT32 PteKind,
            UINT32 PageSize
            );
    void PrintFbConfig(INT32 Pri);
    void PrintDecodeDetails(UINT32 PteKind, UINT32 PageSize);

protected:
    UINT64 PhysicalToRaw(UINT64 physFbOffset, UINT32 pteKind){return physFbOffset;};
    virtual void ComputeQRO();
    virtual void ComputePAKS();
    virtual void ComputePartition();
    UINT32 ComputePI();

    void ComputeSlice();
    void CalcCommonSlice4Local();
    void CalcCommonSlice3Local();
    void CalcCommonSlice2Local();

    virtual void CalcPAKSPitchLinearPS64KParts1();
    virtual void CalcPAKSPitchLinearPS64KParts2();
    virtual void CalcPAKSPitchLinearPS64KParts3();
    virtual void CalcPAKSPitchLinearPS64KParts4();
    virtual void CalcPAKSPitchLinearPS64KParts5();
    virtual void CalcPAKSPitchLinearPS64KParts6();
    virtual void CalcPAKSPitchLinearPS64KParts7();
    virtual void CalcPAKSPitchLinearPS64KParts8();
    virtual void CalcPAKSPitchLinearPS64KParts9();
    virtual void CalcPAKSPitchLinearPS64KParts10();
    virtual void CalcPAKSPitchLinearPS64KParts11();
    virtual void CalcPAKSPitchLinearPS64KParts12();

    virtual void ComputeBlockLinearPAKS();
    virtual UINT64 CommonCalcPAKSBlockLinearParts4Lower();
    virtual UINT64 CommonCalcPAKSPitchLinearParts1Lower();

    virtual UINT64 CommonCalcPAKSParts1(UINT64 Lower);
    virtual UINT64 CommonCalcPAKSParts2(UINT64 Lower);
    virtual UINT64 CommonCalcPAKSParts3(UINT64 Lower);
    virtual UINT64 CommonCalcPAKSParts4(UINT64 Lower);
    virtual UINT64 CommonCalcPAKSParts5(UINT64 Lower);
    virtual UINT64 CommonCalcPAKSParts6(UINT64 Lower);
    virtual UINT64 CommonCalcPAKSParts7(UINT64 Lower);
    virtual UINT64 CommonCalcPAKSParts8(UINT64 Lower);
    virtual UINT64 CommonCalcPAKSParts9(UINT64 Lower);
    virtual UINT64 CommonCalcPAKSParts10(UINT64 Lower);
    virtual UINT64 CommonCalcPAKSParts11(UINT64 Lower);
    virtual UINT64 CommonCalcPAKSParts12(UINT64 Lower);

    void ComputeRBSC();
    void ComputeRBSCParts8Banks8();
    void ComputeRBSCParts8Banks16();
    virtual bool CalcRBCS4Parts1Banks4Bool();
    virtual bool CalcRBCS4Parts1Banks8Bool();
    virtual bool CalcRBCS4Parts1Banks16Bool();
    virtual bool CalcRBCS4Parts2Banks4Bool();
    virtual bool CalcRBCS4Parts2Banks8Bool();
    virtual bool CalcRBCS4Parts2Banks16Bool();
    virtual bool CalcRBCS4Parts4Banks8Bool();
    virtual bool CalcRBCS4Parts4Banks16Bool();
    virtual bool CalcRBCS4Parts8Banks8Bool();
    virtual bool CalcRBCS4Parts8Banks16Bool();
    enum { GMLIT2_OBIT9_LOC_IN_PADR = 2};

private:
    void FindAddressRanges();
    bool ValidatePhysicalFbOffset(UINT64 physFbOffset);
    void ComputeFbPadr();
    UINT32 ComputeEccPadr();
    UINT32 m_NumLtcs;
    UINT32 m_NumUpperLtcs;
    UINT32 m_Partition;
    UINT32 m_LtcId;
    vector <UINT32> m_NumRowBitsPerLtc;
protected:
    bool m_DRAMBankSwizzle;
    UINT32 m_ECCPageSize;
    UINT32 m_EccLinesPerPage;
    UINT32 m_LtcLinesPerPage;
    //Array[ltcId] has corresponding FbPaIdx. FbPaIdx is not physical Fbp number
    vector <UINT32> m_FbpForLtcId;
    //Reverse map to store Ltc for given logical Fbp
    vector <UINT32> m_LtcMaskForLFbp;
    UINT64 m_LowerStart;
    UINT64 m_UpperStart;
    UINT64 m_LowerEnd;
    UINT64 m_UpperEnd;
};

class GMLIT2AddressEncode : public GMLIT1AddressEncode
{
public:
    GMLIT2AddressEncode(GpuSubdevice* pSubdev);
    UINT64 EncodeAddress(
            const FbDecode& decode,
            UINT32 PteKind,
            UINT32 PageSize,
            bool EccOn);
protected:

    void MapRBCToXBar();
    UINT32 BankSwizzle8(UINT64 upperAdr, UINT32 row);
    UINT32 BankSwizzle16(UINT64 upperAdr, UINT32 row);

    virtual void MapXBarToPAKS();

    UINT64 CommonCalcPAKS(UINT64 paValue, UINT32 numLTSPerLTC, UINT32 fbps);
    UINT64 PaksLowerPitchLinearPart8(UINT64 PA_value);

private:
    UINT64 ReverseEccAddr();
    UINT32 GenerateSwizzle(UINT08* galois, UINT32 numSwizzleBits, UINT64 address);

    vector <UINT32> m_LtcMaskForLFbp;
    UINT32 m_LtcId;
    vector <UINT32> m_NumRowBitsPerFbp;
    UINT32 m_EccLinesPerPage;
    UINT32 m_LtcLinesPerPage;

    bool m_FloorsweepingActive;
    bool m_DoubleDensityActive;

};

#endif // INCLUDED_GMLIT2ADR_H
