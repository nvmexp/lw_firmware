/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  perfsub_clientrm.h
 * @brief APIs that maintain various performance settings for CheetAh SOCs
 *
 * Applicable to devices that are SOCs and have client side resman.
 * In other words, the following should be true for devices using this class:
 * (m_pSubdevice->IsSOC() && Platform::HasClientSideResman())
 */

#pragma once
#ifndef INCLUDED_PERFSUB_SOCCLIENTRM_H
#define INCLUDED_PERFSUB_SOCCLIENTRM_H

#include "gpu/perf/perfsub_20.h"

class PerfSocClientRm : public Perf20
{
public:
    PerfSocClientRm(GpuSubdevice* pSubdevice, bool owned);
    ~PerfSocClientRm() override;

    RC GetCoreVoltageMv(UINT32 *pVoltageMv) override;
    RC SetPStateDisabledVoltageMv(UINT32 voltageMv) override;

    RC RestorePStateTable(UINT32 PStateRestoreMask, WhichDomains d) override;

protected:
    RC InnerSetPerfPoint(const PerfPoint &Setting) override;
private:
};

#endif
