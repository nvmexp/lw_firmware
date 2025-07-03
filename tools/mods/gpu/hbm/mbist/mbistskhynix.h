/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDE_SKHYNIX_MBIST_H
#define INCLUDE_SKHYNIX_MBIST_H

#include "mbist.h"

class SKHynixMBist : public MBistImpl
{
public:
    SKHynixMBist(HBMDevice* pHbmDev, bool useHostToJtag, UINT32 stackHeight) :
        MBistImpl(pHbmDev, useHostToJtag, stackHeight)
    {}

    RC StartMBist(const UINT32 siteID, const UINT32 mbistType) override;
    RC CheckCompletionStatus
    (
        const UINT32 siteID
        ,const UINT32 mbistType
    ) override;

    RC SoftRowRepair
    (
        const UINT32 siteID
        ,const UINT32 stackID
        ,const UINT32 channel
        ,const UINT32 row
        ,const UINT32 bank
        ,const bool firstRow
        ,const bool lastRow
    ) override
        { return RC::UNSUPPORTED_FUNCTION; }

    RC HardRowRepair
    (
        const bool isPseudoRepair
        ,const UINT32 siteID
        ,const UINT32 stackID
        ,const UINT32 channel
        ,const UINT32 row
        ,const UINT32 bank
    ) const override
        { return RC::UNSUPPORTED_FUNCTION; }

    RC DidJtagResetOclwrAfterSWRepair(bool* pReset) const override
        { return RC::UNSUPPORTED_FUNCTION; }

    RC GetNumSpareRowsAvailable
    (
        const UINT32 siteID
        ,const UINT32 stackID
        ,const UINT32 channel
        ,const UINT32 bank
        ,UINT32* pNumAvailable
        ,bool doPrintResults = true
    ) const override
        { return RC::UNSUPPORTED_FUNCTION; }
};

#endif
