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

#pragma once
#ifndef INCLUDED_GK107ADR_H
#define INCLUDED_GK107ADR_H

#include "gklit1adr.h"

/**
 * @class( GK107AddressDecode )
 *
 * Class for decoding GK107 physical addresses.
 */

class GK107AddressDecode : public GKLIT1AddressDecode
{
public:
    GK107AddressDecode(
        UINT32 NumPartitions,
        GpuSubdevice* pSubdev
        );
    virtual ~GK107AddressDecode();
    virtual void PrintDecodeDetails(UINT32 PteKind, UINT32 PageSize);

protected:
    virtual bool CalcRBCSlices4L2Sets16DRAMBanks16Bool();
private:
};

#endif // INCLUDED_GK107ADR_H
