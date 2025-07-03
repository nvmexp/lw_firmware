/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_MAXWELLPCIE_H
#define INCLUDED_MAXWELLPCIE_H

#include "keplerpcie.h"

class MaxwellPcie : public KeplerPcie
{
public:
    MaxwellPcie() = default;
    virtual ~MaxwellPcie() {}

protected:
    RC DoGetAspmCyaL1SubState(UINT32 *pState) override;
    RC DoGetAspmEntryCounts(Pcie::AspmEntryCounters *pCounts) override;
    RC DoGetAspmL1SSCapability(UINT32 *pCap) override;
    RC DoGetAspmL1ssEnabled(UINT32 *state) override;
    UINT32 DoGetL1SubstateOffset() override;
    RC DoGetLTREnabled(bool *pEnabled) override;
    RC DoResetAspmEntryCounters() override;
};

#endif

