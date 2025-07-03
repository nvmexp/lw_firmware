/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012,2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file keplergpupcie.h -- Concrete implemetation classes for KeplerGpuPcie

#pragma once

#ifndef INCLUDED_KEPLERPCIE_H
#define INCLUDED_KEPLERPCIE_H

#include "fermipcie.h"

//--------------------------------------------------------------------
//! \brief GPU PCIE implementation used for Kepler+ GPUs (including GF117).
//!        Maintains much of the functionality from Fermi GPUs
//!
class KeplerPcie : public FermiPcie
{
public:
    KeplerPcie() = default;
    virtual ~KeplerPcie() {}

protected: // Functions from GpuPcie
    RC DoResetErrorCounters() override;

protected: // Functions from FermiGpuPcie
    RC DoGetErrorCounters(PexErrorCounts *pCounts) override;
};

#endif

