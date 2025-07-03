/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009, 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
 //! \file host_bar1_perf.h
 //! \brief Definition of a class to test CPU bar 1 bandwidth.
 //!
 //!

#ifndef _FERMI_HOST_BAR1_PERF_H
#define _FERMI_HOST_BAR1_PERF_H

#include "core/include/refparse.h"
#include "mdiag/tests/gpu/host/host_bar1_perf.h"

//! \brief Bar 1 bandwidth perf test
//!
//! This class tests the Bar 1 CPU bandwidth for writes or reads.
class FermiHostBar1PerfTest : public HostBar1PerfTest
{
public:
    //! Constructor
    //!
    //! \param lwgpu Pointer to an LWGpuResource object to use for memory/register access. This pointer will not be freed by HostBar1PerfTest
    FermiHostBar1PerfTest(LWGpuResource * lwgpu);
    //! Destructor
    virtual ~FermiHostBar1PerfTest(void){};

    //! Uses register writes to start the PmCounters
    virtual void StartPmTriggers(void) const;
    //! Uses register writes to stop the PmCounters
    virtual void StopPmTriggers(void) const;
    //! Configures the passed in Host block linear remapping register to remap a surface.
    //!
    //! \param remapper  Remapper to configure.
    //! \param surface   Surface to remap.
    //! \param blSurface Block Linear surface to act as destination.
    //! \param srcSize   Size of the surface to remap in bytes.
    virtual void SetupHostBlRemapper(const UINT32 remapper, const MdiagSurf &surface, const MdiagSurf &blSurface, const UINT32 srcSize) const;
    //! Backs up the current settings of the passed in remapper.
    virtual void BackupBlRemapperRegisters(const UINT32 remapper);
    //! Restores the previous settings of the passed in remapper.
    virtual void RestoreBlRemapperRegisters(const UINT32 remapper) const;
protected:
    //! Pointer to the LW_PBUS_BL_REMAP_1 register manual information
    const RefManualRegister *pRegRemap1;
    //! Pointer to the LW_PBUS_BL_REMAP_2 register manual information
    const RefManualRegister *pRegRemap2;
    //! Pointer to the LW_PBUS_BL_REMAP_4 register manual information
    const RefManualRegister *pRegRemap4;
    //! Pointer to the LW_PBUS_BL_REMAP_6 register manual information
    const RefManualRegister *pRegRemap6;
    //! Pointer to the LW_PBUS_BL_REMAP_7 register manual information
    const RefManualRegister *pRegRemap7;
    UINT32 pmRegOffset;
    UINT32 pmRegStartVal;
    UINT32 pmRegStopVal;
};

#endif // end _FERMI_HOST_BAR1_PERF_H
