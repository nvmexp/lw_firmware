/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012,2014,2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#ifndef INCLUDED_GK208ADR_H
#define INCLUDED_GK208ADR_H

#include "gklit2adr.h"

/**
 * @class( GK208AddressDecode )
 *
 * Class for decoding GKLIT2 physical addresses.
 */

class GK208AddressDecode : public GKLIT2AddressDecode
{
public:
    GK208AddressDecode(
        UINT32 NumPartitions,
        GpuSubdevice* pSubdev
        );
    virtual ~GK208AddressDecode();

protected:
    virtual void ComputeL2Address();

protected:
    //PAKS
    void CommonCalcPAKSBlockLinearPS64KParts1();
    void CommonCalcPAKSBlockLinearPS64KParts2();
    // L2 address
    void CalcL2SetSlices4L2Set32();
    void CalcL2SetSlices4L2Set128();
    void CommonCalcL2AddressParts421();

    //RBSC
    bool CalcRBCSlices4L2Sets16DRAMBanks4Bool();
    bool CalcRBCSlices4L2Sets16DRAMBanks8Bool();
    bool CalcRBCSlices4L2Sets16DRAMBanks16Bool();
    bool RBSCCommon();

protected:
    UINT32 m_NumSubPartitions;
};

#endif // INCLUDED_GK208ADR_H
