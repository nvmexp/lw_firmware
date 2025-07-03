/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef INCLUDED_GMXXXPM_H
#define INCLUDED_GMXXXPM_H
#include "gpu/perfmon/gpupm_p.h"
//------------------------------------------------------------------------------------------------
// This is a thin wrapper around the GF100GpuPerfmon to address register address spacing changes
// on Maxwell. The algorithms for running the experiments are still the same.
class GM10xGpuPerfmon : public GpuPerfmon
{
public:
    GM10xGpuPerfmon(GpuSubdevice *pSubdev);
    virtual ~GM10xGpuPerfmon() { };
    virtual RC CreateL2CacheExperiment
    (
        UINT32 FbpNum,
        UINT32 L2Slice,
        const GpuPerfmon::Experiment **pHandle
    );
};
//------------------------------------------------------------------------------
//! \brief GML2CacheExperiment Cache experiment
//!
//! This L2 Cache experiment aclwmulates statistics for all operations on the
//! specified slice, for all GM GPUs.
class GML2CacheExperiment : public L2CacheExperiment
{
public:
    // L2 BusSignalIndex values for the Active/Hit/Miss signals
    struct L2Bsi
    {
        UINT32 ltcActive = 0;
        UINT32 ltcHit    = 0;
        UINT32 ltcMiss   = 0;
        UINT32 pmTrigger = 0;
    };

    GML2CacheExperiment
    (
        UINT32 fbpNum,
        UINT32 l2Slice,
        GpuPerfmon* pPerfmon
    );
    virtual ~GML2CacheExperiment() { }
    virtual UINT32 CalcPmIdx();
    virtual RC Start();
    virtual RC End();
    L2Bsi m_L2Bsi;
    UINT32 m_SLCG = 0;          // original value of SLCG

private:
    UINT32 GetFbpChipletSeparation();
};

//------------------------------------------------------------------------------
//! \brief GMLit2L2CacheExperiment Cache experiment
//!
//! This L2 Cache experiment aclwmulates statistics for all operations on the
//! specified slice, for all GMLit1 GPUs (GM107, GM108). It's only purpose is
//! to define the busIndex values during construction.
class GMLit1L2CacheExperiment : public GML2CacheExperiment
{
public:
    GMLit1L2CacheExperiment
    (
        UINT32 fbpNum,
        UINT32 l2Slice,
        GpuPerfmon* pPerfmon
    );
    virtual ~GMLit1L2CacheExperiment() { }
};

//------------------------------------------------------------------------------
//! \brief GMLit2L2CacheExperiment Cache experiment
//!
//! This L2 Cache experiment aclwmulates statistics for all operations on the
//! specified slice, for all GMLit1 GPUs (GM104, GP100). It's only purpose is
//! to define the busIndex values during construction.
//! Note: GP100 did us this same litter, its not a typo.
class GMLit2L2CacheExperiment : public GML2CacheExperiment
{
public:
    GMLit2L2CacheExperiment
    (
        UINT32 fbpNum,
        UINT32 l2Slice,
        GpuPerfmon* pPerfmon
    );
    virtual ~GMLit2L2CacheExperiment() { }
};
#endif
